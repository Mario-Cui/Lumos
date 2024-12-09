#include "Precompiled.h"
#include "VKIMGUIRenderer.h"
#include <imgui/imgui.h>

#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#include <imgui/backends/imgui_impl_vulkan.h>

#include "VKDevice.h"
#include "VKCommandBuffer.h"
#include "VKRenderer.h"
#include "VKRenderPass.h"
#include "VKTexture.h"
#include "Graphics/RHI/GPUProfile.h"
#include "Core/Application.h"

static ImGui_ImplVulkanH_Window g_WindowData;
static VkAllocationCallbacks* g_Allocator   = nullptr;
static VkDescriptorPool g_DescriptorPool[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };

static void check_vk_result(VkResult err)
{
    if(err == 0)
        return;
    printf("VkResult %d\n", err);
    if(err < 0)
        abort();
}

namespace Lumos
{
    namespace Graphics
    {
        VKIMGUIRenderer::VKIMGUIRenderer(uint32_t width, uint32_t height, bool clearScreen)
            : m_Framebuffers {}
            , m_Renderpass(nullptr)
            , m_FontTexture(nullptr)
        {
            LUMOS_PROFILE_FUNCTION();

            m_WindowHandle = nullptr;
            m_Width        = width;
            m_Height       = height;
            m_ClearScreen  = clearScreen;
        }

