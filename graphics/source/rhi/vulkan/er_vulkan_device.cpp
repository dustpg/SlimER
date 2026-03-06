#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cstdio>
#include <algorithm>
#include "er_vulkan_device.h"
#include "er_vulkan_adapter.h"
#include "er_vulkan_swapchain.h"
#include "er_vulkan_command.h"
#include "er_vulkan_pipeline.h"
#include "er_vulkan_resource.h"
#include "er_vulkan_descriptor.h"
#include "er_rhi_vulkan.h"
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#endif
#ifndef SLIM_RHI_NO_VMA
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#endif
using namespace Slim;


namespace Slim { namespace impl {

    static VkResult create_surface(
        VkInstance ins,
        void* hwnd,
        const VkAllocationCallbacks* callback,
        VkSurfaceKHR* pSurface) noexcept {
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        createInfo.hinstance = ::GetModuleHandleW(nullptr);
        createInfo.hwnd = reinterpret_cast<HWND>(hwnd);
        VkResult result = ::vkCreateWin32SurfaceKHR(
            ins,
            &createInfo,
            callback,
            pSurface
        );
        return result;
#elif defined(__APPLE__)
        static_assert(0, "NOT IMPL");
#elif defined(__ANDROID__)
        static_assert(0, "NOT IMPL");
#elif defined(__linux__)
        static_assert(0, "NOT IMPL");
#endif
        return VK_ERROR_UNKNOWN;
    }

#ifndef SLIM_RHI_NO_VMA
    VmaMemoryUsage rhi_vma_usage(RHI::MEMORY_TYPE type) noexcept 
    {
        switch (type)
        {
        case RHI::MEMORY_TYPE_DEFAULT:
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // 3.X
            //return VMA_MEMORY_USAGE_GPU_ONLY; // 2.X
        case RHI::MEMORY_TYPE_UPLOAD:
            return VMA_MEMORY_USAGE_AUTO_PREFER_HOST; // 3.X
            //return VMA_MEMORY_USAGE_CPU_TO_GPU; // 2.X
        case RHI::MEMORY_TYPE_READBACK:
            return VMA_MEMORY_USAGE_AUTO_PREFER_HOST; // 3.X
            //return VMA_MEMORY_USAGE_GPU_TO_CPU; // 2.X
        default:
            return VMA_MEMORY_USAGE_AUTO;
        }
    }
    VmaAllocationCreateFlags rhi_vma_flags(RHI::MEMORY_TYPE type) noexcept
    {
        VmaAllocationCreateFlags flags = 0;
        switch (type)
        {
        case RHI::MEMORY_TYPE_UPLOAD:
            flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case RHI::MEMORY_TYPE_READBACK:
            flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        }
        return flags;
    }
    VkMemoryPropertyFlags rhi_vma_required(RHI::MEMORY_TYPE type) noexcept
    {
        VkMemoryPropertyFlags flags = 0;
        switch (type)
        {
        case RHI::MEMORY_TYPE_DEFAULT:
            break;
        case RHI::MEMORY_TYPE_UPLOAD:
            flags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case RHI::MEMORY_TYPE_READBACK:
            flags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ;
            break;
        default:
            break;
        }
        return flags;
    }
#endif
}}

RHI::CVulkanDevice::CVulkanDevice(CVulkanAdapter* ada, 
    VkDevice device) noexcept
    : m_pAdapter(ada)
    , m_hDevice(device)
{
    assert(ada && device);
    ada->AddRefCnt();
}   

RHI::CVulkanDevice::~CVulkanDevice() noexcept
{
    if (m_hStaticSamplerPool != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorPool(m_hDevice, m_hStaticSamplerPool, &gc_sAllocationCallbackDRHI);
        m_hStaticSamplerPool = VK_NULL_HANDLE;
    }
#ifndef SLIM_RHI_NO_VMA
    if (m_hVmaAllocator != VK_NULL_HANDLE) {
        ::vmaDestroyAllocator(m_hVmaAllocator);
        m_hVmaAllocator = VK_NULL_HANDLE;
    }
#endif
    ::vkDestroyDevice(m_hDevice, &gc_sAllocationCallbackDRHI);
    RHI::SafeDispose(m_pAdapter);
}

RHI::CODE RHI::CVulkanDevice::Flush() noexcept
{
    const auto result = ::vkDeviceWaitIdle(m_hDevice);
    return impl::vulkan_rhi(result);
}

