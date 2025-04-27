#include "Precompiled.h"
#include "VKPipeline.h"
#include "VKDevice.h"
#include "VKCommandBuffer.h"
#include "VKRenderPass.h"
#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKShader.h"
#include "VKTexture.h"
#include "VKUtilities.h"
#include "Graphics/RHI/DescriptorSet.h"
#include "VKInitialisers.h"
#include "Core/Engine.h"

namespace Lumos
{

    namespace Graphics
    {
        VKPipeline::VKPipeline(const PipelineDesc& pipelineDesc)
        {
            Init(pipelineDesc);
        }

        VKPipeline::~VKPipeline()
        {
            LUMOS_PROFILE_FUNCTION_LOW();
            DeletionQueue& deletionQueue = VKRenderer::GetCurrentDeletionQueue();

            auto pipeline = m_Pipeline;

            deletionQueue.PushFunction([pipeline]
                                       { vkDestroyPipeline(VKDevice::Get().GetDevice(), pipeline, VK_NULL_HANDLE); });
        }

        bool VKPipeline::Init(const PipelineDesc& pipelineDesc)
        {
            LUMOS_PROFILE_FUNCTION_LOW();
            m_Description    = pipelineDesc;
            m_Shader         = m_Description.shader;
            m_PipelineLayout = m_Shader.As<VKShader>()->GetPipelineLayout();

            TransitionAttachments();

            // Pipeline
            TDArray<VkDynamicState> dynamicStateDescriptors;
            VkPipelineDynamicStateCreateInfo dynamicStateCI {};
            dynamicStateCI.sType          = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCI.pNext          = NULL;
            dynamicStateCI.pDynamicStates = dynamicStateDescriptors.Data();

            TDArray<VkVertexInputAttributeDescription> vertexInputDescription;

            m_Compute = m_Shader.As<VKShader>()->IsCompute();

            if(m_Compute)
            {
                VkComputePipelineCreateInfo pipelineInfo = {};
                pipelineInfo.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                pipelineInfo.layout                      = m_PipelineLayout;
                pipelineInfo.stage                       = m_Shader.As<VKShader>()->GetShaderStages()[0];
                VK_CHECK_RESULT(vkCreateComputePipelines(VKDevice::Get().GetDevice(), nullptr, 1, &pipelineInfo, nullptr, &m_Pipeline));
            }
            else
            {
                CreateFramebuffers();

                uint32_t stride = m_Shader.As<VKShader>()->GetVertexInputStride();

                // Vertex layout
                VkVertexInputBindingDescription vertexBindingDescription;

                if(stride > 0)
                {
                    vertexBindingDescription.binding   = 0;
                    vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                    vertexBindingDescription.stride    = m_Shader.As<VKShader>()->GetVertexInputStride();
                }

                const TDArray<VkVertexInputAttributeDescription>& vertexInputAttributeDescription = m_Shader.As<VKShader>()->GetVertexInputAttributeDescription();

                VkPipelineVertexInputStateCreateInfo vi {};
                vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                vi.pNext                           = NULL;
                vi.vertexBindingDescriptionCount   = stride > 0 ? 1 : 0;
                vi.pVertexBindingDescriptions      = stride > 0 ? &vertexBindingDescription : nullptr;
                vi.vertexAttributeDescriptionCount = stride > 0 ? uint32_t(vertexInputAttributeDescription.Size()) : 0;
                vi.pVertexAttributeDescriptions    = stride > 0 ? vertexInputAttributeDescription.Data() : nullptr;

                VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI {};
                inputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                inputAssemblyCI.pNext                  = NULL;
                inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
                inputAssemblyCI.topology               = VKUtilities::DrawTypeToVk(pipelineDesc.drawType);

                VkPipelineRasterizationStateCreateInfo rs {};
                rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
                rs.polygonMode             = VKUtilities::PolygonModeToVk(pipelineDesc.polygonMode);
                rs.cullMode                = VKUtilities::CullModeToVK(pipelineDesc.cullMode);
                rs.frontFace               = pipelineDesc.swapchainTarget ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
                rs.depthClampEnable        = VK_TRUE;
                rs.rasterizerDiscardEnable = VK_FALSE;
                rs.depthBiasEnable         = (pipelineDesc.depthBiasEnabled ? VK_TRUE : VK_FALSE);
                rs.depthBiasConstantFactor = pipelineDesc.depthBiasConstantFactor;
                rs.depthBiasClamp          = 0;
                rs.depthBiasSlopeFactor    = pipelineDesc.depthBiasSlopeFactor;

                if(Renderer::GetCapabilities().WideLines)
                    rs.lineWidth = pipelineDesc.lineWidth;
                else
                    rs.lineWidth = 1.0f;
                rs.pNext = NULL;

                VkPipelineColorBlendStateCreateInfo cb {};
                cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
                cb.pNext = NULL;
                cb.flags = 0;

                TDArray<VkPipelineColorBlendAttachmentState> blendAttachState;
                blendAttachState.Resize(m_RenderPass.As<VKRenderPass>()->GetColourAttachmentCount());

                for(unsigned int i = 0; i < blendAttachState.Size(); i++)
                {
                    blendAttachState[i]                     = VkPipelineColorBlendAttachmentState();
                    blendAttachState[i].colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                    blendAttachState[i].alphaBlendOp        = VK_BLEND_OP_ADD;
                    blendAttachState[i].colorBlendOp        = VK_BLEND_OP_ADD;
                    blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

                    if(pipelineDesc.transparencyEnabled)
                    {
                        blendAttachState[i].blendEnable         = VK_TRUE;
                        blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                        blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

                        if(pipelineDesc.blendMode == BlendMode::SrcAlphaOneMinusSrcAlpha)
                        {
                            blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                            blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                            blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                            blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                        }
                        else if(pipelineDesc.blendMode == BlendMode::SrcAlphaOne)
                        {
                            blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                            blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                            blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                            blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                        }
                        else if(pipelineDesc.blendMode == BlendMode::OneMinusSrcAlpha)
                        {
                            blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                            blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                            blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                            blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                        }
                        else if(pipelineDesc.blendMode == BlendMode::ZeroSrcColor)
                        {
                            blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                            blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
                        }
                        else if(pipelineDesc.blendMode == BlendMode::OneZero)
                        {
                            blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                            blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                        }
                        else
                        {
                            blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                            blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                        }
                    }
                    else
                    {
                        blendAttachState[i].blendEnable         = VK_FALSE;
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                        blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                        blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                    }
                }

                cb.attachmentCount   = static_cast<uint32_t>(blendAttachState.Size());
                cb.pAttachments      = blendAttachState.Data();
                cb.logicOpEnable     = VK_FALSE;
                cb.logicOp           = VK_LOGIC_OP_NO_OP;
                cb.blendConstants[0] = 1.0f;
                cb.blendConstants[1] = 1.0f;
                cb.blendConstants[2] = 1.0f;
                cb.blendConstants[3] = 1.0f;

                VkPipelineViewportStateCreateInfo vp {};
                vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
                vp.pNext         = NULL;
                vp.viewportCount = 1;
                vp.scissorCount  = 1;
                vp.pScissors     = NULL;
                vp.pViewports    = NULL;
                dynamicStateDescriptors.PushBack(VK_DYNAMIC_STATE_VIEWPORT);
                dynamicStateDescriptors.PushBack(VK_DYNAMIC_STATE_SCISSOR);

                if(Renderer::GetCapabilities().WideLines && pipelineDesc.polygonMode == PolygonMode::LINE) // || pipelineDesc.polygonMode == PolygonMode::LINESTRIP)
                    dynamicStateDescriptors.PushBack(VK_DYNAMIC_STATE_LINE_WIDTH);

                if(pipelineDesc.depthBiasEnabled)
                {
                    dynamicStateDescriptors.PushBack(VK_DYNAMIC_STATE_DEPTH_BIAS);
                    m_DepthBiasConstant = pipelineDesc.depthBiasConstantFactor;
                    m_DepthBiasSlope    = pipelineDesc.depthBiasSlopeFactor;
                    m_DepthBiasEnabled  = true;
                }
                else
                {
                    m_DepthBiasEnabled = false;
                }

                VkPipelineDepthStencilStateCreateInfo ds {};
                ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                ds.pNext                 = NULL;
                ds.depthTestEnable       = pipelineDesc.DepthTest ? VK_TRUE : VK_FALSE;
                ds.depthWriteEnable      = pipelineDesc.DepthWrite ? VK_TRUE : VK_FALSE;
                ds.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
                ds.depthBoundsTestEnable = VK_FALSE;
                ds.stencilTestEnable     = VK_FALSE;
                ds.back.failOp           = VK_STENCIL_OP_KEEP;
                ds.back.passOp           = VK_STENCIL_OP_KEEP;
                ds.back.compareOp        = VK_COMPARE_OP_ALWAYS;
                ds.back.compareMask      = 0;
                ds.back.reference        = 0;
                ds.back.depthFailOp      = VK_STENCIL_OP_KEEP;
                ds.back.writeMask        = 0;
                ds.minDepthBounds        = 0;
                ds.maxDepthBounds        = 0;
                ds.front                 = ds.back;

                VkPipelineMultisampleStateCreateInfo ms {};
                ms.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                ms.pNext                 = NULL;
                ms.pSampleMask           = NULL;
                ms.rasterizationSamples  = m_Description.samples > 1 ? (VkSampleCountFlagBits)m_Description.samples : VK_SAMPLE_COUNT_1_BIT;
                ms.sampleShadingEnable   = VK_FALSE;
                ms.alphaToCoverageEnable = VK_FALSE;
                ms.alphaToOneEnable      = VK_FALSE;
                ms.minSampleShading      = 0.0;

                dynamicStateCI.dynamicStateCount = uint32_t(dynamicStateDescriptors.Size());
                dynamicStateCI.pDynamicStates    = dynamicStateDescriptors.Data();

                auto vkshader = pipelineDesc.shader.As<VKShader>();
                VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo {};
                graphicsPipelineCreateInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                graphicsPipelineCreateInfo.pNext               = NULL;
                graphicsPipelineCreateInfo.layout              = m_PipelineLayout;
                graphicsPipelineCreateInfo.basePipelineHandle  = VK_NULL_HANDLE;
                graphicsPipelineCreateInfo.basePipelineIndex   = -1;
                graphicsPipelineCreateInfo.pVertexInputState   = &vi;
                graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCI;
                graphicsPipelineCreateInfo.pRasterizationState = &rs;
                graphicsPipelineCreateInfo.pColorBlendState    = &cb;
                graphicsPipelineCreateInfo.pTessellationState  = VK_NULL_HANDLE;
                graphicsPipelineCreateInfo.pMultisampleState   = &ms;
                graphicsPipelineCreateInfo.pDynamicState       = &dynamicStateCI;
                graphicsPipelineCreateInfo.pViewportState      = &vp;
                graphicsPipelineCreateInfo.pDepthStencilState  = &ds;
                graphicsPipelineCreateInfo.pStages             = vkshader->GetShaderStages();
                graphicsPipelineCreateInfo.stageCount          = vkshader->GetStageCount();
                graphicsPipelineCreateInfo.renderPass          = m_RenderPass.As<VKRenderPass>()->GetHandle();
                graphicsPipelineCreateInfo.subpass             = 0;

                VK_CHECK_RESULT(vkCreateGraphicsPipelines(VKDevice::Get().GetDevice(), VKDevice::Get().GetPipelineCache(), 1, &graphicsPipelineCreateInfo, VK_NULL_HANDLE, &m_Pipeline));
            }

            if(!pipelineDesc.DebugName)
                VKUtilities::SetDebugUtilsObjectName(VKDevice::Get().GetDevice(), VK_OBJECT_TYPE_PIPELINE, pipelineDesc.DebugName, m_Pipeline);

            return true;
        }

