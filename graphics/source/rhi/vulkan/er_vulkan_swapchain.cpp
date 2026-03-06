#include "er_vulkan_swapchain.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cstdio>
#include <algorithm>
#include "er_backend_vulkan.h"
#include "er_vulkan_device.h"
#include "er_vulkan_adapter.h"
#include "er_vulkan_command.h"
#include "er_vulkan_resource.h"
#include "er_rhi_vulkan.h"
using namespace Slim;



/*
 REF MAP:
   Instance <- Adapter <- Device <- CommandQueue
                            ^          ^
                  --------- |          |
                  |         |          |
                  |     SwapChain------+(PresentQueue)
                  |      ^    ||
                  |      |    ||
                Image-----    ||
                  ^^          || 
                  ||====WEAK====
*/

RHI::CVulkanSwapChain::CVulkanSwapChain(CVulkanDevice* device,
    const SwapChainDescVulkan& desc) noexcept
    : m_pThisDevice(device)
    , m_sDesc(desc)
    // TODO: if PresentIndex != GraphicsIndex
    , m_uPresentIndex(device->RefAdapter().GetGraphicsIndex())
{
    assert(device && desc.surface && desc.swapchain);
    device->AddRefCnt();
}

RHI::CVulkanSwapChain::~CVulkanSwapChain() noexcept
{
    const auto device = m_pThisDevice->GetHandle();
    //const auto commandPool = m_pThisDevice->GetGraphicsCmdPool();
    ::vkDeviceWaitIdle(device);

    const auto scCount = m_sDesc.bufferCount;
    for (uint32_t i = 0; i != scCount; ++i) {
        auto& data = m_szSwapChainImages[i];
        ::vkDestroySemaphore(device, data.renderFinished, &gc_sAllocationCallbackDRHI);
    }

    for (uint32_t i = 0; i != MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& data = m_szFlightFrames[i];
        ::vkDestroySemaphore(device, data.imageAvailable, &gc_sAllocationCallbackDRHI);
        ::vkDestroyFence(device, data.inFlight, &gc_sAllocationCallbackDRHI);
    }

    std::memset(m_szSwapChainImages, 0, sizeof(m_szSwapChainImages));
    std::memset(m_szFlightFrames, 0, sizeof(m_szFlightFrames));

    if (m_sDesc.swapchain != VK_NULL_HANDLE) {
        const auto hDevice = m_pThisDevice->GetHandle();
        ::vkDestroySwapchainKHR(hDevice, m_sDesc.swapchain, &gc_sAllocationCallbackDRHI);
        m_sDesc.swapchain = VK_NULL_HANDLE;
    }

    if (m_sDesc.surface != VK_NULL_HANDLE) {
        const auto hInstance = m_pThisDevice->RefAdapter().RefInstance().GetHandle();
        ::vkDestroySurfaceKHR(hInstance, m_sDesc.surface, &gc_sAllocationCallbackDRHI);
        m_sDesc.surface = VK_NULL_HANDLE;
    }

    RHI::SafeDispose(m_pGraphicsQueue);
    RHI::SafeDispose(m_pThisDevice);
}



VkResult RHI::CVulkanSwapChain::Init() noexcept
{
    assert(m_szFlightFrames[0].inFlight == VK_NULL_HANDLE);
    assert(m_pGraphicsQueue == nullptr);
    VkResult result{};

    VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    semaphoreInfo.flags = 0;

    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    const auto device = m_pThisDevice->GetHandle();

    if (result == VK_SUCCESS) {
        // TODO: support dynamic count of image?
        ::vkGetSwapchainImagesKHR(device, m_sDesc.swapchain, &m_sDesc.bufferCount, nullptr);
        if (m_sDesc.bufferCount > SWAPCHAIN_MAX_BUFFER_COUNT) {
            result = VK_ERROR_FEATURE_NOT_PRESENT;
        }
    }

    // CREATE Semaphore/Fence FOR FRAMES IN FLIGHT
    if (result == VK_SUCCESS) {
        for (uint32_t i = 0; i != MAX_FRAMES_IN_FLIGHT; ++i) {
            auto& data = m_szFlightFrames[i];
            if (result == VK_SUCCESS) {
                result = ::vkCreateSemaphore(device, &semaphoreInfo, &gc_sAllocationCallbackDRHI, &data.imageAvailable);
            }
            if (result == VK_SUCCESS) {
                result = ::vkCreateFence(device, &fenceInfo, &gc_sAllocationCallbackDRHI, &data.inFlight);
            }
        }
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateSemaphore/vkCreateFence failed!");
        }
    }

    auto& adapter = m_pThisDevice->RefAdapter();
    // TODO: HIGHER queueIndex
    ::vkGetDeviceQueue(device, adapter.GetGraphicsIndex(), 0, &m_hGraphicsQueue);
    ::vkGetDeviceQueue(device, this->GetPresentIndex(), 0, &m_hPresentQueue);

    if (result == VK_SUCCESS) {
        m_pGraphicsQueue = new(std::nothrow) CVulkanCmdQueue{ m_pThisDevice, m_hGraphicsQueue };
        if (!m_pGraphicsQueue) {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result == VK_SUCCESS) {
        result = m_pGraphicsQueue->Init(this);
        if (result != VK_SUCCESS) {
            RHI::SafeDispose(m_pGraphicsQueue);
        }
    }
#if 0
    if (result == VK_SUCCESS) {
        //const auto count = m_sDesc.bufferCount;
        VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool = m_pThisDevice->GetGraphicsCmdPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        for (uint32_t i = 0; i != MAX_FRAMES_IN_FLIGHT; ++i) {
            result = ::vkAllocateCommandBuffers(device, &allocInfo, &m_szFlightFrames[i].cmdBuffer);
            if (result != VK_SUCCESS) {
                DRHI_LOGGER_ERR("vkCreateSemaphore/vkCreateFence failed!");
                break;
            }
        }
    }
#endif
    if (result == VK_SUCCESS) {
        VkImage imgBuffers[SWAPCHAIN_MAX_BUFFER_COUNT]{};
        const auto count = m_sDesc.bufferCount;
        result = ::vkGetSwapchainImagesKHR(device, m_sDesc.swapchain, &m_sDesc.bufferCount, imgBuffers);
        for (uint32_t i = 0; i != count; ++i) {
            m_szSwapChainImages[i].handle = imgBuffers[i];
            if (result == VK_SUCCESS) {
                result = ::vkCreateSemaphore(device, &semaphoreInfo, &gc_sAllocationCallbackDRHI, &m_szSwapChainImages[i].renderFinished);
            }
        }
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkGetSwapchainImagesKHR/vkCreateSemaphore failed!");
        }
    }

    return result;
}

