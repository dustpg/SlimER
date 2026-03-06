#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cstdio>
#include "er_vulkan_resource.h"
#include "er_vulkan_device.h"
#include "er_vulkan_swapchain.h"
#include "er_vulkan_pipeline.h"
#include "er_rhi_vulkan.h"
#include "../common/er_rhi_common.h"
#ifndef SLIM_RHI_NO_VMA
#include <vma/vk_mem_alloc.h>
#endif

using namespace Slim;

// ----------------------------------------------------------------------------
//                              CVulkanBuffer
// ----------------------------------------------------------------------------

RHI::CVulkanBuffer::CVulkanBuffer(CVulkanDevice* pDevice
    , const BufferDescVulkan& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_sDesc(desc)
    , m_hBuffer(desc.buffer)
#ifndef SLIM_RHI_NO_VMA
    , m_hAllocation(desc.allocation)
    , m_hVmaAllocator(desc.allocator)
    //, m_pMappedData(nullptr)
#endif
{
    assert(pDevice && desc.buffer && desc.allocation && desc.allocator);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
}

RHI::CVulkanBuffer::~CVulkanBuffer() noexcept
{
#ifndef SLIM_RHI_NO_VMA
    // Unmap if currently mapped
    //if (m_pMappedData != nullptr && m_hVmaAllocator != VK_NULL_HANDLE && m_hAllocation != VK_NULL_HANDLE) {
    //    ::vmaUnmapMemory(m_hVmaAllocator, m_hAllocation);
    //    m_pMappedData = nullptr;
    //}
#endif
    if (m_hBuffer != VK_NULL_HANDLE) {
#ifndef SLIM_RHI_NO_VMA
        if (m_hAllocation != VK_NULL_HANDLE && m_hVmaAllocator != VK_NULL_HANDLE) {
            ::vmaDestroyBuffer(m_hVmaAllocator, m_hBuffer, m_hAllocation);
        }
        else {
            ::vkDestroyBuffer(m_hDevice, m_hBuffer, &gc_sAllocationCallbackDRHI);
        }
#else
        assert(!"NOTIMPL");
        ::vkDestroyBuffer(m_hDevice, m_hBuffer, &gc_sAllocationCallbackDRHI);
#endif
        m_hBuffer = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

void RHI::CVulkanBuffer::SetDebugName(const char* name) noexcept
{
    const auto id =impl::vulkan_handle(m_hBuffer);
    impl::vulkan_set_debug_name(m_pThisDevice, VK_OBJECT_TYPE_BUFFER, id, name);
}

RHI::CODE RHI::CVulkanBuffer::Map(void** ppData, const BufferRange* rangeHint) noexcept
{
    if (!ppData)
        return CODE_POINTER;

    *ppData = nullptr;

#ifndef SLIM_RHI_NO_VMA
    assert(m_hVmaAllocator && m_hAllocation);
    //if (m_hVmaAllocator == VK_NULL_HANDLE || m_hAllocation == VK_NULL_HANDLE)
    //    return CODE_UNEXPECTED;

#ifndef NDEBUG
    // Validate rangeHint if provided (used for validation only, not for pointer offset)
    if (rangeHint != nullptr) {
        size_t begin = rangeHint->begin;
        size_t end = rangeHint->end;
        if (end == 0) {
            if (static_cast<uint64_t>(begin) >= m_sDesc.size) {
                assert(!"CHECK THIS");
                return CODE_INVALIDARG;
            }
            end = static_cast<size_t>(m_sDesc.size);
        }
        
        if (end <= begin || static_cast<uint64_t>(end) > m_sDesc.size) {
            assert(!"CHECK THIS");
            return CODE_INVALIDARG;
        }
    }
#endif
    //if (m_pMappedData != nullptr) {
    //    *ppData = m_pMappedData;
    //    assert(!"CHECK THIS");
    //    return CODE_OK;
    //}

    // Map the memory using VMA
    // Note: VMA maps the entire allocation, rangeHint is only used for validation
    VkResult result = ::vmaMapMemory(m_hVmaAllocator, m_hAllocation, ppData);

    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vmaMapMemory failed: %u", result);
    }

    return impl::vulkan_rhi(result);;
#else
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
#endif
}

void RHI::CVulkanBuffer::Unmap() noexcept
{
#ifndef SLIM_RHI_NO_VMA
    assert(m_hVmaAllocator && m_hAllocation);
    //if (m_hVmaAllocator != VK_NULL_HANDLE && m_hAllocation != VK_NULL_HANDLE) 
    {
        //::vmaFlushAllocation(m_hVmaAllocator, m_hAllocation, 0, m_sDesc.size);
        ::vmaUnmapMemory(m_hVmaAllocator, m_hAllocation);
    }
#else
    assert(!"NOTIMPL");
#endif
}

// ----------------------------------------------------------------------------
//                              CVulkanImage
// ----------------------------------------------------------------------------

RHI::CVulkanImage::CVulkanImage(CVulkanDevice* pDevice
    , const ImageDescVulkan& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_sDesc(desc)
    , m_hImage(desc.image)
    , m_pOwner(desc.swapchain)
#ifndef SLIM_RHI_NO_VMA
    , m_hAllocation(desc.allocation)
    , m_hVmaAllocator(desc.allocator)
#endif
    , m_maskAllAspect(impl::rhi_vulkan_aspect_mask(desc.format))
{
    assert(pDevice && desc.image);
#ifndef SLIM_RHI_NO_VMA
    // If not from swapchain, VMA allocation should be valid
    if (!desc.swapchain) {
        assert(desc.allocation && desc.allocator);
    }
#endif
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
    if (m_pOwner)
        m_pOwner->AddRefCnt();
}

RHI::CVulkanImage::~CVulkanImage() noexcept
{
    if (m_hImage != VK_NULL_HANDLE) {
        // NO Owner
        if (m_pOwner) {
            m_pOwner->OwnerDispose(this);
        }
        else {
#ifndef SLIM_RHI_NO_VMA
            if (m_hAllocation != VK_NULL_HANDLE && m_hVmaAllocator != VK_NULL_HANDLE) {
                ::vmaDestroyImage(m_hVmaAllocator, m_hImage, m_hAllocation);
            }
            else {
                ::vkDestroyImage(m_hDevice, m_hImage, &gc_sAllocationCallbackDRHI);
            }
#else
            ::vkDestroyImage(m_hDevice, m_hImage, &gc_sAllocationCallbackDRHI);
#endif
        }
        m_hImage = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pOwner);
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

void RHI::CVulkanImage::SetDebugName(const char* name) noexcept
{
    const auto id = impl::vulkan_handle(m_hImage);
    impl::vulkan_set_debug_name(m_pThisDevice, VK_OBJECT_TYPE_IMAGE, id, name);
}

// ----------------------------------------------------------------------------
//                              CVulkanAttachment
// ----------------------------------------------------------------------------

RHI::CVulkanAttachment::CVulkanAttachment(CVulkanDevice* pDevice
    , const AttachmentDescVulkan& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_pImage(desc.image)
    , m_hImageView(desc.view)
    , m_sDesc(desc)
{
    assert(pDevice && desc.image && desc.view);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
    m_pImage->AddRefCnt();
}

RHI::CVulkanAttachment::~CVulkanAttachment() noexcept
{
    if (m_hImageView != VK_NULL_HANDLE) {
        ::vkDestroyImageView(m_hDevice, m_hImageView, &gc_sAllocationCallbackDRHI);
        m_hImageView = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pImage);
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

// ----------------------------------------------------------------------------
//                              CVulkanFrameBuffer
// ----------------------------------------------------------------------------

#if 0
RHI::CVulkanFrameBuffer::CVulkanFrameBuffer(CVulkanDevice* pDevice
    , const FrameBufferDescVulkan& desc) noexcept
    : m_pThisDevice(pDevice)
    //, m_pRenderPass(desc.renderPass)
    , m_hFrameBuffer(desc.framebuffer)
    //, m_attachmentCount(desc.attachmentCount)
    //, m_width(desc.width)
    //, m_height(desc.height)
    //, m_layers(desc.layers)
{
    assert(pDevice && desc.framebuffer != VK_NULL_HANDLE);
    pDevice->AddRefCnt();
    //m_hDevice = pDevice->GetHandle();
    //m_pRenderPass->AddRefCnt();

    // Copy attachments and add references
    //for (uint32_t i = 0; i < m_attachmentCount && i < (8 + 1); ++i) {
    //    m_pAttachments[i] = desc.attachments[i];
    //    if (m_pAttachments[i]) {
    //        m_pAttachments[i]->AddRefCnt();
    //    }
    //}
}

RHI::CVulkanFrameBuffer::~CVulkanFrameBuffer() noexcept
{
    if (m_hFrameBuffer != VK_NULL_HANDLE) {
        ::vkDestroyFramebuffer(m_pThisDevice->GetHandle(), m_hFrameBuffer, &gc_sAllocationCallbackDRHI);
        m_hFrameBuffer = VK_NULL_HANDLE;
    }

    // Release attachment references
    //for (uint32_t i = 0; i < m_attachmentCount && i < (8 + 1); ++i) {
    //    RHI::SafeDispose(m_pAttachments[i]);
    //    m_pAttachments[i] = nullptr;
    //}
    //m_attachmentCount = 0;

    //RHI::SafeDispose(m_pRenderPass);
    RHI::SafeDispose(m_pThisDevice);
    //m_hDevice = VK_NULL_HANDLE;
}
#endif

#endif
