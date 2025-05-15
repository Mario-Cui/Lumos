#pragma once
#include "Utilities/TSingleton.h"
#include "VK.h"
#include "VKCommandPool.h"
#include "Graphics/RHI/RHIDefinitions.h"
#include "Core/DataStructures/Set.h"
#include "Core/DataStructures/TDArray.h"

static const uint32_t SMALL_ALLOCATION_MAX_SIZE = 4096;

namespace tracy
{
    class VkCtx;
}

namespace Lumos
{
    namespace Graphics
    {
        class VKPhysicalDevice
        {
        public:
            VKPhysicalDevice();
            ~VKPhysicalDevice();

            struct PhysicalDeviceInfo
            {
                std::string GetVendorName();
                std::string DecodeDriverVersion(const uint32_t version);

                uint32_t Memory;
                uint32_t VendorID;
                std::string Driver;
                std::string APIVersion;
                std::string Vendor;
                std::string Name;
                PhysicalDeviceType Type;
                VkPhysicalDevice Handle;
            };

            bool IsExtensionSupported(String8 extensionName) const;
            uint32_t GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

            PhysicalDeviceInfo GetInfo(VkPhysicalDevice device);

            VkPhysicalDevice GetHandle() const
            {
                return m_Handle;
            }

            int32_t GetGraphicsQueueFamilyIndex()
            {
                return m_QueueFamilyIndices.Graphics;
            }

            VkPhysicalDeviceProperties GetProperties() const
            {
                return m_PhysicalDeviceProperties;
            }

            VkPhysicalDeviceMemoryProperties GetMemoryProperties() const
            {
                return m_MemoryProperties;
            }

        private:
            struct QueueFamilyIndices
            {
                int32_t Graphics = -1;
                int32_t Compute  = -1;
                int32_t Transfer = -1;
            };
            QueueFamilyIndices GetQueueFamilyIndices(int queueFlags);

            uint32_t GetGPUCount() const
            {
                return m_GPUCount;
            }

        private:
            QueueFamilyIndices m_QueueFamilyIndices;

            TDArray<VkQueueFamilyProperties> m_QueueFamilyProperties;
            TDArray<VkDeviceQueueCreateInfo> m_QueueCreateInfos;
            HashSet(uint64_t) m_SupportedExtensions;

            VkPhysicalDevice m_Handle;
            VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
            VkPhysicalDeviceMemoryProperties m_MemoryProperties;

            uint32_t m_GPUCount = 0;
            PhysicalDeviceInfo m_DeviceInfo;

            friend class VKDevice;
        };

        class VKDevice : public ThreadSafeSingleton<VKDevice>
        {
            friend class ThreadSafeSingleton<VKDevice>;

        public:
            VKDevice();
            ~VKDevice();

            bool Init();
            void CreatePipelineCache();
            void CreateTracyContext();

            VkDevice GetDevice() const
            {
                return m_Device;
            }

            VkPhysicalDevice GetGPU() const
            {
                return m_PhysicalDevice->GetHandle();
            }

            const SharedPtr<VKPhysicalDevice>& GetPhysicalDevice() const
            {
                return m_PhysicalDevice;
            }

            VkQueue GetGraphicsQueue() const
            {
                return m_GraphicsQueue;
            }

            VkQueue GetPresentQueue() const
            {
                return m_PresentQueue;
            }

            VkQueue GetComputeQueue() const
            {
                return m_ComputeQueue;
            }

            const SharedPtr<VKCommandPool>& GetCommandPool() const
            {
                return m_CommandPool;
            }

            VkPipelineCache GetPipelineCache() const
            {
                return m_PipelineCache;
            }

#if LUMOS_PROFILE && defined(TRACY_ENABLE) && LUMOS_PROFILE_GPU_TIMINGS
            tracy::VkCtx* GetTracyContext(bool present = false);
#endif

#ifdef USE_VMA_ALLOCATOR
            VmaAllocator GetAllocator() const
            {
                return m_Allocator;
            }
#endif

            static VkDevice GetHandle()
            {
                return VKDevice::Get().GetDevice();
            }

            uint32_t GetGPUCount() const
            {
                return m_PhysicalDevice->GetGPUCount();
            }

            const VkPhysicalDeviceFeatures& GetEnabledFeatures()
            {
                return m_EnabledFeatures;
            }

#ifdef USE_VMA_ALLOCATOR
            VmaPool GetOrCreateSmallAllocPool(uint32_t memTypeIndex);
#endif

        private:
            VkDevice m_Device;

            VkQueue m_ComputeQueue;
            VkQueue m_GraphicsQueue;
            VkQueue m_PresentQueue;
            VkPipelineCache m_PipelineCache;
            VkDescriptorPool m_DescriptorPool;
            VkPhysicalDeviceFeatures m_EnabledFeatures;

            SharedPtr<VKCommandPool> m_CommandPool;
            SharedPtr<VKPhysicalDevice> m_PhysicalDevice;

            bool m_EnableDebugMarkers = false;

            static uint32_t s_GraphicsQueueFamilyIndex;

#if LUMOS_PROFILE && defined(TRACY_ENABLE) && LUMOS_PROFILE_GPU_TIMINGS
            TDArray<tracy::VkCtx*> m_TracyContext;
            tracy::VkCtx* m_PresentTracyContext;
#endif

#ifdef USE_VMA_ALLOCATOR
            VmaAllocator m_Allocator {};
            HashMap(uint32_t, VmaPool) m_SmallAllocPools;
#endif
        };

        class VKGPUMarker
        {
        public:
            VKGPUMarker(const char* name)
            {
                Begin(name);
            }

            ~VKGPUMarker()
            {
                End();
            }
            void Begin(const char* name);
            void End();
        };
    }
}