        VKIMGUIRenderer::~VKIMGUIRenderer()
        {
            LUMOS_PROFILE_FUNCTION();

            for(int i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
            {
                delete m_Framebuffers[i];
            }

            delete m_Renderpass;
            delete m_FontTexture;

            DeletionQueue& deletionQueue = VKRenderer::GetCurrentDeletionQueue();

            for(int i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
            {
                ImGui_ImplVulkanH_Frame* fd = &g_WindowData.Frames[i];
                auto fence                  = fd->Fence;
                auto alloc                  = g_Allocator;
                auto commandPool            = fd->CommandPool;

                deletionQueue.PushFunction([fence, commandPool, alloc]
                                           {
                        vkDestroyFence(VKDevice::Get().GetDevice(), fence, alloc);
                        vkDestroyCommandPool(VKDevice::Get().GetDevice(), commandPool, alloc); });

                auto descriptorPool = g_DescriptorPool[i];

                deletionQueue.PushFunction([descriptorPool]
                                           { vkDestroyDescriptorPool(VKDevice::Get().GetDevice(), descriptorPool, nullptr); });
            }
            deletionQueue.PushFunction([]
                                       { ImGui_ImplVulkan_Shutdown(); });
        }

        void VKIMGUIRenderer::SetupVulkanWindowData(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
        {
            LUMOS_PROFILE_FUNCTION();

            // Create Descriptor Pool
            for(int i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
            {
                VkDescriptorPoolSize pool_sizes[] = {
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }
                };
                VkDescriptorPoolCreateInfo pool_info = {};
                pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                pool_info.maxSets                    = 100 * IM_ARRAYSIZE(pool_sizes);
                pool_info.poolSizeCount              = (uint32_t)IM_ARRAYSIZE(pool_sizes);
                pool_info.pPoolSizes                 = pool_sizes;
                VkResult err                         = vkCreateDescriptorPool(VKDevice::Get().GetDevice(), &pool_info, g_Allocator, &g_DescriptorPool[i]);
                check_vk_result(err);
            }

            wd->Surface     = surface;
            wd->ClearEnable = m_ClearScreen;

            auto swapChain = static_cast<VKSwapChain*>(VKRenderer::GetMainSwapChain());
            wd->Swapchain  = swapChain->GetSwapChain();
            wd->Width      = width;
            wd->Height     = height;

            wd->ImageCount = static_cast<uint32_t>(swapChain->GetSwapChainBufferCount());

            TextureType textureTypes[1] = { TextureType::COLOUR };

            Texture* textures[1] = { swapChain->GetImage(0) };

            Graphics::RenderPassDesc renderPassDesc;
            renderPassDesc.attachmentCount = 1;
            renderPassDesc.attachmentTypes = textureTypes;
            renderPassDesc.clear           = m_ClearScreen;
            renderPassDesc.attachments     = textures;
            renderPassDesc.swapchainTarget = true;
            renderPassDesc.DebugName       = "ImGui";
            m_Renderpass                   = new VKRenderPass(renderPassDesc);
            wd->RenderPass                 = m_Renderpass->GetHandle();

            wd->Frames = (ImGui_ImplVulkanH_Frame*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_Frame) * wd->ImageCount);
            // wd->FrameSemaphores = (ImGui_ImplVulkanH_FrameSemaphores*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_FrameSemaphores) * wd->ImageCount);
            memset(wd->Frames, 0, sizeof(wd->Frames[0]) * wd->ImageCount);
            // memset(wd->FrameSemaphores, 0, sizeof(wd->FrameSemaphores[0]) * wd->ImageCount);

            // Create The Image Views
            {
                for(uint32_t i = 0; i < wd->ImageCount; i++)
                {
                    auto scBuffer                = (VKTexture2D*)swapChain->GetImage(i);
                    wd->Frames[i].Backbuffer     = scBuffer->GetImage();
                    wd->Frames[i].BackbufferView = scBuffer->GetImageView();
                }
            }

            TextureType attachmentTypes[1];
            attachmentTypes[0] = TextureType::COLOUR;

            Texture* attachments[1];
            FramebufferDesc frameBufferDesc {};
            frameBufferDesc.width           = wd->Width;
            frameBufferDesc.height          = wd->Height;
            frameBufferDesc.attachmentCount = 1;
            frameBufferDesc.renderPass      = m_Renderpass;
            frameBufferDesc.attachmentTypes = attachmentTypes;
            frameBufferDesc.screenFBO       = true;

            for(uint32_t i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
            {
                attachments[0]              = Renderer::GetMainSwapChain()->GetImage(i);
                frameBufferDesc.attachments = attachments;

                m_Framebuffers[i]         = new VKFramebuffer(frameBufferDesc);
                wd->Frames[i].Framebuffer = m_Framebuffers[i]->GetFramebuffer();
            }
        }

        void VKIMGUIRenderer::Init()
        {
            LUMOS_PROFILE_FUNCTION();
            int w, h;
            w                            = (int)m_Width;
            h                            = (int)m_Height;
            ImGui_ImplVulkanH_Window* wd = &g_WindowData;
            VkSurfaceKHR surface         = VKRenderer::GetMainSwapChain()->GetSurface();
            SetupVulkanWindowData(wd, surface, w, h);

            // Setup Vulkan binding
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance                  = VKContext::GetVKInstance();
            init_info.PhysicalDevice            = VKDevice::Get().GetGPU();
            init_info.Device                    = VKDevice::Get().GetDevice();
            init_info.QueueFamily               = VKDevice::Get().GetPhysicalDevice()->GetGraphicsQueueFamilyIndex();
            init_info.Queue                     = VKDevice::Get().GetGraphicsQueue();
            init_info.PipelineCache             = VKDevice::Get().GetPipelineCache();
            init_info.DescriptorPools           = g_DescriptorPool;
            init_info.Allocator                 = g_Allocator;
            init_info.CheckVkResultFn           = NULL;
            init_info.MinImageCount             = 2;
            init_info.ImageCount                = (uint32_t)Renderer::GetMainSwapChain()->GetSwapChainBufferCount();
            ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

            RebuildFontTexture();
        }

        void VKIMGUIRenderer::NewFrame()
        {
            u32 currentImageIndex = Renderer::GetMainSwapChain()->GetCurrentBufferIndex();

            vkResetDescriptorPool(VKDevice::Get().GetDevice(), g_DescriptorPool[currentImageIndex], 0);
            m_CurrentFrameTextures = { Application::Get().GetFrameArena() };
            m_CurrentFrameTextures.Reserve(1024);

            ImGui::GetIO().Fonts->TexID = (ImTextureID)AddTexture(m_FontTexture, TextureType::COLOUR, 0, 0);
        }

        void VKIMGUIRenderer::FrameRender(ImGui_ImplVulkanH_Window* wd)
        {
            LUMOS_PROFILE_FUNCTION();

            LUMOS_PROFILE_GPU("ImGui Pass");

            wd->FrameIndex            = VKRenderer::GetMainSwapChain()->GetCurrentImageIndex();
            auto currentCommandBuffer = (VKCommandBuffer*)Renderer::GetMainSwapChain()->GetCurrentCommandBuffer();
            auto swapChain            = static_cast<VKSwapChain*>(VKRenderer::GetMainSwapChain());

            if(wd->Swapchain != swapChain->GetSwapChain())
                OnResize(wd->Width, wd->Height);

            {
                auto draw_data = ImGui::GetDrawData();
                for(auto texture : m_CurrentFrameTextures)
                {
                    if(!texture)
                        continue;

                    if(texture->GetType() == TextureType::COLOUR)
                    {
                        auto tex = (VKTexture2D*)texture;
                        tex->TransitionImage(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, currentCommandBuffer);
                    }
                    else if(texture->GetType() == TextureType::DEPTH)
                    {
                        auto textureDepth = (VKTextureDepth*)texture;
                        textureDepth->TransitionImage(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, currentCommandBuffer);
                    }
                    else if(texture->GetType() == TextureType::DEPTHARRAY)
                    {
                        auto textureDepth = (VKTextureDepthArray*)texture;
                        textureDepth->TransitionImage(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, currentCommandBuffer);
                    }
                }
            }

            float clearColour[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
            m_Renderpass->BeginRenderPass(currentCommandBuffer, clearColour, m_Framebuffers[wd->FrameIndex], Graphics::SubPassContents::INLINE, wd->Width, wd->Height);

            {
                LUMOS_PROFILE_SCOPE("ImGui Vulkan RenderDrawData");
                // Record Imgui Draw Data and draw funcs into command buffer
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCommandBuffer->GetHandle(), VK_NULL_HANDLE, wd->FrameIndex, VKRenderer::GetMainSwapChain()->GetCurrentBufferIndex());
            }
            m_Renderpass->EndRenderPass(currentCommandBuffer);
        }

        void VKIMGUIRenderer::Render(Lumos::Graphics::CommandBuffer* commandBuffer)
        {
            LUMOS_PROFILE_FUNCTION();

            ImGui::Render();
            FrameRender(&g_WindowData);
        }

        void VKIMGUIRenderer::OnResize(uint32_t width, uint32_t height)
        {
            LUMOS_PROFILE_FUNCTION();

            auto* wd       = &g_WindowData;
            auto swapChain = static_cast<VKSwapChain*>(VKRenderer::GetMainSwapChain());
            wd->Swapchain  = swapChain->GetSwapChain();
            for(uint32_t i = 0; i < wd->ImageCount; i++)
            {
                auto scBuffer                = (VKTexture2D*)swapChain->GetImage(i);
                wd->Frames[i].Backbuffer     = scBuffer->GetImage();
                wd->Frames[i].BackbufferView = scBuffer->GetImageView();
            }

            wd->Width  = width;
            wd->Height = height;

            for(uint32_t i = 0; i < wd->ImageCount; i++)
            {
                delete m_Framebuffers[i];
            }
            // Create Framebuffer
            TextureType attachmentTypes[1];
            attachmentTypes[0] = TextureType::COLOUR;

            Texture* attachments[1];
            FramebufferDesc frameBufferDesc {};
            frameBufferDesc.width           = wd->Width;
            frameBufferDesc.height          = wd->Height;
            frameBufferDesc.attachmentCount = 1;
            frameBufferDesc.renderPass      = m_Renderpass;
            frameBufferDesc.attachmentTypes = attachmentTypes;
            frameBufferDesc.screenFBO       = true;

            for(uint32_t i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
            {
                attachments[0]              = Renderer::GetMainSwapChain()->GetImage(i);
                frameBufferDesc.attachments = attachments;

                m_Framebuffers[i]         = new VKFramebuffer(frameBufferDesc);
                wd->Frames[i].Framebuffer = m_Framebuffers[i]->GetFramebuffer();
            }
        }

        void VKIMGUIRenderer::Clear()
        {
            // ImGui_ImplVulkan_ClearDescriptors();
        }

        void VKIMGUIRenderer::MakeDefault()
        {
            CreateFunc = CreateFuncVulkan;
        }

        IMGUIRenderer* VKIMGUIRenderer::CreateFuncVulkan(uint32_t width, uint32_t height, bool clearScreen)
        {
            return new VKIMGUIRenderer(width, height, clearScreen);
        }

        void VKIMGUIRenderer::RebuildFontTexture()
        {
            LUMOS_PROFILE_FUNCTION();

            if(m_FontTexture)
                delete m_FontTexture;

            // Upload Fonts
            {
                ImGuiIO& io = ImGui::GetIO();

                unsigned char* pixels;
                int width, height;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

                auto desc  = TextureDesc(TextureFilter::NEAREST, TextureFilter::NEAREST, TextureWrap::REPEAT);
                desc.flags = TextureFlags::Texture_Sampled;

                m_FontTexture   = new VKTexture2D(width, height, pixels, desc);
                io.Fonts->TexID = (ImTextureID)AddTexture(m_FontTexture, TextureType::COLOUR, 0, 0);
                m_CurrentFrameTextures.PushBack(m_FontTexture);
            }
        }

        ImTextureID VKIMGUIRenderer::AddTexture(Texture* texture, TextureType type, uint32_t level, uint32_t mip)
        {
            u32 currentImageIndex = Renderer::GetMainSwapChain()->GetCurrentBufferIndex();

            // Layout is transitioned to these before rendering
            m_CurrentFrameTextures.PushBack(texture);
            if(type == TextureType::COLOUR)
            {
                auto tex = (VKTexture2D*)texture;
                return ImGui_ImplVulkan_AddTexture(tex->GetSampler(), tex->GetMipImageView(mip), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, g_DescriptorPool[currentImageIndex]);
            }
            else if(type == TextureType::DEPTH)
            {
                auto depthTex = (VKTextureDepth*)texture;
                return ImGui_ImplVulkan_AddTexture(depthTex->GetSampler(), depthTex->GetImageView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, g_DescriptorPool[currentImageIndex]);
            }
            else if(type == TextureType::DEPTHARRAY)
            {
                auto depthTex = (VKTextureDepthArray*)texture;
                return ImGui_ImplVulkan_AddTexture(depthTex->GetSampler(), depthTex->GetImageView(level), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, g_DescriptorPool[currentImageIndex]);
            }

            ASSERT("Unsupported Texture Type with ImGui");
            return nullptr;
        }
    }
}