        void VKPipeline::Bind(CommandBuffer* commandBuffer, uint32_t layer)
        {
            LUMOS_PROFILE_FUNCTION_LOW();
            Engine::Get().Statistics().BoundPipelines++;

            VKFramebuffer* framebuffer;

            if(!m_Compute)
            {
                TransitionAttachments();

                if(m_Description.swapchainTarget)
                {
                    framebuffer = m_Framebuffers[Renderer::GetMainSwapChain()->GetCurrentImageIndex()];
                }
                else if(m_Description.depthArrayTarget || m_Description.cubeMapTarget)
                {
                    framebuffer = m_Framebuffers[layer];
                }
                else
                {
                    framebuffer = m_Framebuffers[0];
                }

                ((VKCommandBuffer*)commandBuffer)->BeginRenderPass(m_RenderPass, m_Description.clearColour, framebuffer, GetWidth(), GetHeight());
                // m_RenderPass->BeginRenderPass(commandBuffer, m_Description.clearColour, framebuffer, Graphics::INLINE, GetWidth(), GetHeight());
            }
            else
            {
                // LWARN("TODO: not correct width and height");
                // uint32_t width  = 1000;
                // uint32_t height = 1000;
                // commandBuffer->UpdateViewport(width, height, false);
            }

            vkCmdBindPipeline(static_cast<VKCommandBuffer*>(commandBuffer)->GetHandle(), m_Compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

            // Bug in moltenVK. Needs to happen after pipeline bound for now.
            if(m_DepthBiasEnabled)
                vkCmdSetDepthBias(static_cast<VKCommandBuffer*>(commandBuffer)->GetHandle(),
                                  m_DepthBiasConstant,
                                  0.0f,
                                  m_DepthBiasSlope);
        }

        void VKPipeline::CreateFramebuffers()
        {
            LUMOS_PROFILE_FUNCTION_LOW();

            ArenaTemp temp = ScratchBegin(0, 0);
            TDArray<TextureType> attachmentTypes(temp.arena);
            TDArray<Texture*> attachments(temp.arena);

            if(m_Description.swapchainTarget)
            {
                attachmentTypes.PushBack(TextureType::COLOUR);
                attachments.PushBack(Renderer::GetMainSwapChain()->GetImage(0));
            }
            else
            {
                for(auto texture : m_Description.colourTargets)
                {
                    if(texture)
                    {
                        attachmentTypes.PushBack(texture->GetType());
                        attachments.PushBack(texture);
                    }
                }
            }

            if(m_Description.depthTarget)
            {
                attachmentTypes.PushBack(m_Description.depthTarget->GetType());
                attachments.PushBack(m_Description.depthTarget);
            }

            if(m_Description.depthArrayTarget)
            {
                attachmentTypes.PushBack(m_Description.depthArrayTarget->GetType());
                attachments.PushBack(m_Description.depthArrayTarget);
            }

            if(m_Description.cubeMapTarget)
            {
                attachmentTypes.PushBack(m_Description.cubeMapTarget->GetType());
                attachments.PushBack(m_Description.cubeMapTarget);
            }

            if(m_Description.resolveTexture)
            {
                attachmentTypes.PushBack(TextureType::COLOUR);
                attachments.PushBack(m_Description.resolveTexture);
            }

            Graphics::RenderPassDesc renderPassDesc = {};
            renderPassDesc.attachmentCount          = uint32_t(attachmentTypes.Size()) - (m_Description.resolveTexture ? 1 : 0);
            renderPassDesc.attachmentTypes          = attachmentTypes.Data();
            renderPassDesc.attachments              = attachments.Data();
            renderPassDesc.swapchainTarget          = m_Description.swapchainTarget;
            renderPassDesc.clear                    = m_Description.clearTargets;
            renderPassDesc.cubeMapIndex             = m_Description.cubeMapIndex;
            renderPassDesc.mipIndex                 = m_Description.mipIndex;
            renderPassDesc.resolveTexture           = m_Description.resolveTexture;
            if(m_Description.DebugName != NULL)
                renderPassDesc.DebugName = m_Description.DebugName;
            renderPassDesc.samples = m_Description.samples;

            m_RenderPass = Graphics::RenderPass::Get(renderPassDesc);

            FramebufferDesc frameBufferDesc = {};
            frameBufferDesc.width           = GetWidth();
            frameBufferDesc.height          = GetHeight();
            frameBufferDesc.attachmentCount = uint32_t(attachments.Size());
            frameBufferDesc.renderPass      = m_RenderPass.get();
            frameBufferDesc.attachmentTypes = attachmentTypes.Data();
            frameBufferDesc.samples         = m_Description.samples;

            if(m_Description.swapchainTarget)
            {
                for(uint32_t i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
                {
                    attachments[0]              = Renderer::GetMainSwapChain()->GetImage(i);
                    frameBufferDesc.screenFBO   = true;
                    frameBufferDesc.attachments = attachments.Data();

                    m_Framebuffers.EmplaceBack(Framebuffer::Get(frameBufferDesc));
                }
            }
            else if(m_Description.depthArrayTarget)
            {
                for(uint32_t i = 0; i < ((VKTextureDepthArray*)m_Description.depthArrayTarget)->GetCount(); ++i)
                {
                    attachments[0]              = m_Description.depthArrayTarget;
                    frameBufferDesc.layer       = i;
                    frameBufferDesc.screenFBO   = false;
                    frameBufferDesc.attachments = attachments.Data();

                    m_Framebuffers.EmplaceBack(Framebuffer::Get(frameBufferDesc));
                }
            }
            else if(m_Description.cubeMapTarget)
            {
                for(uint32_t i = 0; i < 6; ++i)
                {
                    frameBufferDesc.layer     = i;
                    frameBufferDesc.screenFBO = false;
                    frameBufferDesc.mipIndex  = m_Description.mipIndex;

                    attachments[0]              = m_Description.cubeMapTarget;
                    frameBufferDesc.attachments = attachments.Data();

                    m_Framebuffers.EmplaceBack(Framebuffer::Get(frameBufferDesc));
                }
            }
            else
            {
                frameBufferDesc.attachments = attachments.Data();
                frameBufferDesc.screenFBO   = false;
                frameBufferDesc.mipIndex    = m_Description.mipIndex;
                m_Framebuffers.EmplaceBack(Framebuffer::Get(frameBufferDesc));
            }

            ScratchEnd(temp);
        }

        void VKPipeline::End(CommandBuffer* commandBuffer)
        {
            LUMOS_PROFILE_FUNCTION();
            if(!m_Compute)
            {
                // m_RenderPass->EndRenderPass(commandBuffer);
            }
        }

        void VKPipeline::ClearRenderTargets(CommandBuffer* commandBuffer)
        {
            LUMOS_PROFILE_FUNCTION_LOW();
            if(m_Description.swapchainTarget)
            {
                for(uint32_t i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
                {
                    Renderer::GetRenderer()->ClearRenderTarget(Renderer::GetMainSwapChain()->GetImage(i), commandBuffer);
                }
            }

            if(m_Description.depthArrayTarget)
            {
                Renderer::GetRenderer()->ClearRenderTarget(m_Description.depthArrayTarget, commandBuffer);
            }

            if(m_Description.depthTarget)
            {
                Renderer::GetRenderer()->ClearRenderTarget(m_Description.depthTarget, commandBuffer);
            }

            {
                for(auto texture : m_Description.colourTargets)
                {
                    if(texture != nullptr)
                    {
                        Renderer::GetRenderer()->ClearRenderTarget(texture, commandBuffer);
                    }
                }
            }
        }

        void VKPipeline::TransitionAttachments()
        {
            LUMOS_PROFILE_FUNCTION_LOW();
            auto commandBuffer = Renderer::GetMainSwapChain()->GetCurrentCommandBuffer();

            if(m_Description.swapchainTarget)
            {
                for(uint32_t i = 0; i < Renderer::GetMainSwapChain()->GetSwapChainBufferCount(); i++)
                {
                    ((VKTexture2D*)Renderer::GetMainSwapChain()->GetImage(i))->TransitionImage(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, (VKCommandBuffer*)commandBuffer);
                }
            }

            if(m_Description.depthArrayTarget)
            {
                ((VKTextureDepthArray*)m_Description.depthArrayTarget)->TransitionImage(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VKCommandBuffer*)commandBuffer);
            }

            if(m_Description.depthTarget)
            {
                ((VKTextureDepth*)m_Description.depthTarget)->TransitionImage(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, (VKCommandBuffer*)commandBuffer);
            }

            for(auto texture : m_Description.colourTargets)
            {
                if(texture != nullptr)
                {
                    ((VKTexture2D*)texture)->TransitionImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VKCommandBuffer*)commandBuffer);
                }
            }

            if(m_Description.resolveTexture)
            {
                ((VKTexture2D*)m_Description.resolveTexture)->TransitionImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VKCommandBuffer*)commandBuffer);
            }
        }

        void VKPipeline::MakeDefault()
        {
            CreateFunc = CreateFuncVulkan;
        }

        Pipeline* VKPipeline::CreateFuncVulkan(const PipelineDesc& pipelineDesc)
        {
            return new VKPipeline(pipelineDesc);
        }
    }
}