void RHI::CVulkanSwapChain::GetDesc(SwapChainDesc* pDesc) noexcept
{
    if (pDesc) *pDesc = m_sDesc;
}

void RHI::CVulkanSwapChain::GetBoundQueue(IRHICommandQueue** ppQueue) noexcept
{
    if (ppQueue) {
        *ppQueue = m_pGraphicsQueue;
        m_pGraphicsQueue->AddRefCnt();
    }
}

RHI::CODE RHI::CVulkanSwapChain::AcquireFrame(FrameContext& ctx) noexcept
{
    ctx = {};
    VkResult result{};
    const auto device = m_pThisDevice->GetHandle();
    auto& frame = m_szFlightFrames[m_uCurrentFlight];

    result = ::vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        // TODO: ERROR HANDLE FOR FENCE
        DRHI_LOGGER_ERR("vkWaitForFences failed: %u", result);
    }
    result = ::vkResetFences(device, 1, &frame.inFlight);
    if (result != VK_SUCCESS) {
        // TODO: ERROR HANDLE FOR FENCE
        DRHI_LOGGER_ERR("vkResetFences failed: %u", result);
    }
    result = ::vkAcquireNextImageKHR(device, m_sDesc.swapchain,
        UINT64_MAX,
        frame.imageAvailable,
        VK_NULL_HANDLE,
        &m_uNextFrame
    );
    
    // VK_SUBOPTIMAL_KHR

    if (result != VK_SUCCESS) {
        switch (result)
        {
        case VK_SUBOPTIMAL_KHR:
            // TODO: VK_SUBOPTIMAL_KHR
            DRHI_LOGGER_ERR("vkAcquireNextImageKHR: VK_SUBOPTIMAL_KHR");
            result = VK_SUCCESS;
            break;
        default:
            DRHI_LOGGER_ERR("vkAcquireNextImageKHR failed: %u", result);
            break;
        }
    }

    //assert(result != VK_ERROR_OUT_OF_DATE_KHR && "TODO");

    ctx.indexBuffer = m_uNextFrame;
    ctx.indexInFlight = m_uCurrentFlight;

    assert(m_pGraphicsQueue);
    m_pGraphicsQueue->AfterAcquire();

    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanSwapChain::PresentFrame(uint32_t syncInterval) noexcept
{
    assert(m_pGraphicsQueue);
    m_pGraphicsQueue->BeforePresent();

    // TODO: syncInterval
    auto& frameFt = this->CurrentFlight();
    auto& frameSC = this->CurrentSwapchain();


    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameSC.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_sDesc.swapchain;
    presentInfo.pImageIndices = &m_uNextFrame;


    const auto hDevice = m_pThisDevice->GetHandle();

    auto result = ::vkQueuePresentKHR(m_hPresentQueue, &presentInfo);

    m_uCurrentFlight = (m_uCurrentFlight + 1) % MAX_FRAMES_IN_FLIGHT;

    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanSwapChain::Resize(uint32_t width, uint32_t height) noexcept
{
    assert(!"NOTIMPL");
    return CODE();
}

void Slim::RHI::CVulkanSwapChain::OwnerDispose(CVulkanImage* image) noexcept
{
    assert(image);
    const auto begin = m_szSwapChainImages;
    const auto end = m_szSwapChainImages + m_sDesc.bufferCount;
    const auto itr = std::find_if(begin, end, [=](const SwapChainImageData& d) noexcept {
        return d.image.ptr == image;
    });
    if (itr == end) {
        assert("BAD OBJECT");
    }
    else {
        itr->image = { };
    }
}

RHI::CODE RHI::CVulkanSwapChain::GetImage(uint32_t index, IRHIImage** image) noexcept
{
    if (!image)
        return CODE_POINTER;
    if (index >= m_sDesc.bufferCount)
        return CODE_INVALIDARG;

    auto& data = m_szSwapChainImages[index];
    if (!data.image.ptr) {
        ImageDescVulkan desc{};
        desc.width = m_sDesc.width;
        desc.height = m_sDesc.height;
        desc.format = m_sDesc.format;
        desc.image = data.handle;
        desc.swapchain = this;
        desc.usage.flags = IMAGE_USAGE_TRANSFER_SRC | IMAGE_USAGE_TRANSFER_DST | IMAGE_USAGE_COLOR_ATTACHMENT;
        data.image.ptr = new(std::nothrow) CVulkanImage{ m_pThisDevice, desc };
        if (data.image.ptr) {
            *image = data.image.ptr;
            return CODE_OK;
        }
        else {
            return CODE_OUTOFMEMORY;
        }
    }
    data.image.ptr->AddRefCnt();

    *image = data.image.ptr;
    return CODE_OK;
}

#endif