void Slim::RHI::CVulkanDevice::DisposeStaticSamplerSet(VkDescriptorSet& set) noexcept
{
    if (set != VK_NULL_HANDLE) {
        assert(this && m_hDevice);
        ::vkFreeDescriptorSets(
            m_hDevice,
            m_hStaticSamplerPool,
            1,
            &set
        );
        set = VK_NULL_HANDLE;
    }
}

RHI::CODE RHI::CVulkanDevice::Init() noexcept
{
    assert(m_hDevice);
    VkResult result{};
#ifndef SLIM_RHI_NO_VMA
    if (result == VK_SUCCESS) {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorInfo.physicalDevice = m_pAdapter->GetHandle();
        allocatorInfo.device = m_hDevice;
        allocatorInfo.instance = m_pAdapter->RefInstance().GetHandle();
        allocatorInfo.pAllocationCallbacks = &gc_sAllocationCallbackDRHI;
        result = ::vmaCreateAllocator(&allocatorInfo, &m_hVmaAllocator);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vmaCreateAllocator failed: %u", result);
        }
    }
#endif
    // POOL FOR STATIC SAMPLER SET
    if (result == VK_SUCCESS) {
        VkDescriptorPoolCreateInfo poolInfo = {};
        VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_SAMPLER, 1 };
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = MAX_STATIC_SAMPLER_SET;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &size;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        result = ::vkCreateDescriptorPool(m_hDevice, &poolInfo, &gc_sAllocationCallbackDRHI, &m_hStaticSamplerPool);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateDescriptorPool(STATIC_SAMPLER) failed: %u", result);
        }
    }

    if (result == VK_SUCCESS) {
        const auto fn = ::vkGetDeviceProcAddr(m_hDevice, "vkCmdPushDescriptorSetKHR");
        m_fnCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(fn);
    }

    return impl::vulkan_rhi(result);
}


RHI::CODE RHI::CVulkanDevice::CreateSwapChain(const SwapChainDesc* pDesc, IRHISwapChain** ppSwapChain) noexcept
{
    if (!pDesc || !ppSwapChain)
        return CODE_POINTER;

    VkResult result = VK_SUCCESS;
    SwapChainDescVulkan descInner;
    VkSurfaceKHR hSurface = VK_NULL_HANDLE;
    const auto hInstance = m_pAdapter->RefInstance().GetHandle();
    const auto hPhysicalDevice = m_pAdapter->GetHandle();
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    pod::vector<VkSurfaceFormatKHR> surfaceFormats;
    //VkPresentModeKHR presentModes [16];
    uint32_t formatCount = 0;
    //uint32_t presentCount = 0;
    VkColorSpaceKHR colorSpace{};
    VkSwapchainKHR hSwapchain = VK_NULL_HANDLE;
    const auto fmt = impl::rhi_vulkan(pDesc->format);

    // CREATE SURFACE
    if (result == VK_SUCCESS) {
        result = impl::create_surface(hInstance, pDesc->windowHandle, &gc_sAllocationCallbackDRHI, &hSurface);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("create_surface failed!");
        }
    }

    // CHECK SUPPORT
    if (result == VK_SUCCESS) {
        VkBool32 support = false;
        result = ::vkGetPhysicalDeviceSurfaceSupportKHR(
            hPhysicalDevice,
            m_pAdapter->GetGraphicsIndex(),
            hSurface,
            &support
        );
        if (!support) {
            // TODO: fallback to find new one
            DRHI_LOGGER_ERR("unsupported vkGetPhysicalDeviceSurfaceSupportKHR");
            result = VK_ERROR_UNKNOWN;
        }
    }

    // GET CAP
    if (result == VK_SUCCESS) {
        result = ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            hPhysicalDevice,
            hSurface,
            &surfaceCapabilities
        );
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
        }
    }

    // GET FORMATS LIST(SIZE)
    if (result == VK_SUCCESS) {
        result = ::vkGetPhysicalDeviceSurfaceFormatsKHR(
            hPhysicalDevice,
            hSurface,
            &formatCount,
            nullptr
        );
        if (!surfaceFormats.resize(formatCount)) {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // GET FORMATS LIST(DATA)
    if (result == VK_SUCCESS) {
        result = ::vkGetPhysicalDeviceSurfaceFormatsKHR(
            hPhysicalDevice,
            hSurface,
            &formatCount,
            surfaceFormats.data()
        );

        const auto found = std::any_of(surfaceFormats.begin(), surfaceFormats.end(),
            [fmt, &colorSpace](const VkSurfaceFormatKHR& item) noexcept {
            colorSpace = item.colorSpace;
            return item.format == fmt;
        });
        if (!found) {
            result = VK_ERROR_FEATURE_NOT_PRESENT;
        }
    }

#if 0

    if (result == VK_SUCCESS) {
        result = ::vkGetPhysicalDeviceSurfacePresentModesKHR(
            hPhysicalDevice,
            surface,
            &presentCount,
            presentModes
        );
        int bk = 9;
    }
#endif

    // CREATE SWAP CHAIN
    if (result == VK_SUCCESS) {
        auto imageCount = std::max(surfaceCapabilities.minImageCount, pDesc->bufferCount);
        imageCount = std::min(surfaceCapabilities.maxImageCount, imageCount);
        imageCount = std::min<uint32_t>(SWAPCHAIN_MAX_BUFFER_COUNT, imageCount);
        
        //VkSwapchainPresentScalingCreateInfoEXT scalingInfo = {};
        //scalingInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT;
        //scalingInfo.scalingBehavior = VK_PRESENT_SCALING_STRETCH_BIT_EXT;
        //scalingInfo.presentGravityX = VK_PRESENT_GRAVITY_CENTERED_BIT_EXT;
        //scalingInfo.presentGravityY = VK_PRESENT_GRAVITY_CENTERED_BIT_EXT;

        VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = hSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = fmt;
        createInfo.imageColorSpace = colorSpace;
        createInfo.imageExtent = surfaceCapabilities.currentExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // TODO: CHECK SUPPORT
        //createInfo.pNext = &scalingInfo;
#if 0
        // TODO:
        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
#endif


        createInfo.preTransform = surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        static_cast<SwapChainDesc&>(descInner) = *pDesc;
        descInner.bufferCount = imageCount;
        descInner.width = createInfo.imageExtent.width;
        descInner.height = createInfo.imageExtent.height;
        descInner.presentMode = createInfo.presentMode;

        result = ::vkCreateSwapchainKHR(
            m_hDevice,
            &createInfo,
            &gc_sAllocationCallbackDRHI,
            &hSwapchain
        );

        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateSwapchainKHR failed");
        }
    }

    // CREATE VULKAN COMMAND QUEUE

    // CREATE RHI COMMAND QUEUE

    // CREATE RHI SWAPCHAIN
    if (result == VK_SUCCESS) {
        descInner.surface = hSurface;
        descInner.swapchain = hSwapchain;
        const auto obj = new (std::nothrow) CVulkanSwapChain{
            this, descInner
        };
        *ppSwapChain = nullptr;
        if (obj) {
            result = obj->Init();
            if (result == VK_SUCCESS) {
                hSwapchain = VK_NULL_HANDLE;
                hSurface = VK_NULL_HANDLE;
                *ppSwapChain = obj;
            }
            else {
                DRHI_LOGGER_ERR("CVulkanSwapChain::Init failed");
                obj->Dispose();
            }
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hSwapchain != VK_NULL_HANDLE) {
        ::vkDestroySwapchainKHR(m_hDevice, hSwapchain, &gc_sAllocationCallbackDRHI);
        hSwapchain = VK_NULL_HANDLE;
    }
    if (hSurface != VK_NULL_HANDLE) {
        ::vkDestroySurfaceKHR(hInstance, hSurface, &gc_sAllocationCallbackDRHI);
        hSurface = VK_NULL_HANDLE;
    }

    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanDevice::CreateCommandQueue(const CommandQueueDesc*, IRHICommandQueue** ppQueue) noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

RHI::CODE RHI::CVulkanDevice::CreateCommandPool(const CommandPoolDesc* pDesc, IRHICommandPool** ppPool) noexcept
{
    if (!pDesc || !ppPool)
        return CODE_POINTER;

    const auto queueType = pDesc->queueType;
    int32_t index = -1;
    switch (queueType)
    {
    case QUEUE_TYPE_GRAPHICS:
        index = this->RefAdapter().GetGraphicsIndex();
        break;
    case QUEUE_TYPE_COMPUTE:
        index = this->RefAdapter().GetComputeIndex();
        break;
    case QUEUE_TYPE_COPY:
        index = this->RefAdapter().GetTransferIndex();
        break;
    default:
        assert(!"NOTIMPL");
        break;
    }

    if (index < 0) {
        assert(!"CODE_INVALIDARG");
        return CODE_INVALIDARG;
    }

    VkCommandPool hCommandPool = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;

    // CREATE VULKAN COMMAND POOL
    if (result == VK_SUCCESS) {
        VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = static_cast<uint32_t>(index);
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        result = ::vkCreateCommandPool(m_hDevice, &poolInfo, &gc_sAllocationCallbackDRHI, &hCommandPool);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateCommandPool failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        const auto obj = new (std::nothrow) CVulkanCmdPool{ this, hCommandPool };
        *ppPool = obj;
        if (obj) {
            hCommandPool = VK_NULL_HANDLE; // Object now owns it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hCommandPool != VK_NULL_HANDLE) {
        ::vkDestroyCommandPool(m_hDevice, hCommandPool, &gc_sAllocationCallbackDRHI);
        hCommandPool = VK_NULL_HANDLE;
    }

    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanDevice::CreateFence(uint64_t initialValue, IRHIFence** ppFence) noexcept
{
    if (!ppFence)
        return CODE_POINTER;


    VkSemaphoreTypeCreateInfo timelineCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = initialValue;

    VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    createInfo.pNext = &timelineCreateInfo;
    createInfo.flags = 0;

    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    VkResult result = ::vkCreateSemaphore(m_hDevice, &createInfo, &gc_sAllocationCallbackDRHI, &timelineSemaphore);


    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkCreateSemaphore failed: %u", result);
    }
    else {
        const auto obj = new(std::nothrow) CVulkanFence{ this, timelineSemaphore };
        *ppFence = obj;
        if (obj) {
            timelineSemaphore = VK_NULL_HANDLE; // object own it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (timelineSemaphore != VK_NULL_HANDLE) {
        ::vkDestroySemaphore(m_hDevice, timelineSemaphore, &gc_sAllocationCallbackDRHI);
        timelineSemaphore = VK_NULL_HANDLE;
    }
    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanDevice::CreateBuffer(const BufferDesc* pDesc, IRHIBuffer** ppBuffer) noexcept
{
    if (!pDesc || !ppBuffer)
        return CODE_POINTER;
    if (!pDesc->size)
        return CODE_INVALIDARG;


    VkBuffer hBuffer = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;

#ifndef SLIM_RHI_NO_VMA
    // Convert BUFFER_USAGES to VkBufferUsageFlags
    const auto usageFlags = impl::rhi_vulkan(pDesc->usage);
    if (usageFlags == 0) {
        return CODE_INVALIDARG;
    }


    // CREATE VULKAN BUFFER WITH VMA
    VmaAllocation allocation = VK_NULL_HANDLE;
    if (result == VK_SUCCESS) {
        VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = pDesc->size;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = impl::rhi_vma_usage(pDesc->type);
        allocInfo.flags = impl::rhi_vma_flags(pDesc->type);
        allocInfo.requiredFlags = impl::rhi_vma_required(pDesc->type);

        if (pDesc->alignment) {
            // Use vmaCreateBufferWithAlignment for custom alignment
            result = ::vmaCreateBufferWithAlignment(
                m_hVmaAllocator,
                &bufferInfo,
                &allocInfo,
                static_cast<VkDeviceSize>(pDesc->alignment),
                &hBuffer,
                &allocation,
                nullptr
            );
            if (result != VK_SUCCESS) {
                DRHI_LOGGER_ERR("vmaCreateBufferWithAlignment failed: %u", result);
            }
        }
        else {
            // Use standard vmaCreateBuffer for default alignment
            result = ::vmaCreateBuffer(
                m_hVmaAllocator,
                &bufferInfo,
                &allocInfo,
                &hBuffer,
                &allocation,
                nullptr
            );
            if (result != VK_SUCCESS) {
                DRHI_LOGGER_ERR("vmaCreateBuffer failed: %u", result);
            }
        }
    }
#else
    static_assert(false, "NOTIMPL");
    const auto usageFlags = impl::rhi_vulkan(pDesc->type);
    if (usageFlags == 0) {
        return CODE_INVALIDARG;
    }

    // CREATE VULKAN OBJECT
    if (result == VK_SUCCESS) {
        VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = pDesc->size;
        createInfo.usage = usageFlags;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        result = ::vkCreateBuffer(m_hDevice, &createInfo, &gc_sAllocationCallbackDRHI, &hBuffer);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateBuffer failed: %u", result);
        }
    }

#endif
    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        BufferDescVulkan descVulkan {};
        static_cast<BufferDesc&>(descVulkan) = *pDesc;
        descVulkan.buffer = hBuffer;
#ifndef SLIM_RHI_NO_VMA
        descVulkan.allocation = allocation;
        descVulkan.allocator = m_hVmaAllocator;
#endif
        CVulkanBuffer* pBuffer = new(std::nothrow) CVulkanBuffer{ this, descVulkan };
        if (pBuffer) {
            hBuffer = VK_NULL_HANDLE; // object owns it
#ifndef SLIM_RHI_NO_VMA
            allocation = VK_NULL_HANDLE; // object owns it
#endif
            *ppBuffer = pBuffer;
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hBuffer != VK_NULL_HANDLE) {
#ifndef SLIM_RHI_NO_VMA
        if (allocation != VK_NULL_HANDLE) {
            ::vmaDestroyBuffer(m_hVmaAllocator, hBuffer, allocation);
        }
        else {
            ::vkDestroyBuffer(m_hDevice, hBuffer, &gc_sAllocationCallbackDRHI);
        }
#else
        ::vkDestroyBuffer(m_hDevice, hBuffer, &gc_sAllocationCallbackDRHI);
#endif
        hBuffer = VK_NULL_HANDLE;
    }

    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanDevice::CreateShader(
    const void* data, size_t size, 
    IRHIShader** ppShader, const char* entry) noexcept
{
    CVulkanShader* pShader_NoClean = nullptr;
    if (!data || !ppShader)
        return CODE_POINTER;
    if (!size)
        return CODE_INVALIDARG;

    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = size;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(data);

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    VkResult result = VK_SUCCESS;
    // CREATE SHADER MODULE
    if (result == VK_SUCCESS) {
        result = ::vkCreateShaderModule(m_hDevice, &createInfo, &gc_sAllocationCallbackDRHI, &shaderModule);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateShaderModule failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        pShader_NoClean = new(std::nothrow) CVulkanShader{ this, shaderModule };
        if (pShader_NoClean) {
            shaderModule = VK_NULL_HANDLE;
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            ::vkDestroyShaderModule(m_hDevice, shaderModule, &gc_sAllocationCallbackDRHI);
            return CODE_OUTOFMEMORY;
        }
    }

    auto code = impl::vulkan_rhi(result);
    if (result == VK_SUCCESS) {
        if (entry && entry[0]) {
            code = pShader_NoClean->SetEntry(entry);
            if (RHI::Failure(code)) {
                DRHI_LOGGER_ERR("SetEntry failed: entry name too long");
                pShader_NoClean->Dispose();
                pShader_NoClean = nullptr;
            }
        }
    }

    *ppShader = pShader_NoClean;

    // CLEAN UP
    if (shaderModule != VK_NULL_HANDLE) {
        ::vkDestroyShaderModule(m_hDevice, shaderModule, &gc_sAllocationCallbackDRHI);
        shaderModule = VK_NULL_HANDLE;
    }

    return code;
}

RHI::CODE RHI::CVulkanDevice::CreateImage(const ImageDesc* pDesc, IRHIImage** ppImage) noexcept
{
    if (!pDesc || !ppImage)
        return CODE_POINTER;

#ifndef SLIM_RHI_NO_VMA
    if (!pDesc->width || !pDesc->height)
        return CODE_INVALIDARG;

    VkImage hImage = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;

    // Convert IMAGE_USAGES to VkImageUsageFlags
    const auto usageFlags = impl::rhi_vulkan(pDesc->usage);
    if (usageFlags == 0) {
        return CODE_INVALIDARG;
    }

    // CREATE VULKAN IMAGE WITH VMA
    VmaAllocation allocation = VK_NULL_HANDLE;
    if (result == VK_SUCCESS) {
        VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = pDesc->width;
        imageInfo.extent.height = pDesc->height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = impl::rhi_vulkan(pDesc->format);
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        //imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // impl::rhi_vulkan(pDesc->initialLayout);
        imageInfo.usage = usageFlags;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = impl::rhi_vma_usage(RHI::MEMORY_TYPE_DEFAULT);
        allocInfo.flags = impl::rhi_vma_flags(RHI::MEMORY_TYPE_DEFAULT);
        allocInfo.requiredFlags = impl::rhi_vma_required(RHI::MEMORY_TYPE_DEFAULT);

        result = ::vmaCreateImage(
            m_hVmaAllocator,
            &imageInfo,
            &allocInfo,
            &hImage,
            &allocation,
            nullptr
        );
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vmaCreateImage failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        ImageDescVulkan descVulkan{};
        static_cast<ImageDesc&>(descVulkan) = *pDesc;
        descVulkan.image = hImage;
        descVulkan.swapchain = nullptr;
#ifndef SLIM_RHI_NO_VMA
        descVulkan.allocation = allocation;
        descVulkan.allocator = m_hVmaAllocator;
#endif
        CVulkanImage* pImage = new(std::nothrow) CVulkanImage{ this, descVulkan };
        if (pImage) {
            hImage = VK_NULL_HANDLE; // object owns it
#ifndef SLIM_RHI_NO_VMA
            allocation = VK_NULL_HANDLE; // object owns it
#endif
            *ppImage = pImage;
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hImage != VK_NULL_HANDLE) {
#ifndef SLIM_RHI_NO_VMA
        if (allocation != VK_NULL_HANDLE) {
            ::vmaDestroyImage(m_hVmaAllocator, hImage, allocation);
        }
        else {
            ::vkDestroyImage(m_hDevice, hImage, &gc_sAllocationCallbackDRHI);
        }
#else
        ::vkDestroyImage(m_hDevice, hImage, &gc_sAllocationCallbackDRHI);
#endif
        hImage = VK_NULL_HANDLE;
    }

    return impl::vulkan_rhi(result);
#else
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
#endif
}

RHI::CODE RHI::CVulkanDevice::CreateAttachment(const AttachmentDesc* pDesc, IRHIImage * pImage, IRHIAttachment** ppView) noexcept
{
    if (!pDesc || !pImage || !ppView)
        return CODE_POINTER;
    
    const auto image = static_cast<CVulkanImage*>(pImage);
    // SIMPLE RTTI
    assert(this == &image->RefDevice());

    VkImageView hImageView = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    AttachmentDescVulkan desc{};
    static_cast<AttachmentDesc&>(desc) = *pDesc;

    // CREATE ATTACHMENT
    if (result == VK_SUCCESS) {
        const auto& imageDesc = image->RefDesc();
        // Use format from AttachmentDesc if specified, otherwise use format from Image
        desc.format = (pDesc->format != RHI::RHI_FORMAT_UNKNOWN)
            ? pDesc->format
            : imageDesc.format;

        VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = image->GetHandle();
        // TODO: VK_IMAGE_VIEW_TYPE
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = impl::rhi_vulkan(desc.format);

        // TODO: components/subresourceRange
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = impl::rhi_vulkan(pDesc->type);
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        result = ::vkCreateImageView(m_hDevice, &createInfo, &gc_sAllocationCallbackDRHI, &hImageView);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateImageView failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        desc.image = image;
        desc.view = hImageView;

        const auto pAttachmentObj = new (std::nothrow) CVulkanAttachment{ this, desc };
        *ppView = pAttachmentObj;
        if (pAttachmentObj) {
            hImageView = VK_NULL_HANDLE; // Object will own it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hImageView != VK_NULL_HANDLE) {
        ::vkDestroyImageView(m_hDevice, hImageView, &gc_sAllocationCallbackDRHI);
        hImageView = VK_NULL_HANDLE;
    }
    return impl::vulkan_rhi(result);
}

RHI::CVulkanFence::CVulkanFence(CVulkanDevice* pDevice
    , VkSemaphore semaphore) noexcept
    : m_pThisDevice(pDevice)
    , m_hTimelineSemaphore(semaphore)
{
    assert(pDevice && semaphore);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
}

RHI::CVulkanFence::~CVulkanFence() noexcept
{
    if (m_hTimelineSemaphore != VK_NULL_HANDLE) {
        ::vkDestroySemaphore(m_hDevice, m_hTimelineSemaphore, &gc_sAllocationCallbackDRHI);
        m_hTimelineSemaphore = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

uint64_t RHI::CVulkanFence::GetCompletedValue() noexcept
{
    assert(m_hTimelineSemaphore != VK_NULL_HANDLE);
    uint64_t currentValue{};
    const auto result = ::vkGetSemaphoreCounterValue(m_hDevice, m_hTimelineSemaphore, &currentValue);
    if (result != VK_SUCCESS) {
        // TIDO: ERROR HANDLE
        DRHI_LOGGER_ERR("vkGetSemaphoreCounterValue failed: %u", result);
    }
    return currentValue;
}

RHI::WAIT Slim::RHI::CVulkanFence::Wait(uint64_t value, uint32_t time) noexcept
{
    assert(m_hDevice && m_hTimelineSemaphore);
    const auto timeoutNs = time == RHI::WAIT_FOREVER ? UINT64_MAX : uint64_t(time) * uint64_t(1000 * 1000);


    VkSemaphoreWaitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_hTimelineSemaphore;
    waitInfo.pValues = &value;

    const auto result = ::vkWaitSemaphores(m_hDevice, &waitInfo, timeoutNs);

    if (result == VK_SUCCESS)
        return WAIT_CODE_SUCCESS;
    if (result == VK_TIMEOUT)
        return WAIT_CODE_TIME_OUT;

    return WAIT_CODE_FAILED;
}

#if 0
RHI::CODE RHI::CVulkanDevice::CreateFrameBuffer(const FrameBufferDesc* pDesc, IRHIFrameBuffer** ppFrameBuffer) noexcept
{
    if (!pDesc || !ppFrameBuffer)
        return CODE_POINTER;
    constexpr uint32_t COLOR_BLEND_ATTACHMENT_COUNT = sizeof(BlendDesc::attachments) / sizeof(BlendDesc::attachments[0]);

    if (!pDesc->renderpass || pDesc->colorViewCount > COLOR_BLEND_ATTACHMENT_COUNT) {
        assert(!"CODE_INVALIDARG");
        return CODE_INVALIDARG;;
    }
    const auto renderpass = static_cast<CVulkanRenderPass*>(pDesc->renderpass);
    // SIMPLE RTTI
    assert(&renderpass->RefDevice() == this);

    // +1 depth/stencil
    VkImageView handles[COLOR_BLEND_ATTACHMENT_COUNT + 1];
    CVulkanAttachment* views[COLOR_BLEND_ATTACHMENT_COUNT + 1];
    CVulkanAttachment* last = nullptr;

    VkFramebufferCreateInfo framebufferInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    
    uint32_t countAll = pDesc->colorViewCount;
    {
        // COLOR
        const uint32_t count = pDesc->colorViewCount;
        for (uint32_t i = 0; i != count; ++i) {
            last = static_cast<CVulkanAttachment*>(pDesc->colorViews[i]);
            views[i] = last;
            // SIMPLE RTTI
            assert(&last->RefDevice() == this);
            handles[i] = last->GetHandle();
        }
        // DEPTH/STENCIL
        if (pDesc->depthStencil) {
            views[count] = static_cast<CVulkanAttachment*>(pDesc->depthStencil);
            if (!last) last = views[count];
            handles[count] = views[count]->GetHandle();
            ++countAll;
        }
    }

    VkResult result = VK_SUCCESS;
    VkFramebuffer hFrameBuffer = VK_NULL_HANDLE;

    if (result == VK_SUCCESS) {
        auto width = pDesc->width;
        auto height = pDesc->height;
        // 0 = auto value from image
        if ((!width || !height) && last) {
            const auto& desc = last->RefImage().RefDesc();
            width = desc.width;
            height = desc.height;
        }
        const uint32_t count = pDesc->colorViewCount;

        framebufferInfo.renderPass = renderpass->GetHandle();
        framebufferInfo.attachmentCount = countAll;
        framebufferInfo.pAttachments = handles;
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        result = ::vkCreateFramebuffer(m_hDevice, &framebufferInfo, &gc_sAllocationCallbackDRHI, &hFrameBuffer);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateFramebuffer failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        FrameBufferDescVulkan desc{};
        static_cast<FrameBufferDesc&>(desc) = *pDesc;
        desc.framebuffer = hFrameBuffer;

        const auto pFrameBufferObj = new (std::nothrow) CVulkanFrameBuffer{ this, desc, &gc_sAllocationCallbackDRHI };
        *ppFrameBuffer = pFrameBufferObj;
        if (pFrameBufferObj) {
            hFrameBuffer = VK_NULL_HANDLE; // Object will own it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hFrameBuffer != VK_NULL_HANDLE) {
        ::vkDestroyFramebuffer(m_hDevice, hFrameBuffer, &gc_sAllocationCallbackDRHI);
        hFrameBuffer = VK_NULL_HANDLE;
    }
    return impl::vulkan_rhi(result);
}
#endif
#endif
