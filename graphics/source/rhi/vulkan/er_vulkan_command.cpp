#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cassert>
#include <cstdio>
#include "er_vulkan_command.h"
#include "er_vulkan_pipeline.h"
#include "er_vulkan_resource.h"
#include "er_vulkan_device.h"
#include "er_vulkan_swapchain.h"
#include "er_vulkan_descriptor.h"
#include "er_rhi_vulkan.h"
#include "../common/er_rhi_common.h"

using namespace Slim;

RHI::CVulkanCmdList::CVulkanCmdList(CVulkanCmdPool* pool, VkCommandBuffer buffer) noexcept
    : m_pCmdPool(pool)
    //, m_hDevice(pool->GetDevice())
    , m_hCommandBuffer(buffer)
    , m_fnCmdPushDescriptorSetKHR(pool->RefDevice().FnCmdPushDescriptorSet())
{
    assert(pool && buffer);
    pool->AddRefCnt();
}

RHI::CVulkanCmdList::~CVulkanCmdList() noexcept
{
    if (m_hCommandBuffer != VK_NULL_HANDLE) {
        const auto device = m_pCmdPool->GetDeviceHandle();
        const auto pool = m_pCmdPool->GetHandle();
        ::vkFreeCommandBuffers(device, pool, 1, &m_hCommandBuffer);
        m_hCommandBuffer = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pCmdPool);
}


RHI::CODE RHI::CVulkanCmdList::Begin() noexcept
{
    VkCommandBufferBeginInfo info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    const auto result = ::vkBeginCommandBuffer(m_hCommandBuffer, &info);
    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkBeginCommandBuffer failed: %u", result);
    }
    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanCmdList::End() noexcept
{
    const auto result = ::vkEndCommandBuffer(m_hCommandBuffer);
    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkEndCommandBuffer failed: %u", result);
    }
    return impl::vulkan_rhi(result);
}

void RHI::CVulkanCmdList::BeginRenderPass(const BeginRenderPassDesc* pDesc) noexcept
{
    assert(pDesc);
#if 0
    VkRenderPassBeginInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    const auto renderPass = static_cast<CVulkanRenderPass*>(pDesc->renderpass);
    const auto framebuffer = static_cast<CVulkanFrameBuffer*>(pDesc->framebuffer);
    
    assert(&renderPass->RefDevice() == &m_pCmdPool->RefDevice());
    assert(&framebuffer->RefDevice() == &m_pCmdPool->RefDevice());

    renderPassInfo.renderPass = renderPass->GetHandle();
    renderPassInfo.framebuffer = framebuffer->GetHandle();
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { pDesc->width, pDesc->height };


    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    
    ::vkCmdBeginRenderPass(m_hCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
#else
    // dynamic rendering
    const auto passWidth = pDesc->width;
    const auto passHeight = pDesc->height;

    constexpr uint32_t MAX_COLOR_ATTACHMENTS = sizeof(pDesc->colors) / sizeof(pDesc->colors[0]);
    VkRenderingAttachmentInfo colorAttachments[MAX_COLOR_ATTACHMENTS];
    VkRenderingAttachmentInfo* pDepthAttachment = nullptr;
    VkRenderingAttachmentInfo* pStencilAttachment = nullptr;
    VkRenderingAttachmentInfo depthStencilAttachment[2];

    // Setup color attachments
    const uint32_t colorCount = pDesc->colorCount;
    assert(colorCount <= MAX_COLOR_ATTACHMENTS);
    
    for (uint32_t i = 0; i < colorCount; ++i) {
        const auto& colorDesc = pDesc->colors[i];
        auto& colorAttach = colorAttachments[i];
        
        const auto colorView = static_cast<CVulkanAttachment*>(colorDesc.attachment);
        assert(&colorView->RefDevice() == &m_pCmdPool->RefDevice());
        
        colorAttach = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttach.imageView = colorView->GetHandle();
        colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttach.loadOp = impl::rhi_vulkan(colorDesc.load);
        colorAttach.storeOp = impl::rhi_vulkan(colorDesc.store);
        
        colorAttach.clearValue.color.float32[0] = colorDesc.clear[0];
        colorAttach.clearValue.color.float32[1] = colorDesc.clear[1];
        colorAttach.clearValue.color.float32[2] = colorDesc.clear[2];
        colorAttach.clearValue.color.float32[3] = colorDesc.clear[3];
        // TODO: resolveMode
        colorAttach.resolveMode = VK_RESOLVE_MODE_NONE;
        colorAttach.resolveImageView = VK_NULL_HANDLE;
        colorAttach.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    // Setup depth/stencil attachment if present
    if (pDesc->depthStencil.attachment) {
        const auto& dsDesc = pDesc->depthStencil;
        const auto dsView = static_cast<CVulkanAttachment*>(dsDesc.attachment);
        assert(&dsView->RefDevice() == &m_pCmdPool->RefDevice());
        
        depthStencilAttachment[0] = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthStencilAttachment[0].imageView = dsView->GetHandle();
        depthStencilAttachment[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStencilAttachment[0].loadOp = impl::rhi_vulkan(dsDesc.depthLoad);
        depthStencilAttachment[0].storeOp = impl::rhi_vulkan(dsDesc.depthStore);
        depthStencilAttachment[0].clearValue.depthStencil.depth = pDesc->depthStencil.depthClear;

        depthStencilAttachment[0].resolveMode = VK_RESOLVE_MODE_NONE;
        depthStencilAttachment[0].resolveImageView = VK_NULL_HANDLE;
        depthStencilAttachment[0].resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        pDepthAttachment = depthStencilAttachment + 0;

        if (pDesc->depthStencil.stencilTestEnable) {

            depthStencilAttachment[1] = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            depthStencilAttachment[1].imageView = dsView->GetHandle();
            depthStencilAttachment[1].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthStencilAttachment[1].loadOp = impl::rhi_vulkan(dsDesc.stencilLoad);
            depthStencilAttachment[1].storeOp = impl::rhi_vulkan(dsDesc.stencilStore);
            depthStencilAttachment[1].clearValue.depthStencil.stencil = pDesc->depthStencil.stencilClear;

            depthStencilAttachment[1].resolveMode = VK_RESOLVE_MODE_NONE;
            depthStencilAttachment[1].resolveImageView = VK_NULL_HANDLE;
            depthStencilAttachment[1].resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            pStencilAttachment = depthStencilAttachment + 1;
        }
    }

    // Setup rendering info
    VkRenderingInfo renderingInfo { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.flags = 0;
    renderingInfo.renderArea = { { 0, 0 }, { passWidth, passHeight } };
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = 0;
    renderingInfo.colorAttachmentCount = colorCount;
    renderingInfo.pColorAttachments = (colorCount > 0) ? colorAttachments : nullptr;
    renderingInfo.pDepthAttachment = pDepthAttachment;
    renderingInfo.pStencilAttachment = pStencilAttachment;

    ::vkCmdBeginRendering(m_hCommandBuffer, &renderingInfo);
#endif
}

void RHI::CVulkanCmdList::EndRenderPass() noexcept
{
#if 0
    ::vkCmdEndRenderPass(m_hCommandBuffer);
#else
    // dynamic rendering
    ::vkCmdEndRendering(m_hCommandBuffer);
#endif
}


void RHI::CVulkanCmdList::BindPipeline(IRHIPipeline* pPipeline) noexcept
{
    assert(pPipeline);
    const auto pipeline = static_cast<CVulkanPipeline*>(pPipeline);
    const auto layout = &pipeline->RefLayout();
    assert(&layout->RefDevice() == &m_pCmdPool->RefDevice());
    ::vkCmdBindPipeline(m_hCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
    
    if (const auto staticSamplerSet = layout->StaticSamplerSetHandle()) {
        const uint32_t setIndex = layout->StaticSamplerSetId();
        ::vkCmdBindDescriptorSets(
            m_hCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            layout->GetHandle(),
            setIndex,
            1,
            &staticSamplerSet,
            0,
            nullptr
        );
    }
}

void RHI::CVulkanCmdList::BindDescriptorBuffer(IRHIPipelineLayout* pLayout, IRHIDescriptorBuffer* pDescriptorBuffer) noexcept
{
    assert(pLayout && pDescriptorBuffer);
    const auto layout = static_cast<CVulkanPipelineLayout*>(pLayout);
    const auto desc = static_cast<CVulkanDescriptorBuffer*>(pDescriptorBuffer);
    // SIMPLE RTTI
    assert(&layout->RefDevice() == &m_pCmdPool->RefDevice());
    assert(&desc->RefDevice() == &m_pCmdPool->RefDevice());

    const auto handle = desc->GetSetHandle();

    ::vkCmdBindDescriptorSets(
        m_hCommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout->GetHandle(),
        layout->DescriptorBufferSetId(),
        1,
        &handle,
        0,
        nullptr
    );
}

void RHI::CVulkanCmdList::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const VertexBufferView* pViews) noexcept
{
    constexpr uint32_t RAID = 32;
    VkBuffer vkBuffers[RAID];
    VkDeviceSize offsets[RAID] = {};
    
    uint32_t remaining = count;
    uint32_t offset = 0;
    uint32_t currentFirstBinding = firstBinding;
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;
        
        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto& view = pViews[offset + i];
            const auto buffer = static_cast<CVulkanBuffer*>(view.buffer);
            // SIMPLE RTTI
            assert(&buffer->RefDevice() == &m_pCmdPool->RefDevice());
            vkBuffers[i] = buffer->GetHandle();
            offsets[i] = view.offset;
        }
        
        // Execute this raid
        ::vkCmdBindVertexBuffers(m_hCommandBuffer, currentFirstBinding, raidSize, vkBuffers, offsets);
        
        remaining -= raidSize;
        offset += raidSize;
        currentFirstBinding += raidSize;
    }
}

void RHI::CVulkanCmdList::BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, INDEX_TYPE type) noexcept
{
    const auto vkBuffer = static_cast<CVulkanBuffer*>(buffer);
    // SIMPLE RTTI
    assert(&vkBuffer->RefDevice() == &m_pCmdPool->RefDevice());
    
    const VkBuffer hBuffer = vkBuffer->GetHandle();
    const VkIndexType vkIndexType = impl::rhi_vulkan(type);
    
    ::vkCmdBindIndexBuffer(m_hCommandBuffer, hBuffer, offset, vkIndexType);
}

void RHI::CVulkanCmdList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) noexcept
{
    ::vkCmdDraw(m_hCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void RHI::CVulkanCmdList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) noexcept
{
    ::vkCmdDrawIndexed(m_hCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void RHI::CVulkanCmdList::ResourceBarriers(uint32_t count, const ResourceBarrierDesc* pList) noexcept
{
    constexpr uint32_t RAID = 32;
    VkImageMemoryBarrier barriers[RAID];
    
    uint32_t remaining = count;
    uint32_t offset = 0;
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;
        
        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto& desc = pList[offset + i];
            const auto image = static_cast<CVulkanImage*>(desc.resource);
            assert(&image->RefDevice() == &m_pCmdPool->RefDevice());
            auto& barrier = barriers[i];
            barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = impl::rhi_vulkan(desc.before);
            barrier.newLayout = impl::rhi_vulkan(desc.after);
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // TODO:
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // TODO:
            barrier.image = image->GetHandle();
            barrier.subresourceRange.aspectMask = image->GetAllAspectMask();
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;  // TODO:
            barrier.dstAccessMask = 0;  // TODO:
        }
        
        // Execute this raid
        ::vkCmdPipelineBarrier(
            m_hCommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // TODO: derive from oldLayout
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // TODO: derive from newLayout
            0,
            0, nullptr,
            0, nullptr,
            raidSize, barriers
        );
        
        remaining -= raidSize;
        offset += raidSize;
    }
}

void RHI::CVulkanCmdList::CopyBuffer(const CopyBufferDesc* pDesc) noexcept
{
    // TODO: auto barrier?
    assert(pDesc && pDesc->source && pDesc->destination);
    
    const auto srcBuffer = static_cast<CVulkanBuffer*>(pDesc->source);
    const auto dstBuffer = static_cast<CVulkanBuffer*>(pDesc->destination);
    
    // SIMPLE RTTI
    assert(&srcBuffer->RefDevice() == &m_pCmdPool->RefDevice());
    assert(&dstBuffer->RefDevice() == &m_pCmdPool->RefDevice());

    // USAGE CHECK
    assert(srcBuffer->RefDesc().usage.flags & BUFFER_USAGE_TRANSFER_SRC);
    assert(dstBuffer->RefDesc().usage.flags & BUFFER_USAGE_TRANSFER_DST);
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = pDesc->sourceOffset;
    copyRegion.dstOffset = pDesc->destinationOffset;
    copyRegion.size = pDesc->size;
    
    ::vkCmdCopyBuffer(
        m_hCommandBuffer,
        srcBuffer->GetHandle(),
        dstBuffer->GetHandle(),
        1,
        &copyRegion
    );
}

void RHI::CVulkanCmdList::CopyImage(const CopyImageDesc* pDesc) noexcept
{
    assert(!"TODO: CopyImageDesc");
    assert(pDesc && pDesc->source && pDesc->destination);
    
    const auto srcImage = static_cast<CVulkanImage*>(pDesc->source);
    const auto dstImage = static_cast<CVulkanImage*>(pDesc->destination);
    
    // SIMPLE RTTI
    assert(&srcImage->RefDevice() == &m_pCmdPool->RefDevice());
    assert(&dstImage->RefDevice() == &m_pCmdPool->RefDevice());
    
    // USAGE CHECK
    assert(srcImage->RefDesc().usage.flags & IMAGE_USAGE_TRANSFER_SRC);
    assert(dstImage->RefDesc().usage.flags & IMAGE_USAGE_TRANSFER_DST);
    
    // Get image descriptions
    const auto& srcDesc = srcImage->RefDesc();
    const auto& dstDesc = dstImage->RefDesc();
    
    // Format check - only support same format (as requested)
    assert(srcDesc.format == dstDesc.format && "CopyImage only supports same format");
    
    // Setup VkImageCopy structure
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = { 0, 0, 0 };
    
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = { 0, 0, 0 };
    
    copyRegion.extent.width = srcDesc.width;
    copyRegion.extent.height = srcDesc.height;
    copyRegion.extent.depth = 1;
    
    // Execute copy command
    ::vkCmdCopyImage(
        m_hCommandBuffer,
        srcImage->GetHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage->GetHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion
    );
}

void RHI::CVulkanCmdList::CopyBufferToImage(const CopyBufferImageDesc* pDesc) noexcept
{
    assert(pDesc && pDesc->source && pDesc->destination && pDesc->sourcePitch);
    assert(pDesc->destinationWidth > 0 && pDesc->destinationHeight > 0);
    
    const auto srcBuffer = static_cast<CVulkanBuffer*>(pDesc->source);
    const auto dstImage = static_cast<CVulkanImage*>(pDesc->destination);

    // SIMPLE RTTI
    assert(&srcBuffer->RefDevice() == &m_pCmdPool->RefDevice());
    assert(&dstImage->RefDevice() == &m_pCmdPool->RefDevice());
    
    // USAGE CHECK
    assert(srcBuffer->RefDesc().usage.flags & BUFFER_USAGE_TRANSFER_SRC);
    assert(dstImage->RefDesc().usage.flags & IMAGE_USAGE_TRANSFER_DST);
    
    // Get image description
    const auto& imageDesc = dstImage->RefDesc();
    
    // Calculate bytes per pixel for the image format
    const uint32_t bytesPerPixel = impl::format_size(imageDesc.format);
    assert(bytesPerPixel > 0 && "Unknown image format");
    
    // Setup VkBufferImageCopy structure
    VkBufferImageCopy copyRegion{};
    // Use sourceOffset from pDesc
    copyRegion.bufferOffset = pDesc->sourceOffset;
    
    // Calculate bufferRowLength based on sourcePitch and destinationWidth
    const uint32_t expectedPitch = pDesc->destinationWidth * bytesPerPixel;
    if (pDesc->sourcePitch == expectedPitch) {
        // AUTO - pitch matches expected width, no padding
        copyRegion.bufferRowLength = 0;
    } else {
        const auto padding = pDesc->sourcePitch % bytesPerPixel;
        assert(padding == 0 && "sourcePitch must be aligned to bytesPerPixel");
        copyRegion.bufferRowLength = pDesc->sourcePitch / bytesPerPixel;
    }
    // AUTO
    copyRegion.bufferImageHeight = 0;
    
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    
    // Use destination offset and dimensions from pDesc
    copyRegion.imageOffset = { 
        static_cast<int32_t>(pDesc->destinationX), 
        static_cast<int32_t>(pDesc->destinationY), 
        0 
    };
    copyRegion.imageExtent.width = pDesc->destinationWidth;
    copyRegion.imageExtent.height = pDesc->destinationHeight;
    copyRegion.imageExtent.depth = 1;
    
    // Execute copy command
    ::vkCmdCopyBufferToImage(
        m_hCommandBuffer,
        srcBuffer->GetHandle(),
        dstImage->GetHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion
    );

}

void RHI::CVulkanCmdList::SetViewports(uint32_t count, const Viewport* pViewports) noexcept
{
    constexpr uint32_t RAID = 32;
    VkViewport VkViewports[RAID];
    uint32_t remaining = count;
    uint32_t offset = 0;
    uint32_t firstViewport = 0;
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;
        
        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto& srcViewport = pViewports[offset + i];
            auto& dstViewport = VkViewports[i];
            
            dstViewport.x = srcViewport.topLeftX;
            // Vulkan 1.1: negative height - adjust y coordinate
            dstViewport.y = srcViewport.topLeftY + srcViewport.height;
            dstViewport.width = srcViewport.width;
            dstViewport.height = -srcViewport.height;  // Negative height for Vulkan 1.1
            dstViewport.minDepth = srcViewport.minDepth;
            dstViewport.maxDepth = srcViewport.maxDepth;
        }
        
        // Execute this raid
        ::vkCmdSetViewport(m_hCommandBuffer, firstViewport, raidSize, VkViewports);
        
        remaining -= raidSize;
        offset += raidSize;
        firstViewport += raidSize;
    }
}

void RHI::CVulkanCmdList::SetScissors(uint32_t count, const Rect* pRects) noexcept
{
    constexpr uint32_t RAID = 32;
    VkRect2D VkRects[RAID];
    uint32_t remaining = count;
    uint32_t offset = 0;
    uint32_t firstScissor = 0;
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;
        
        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto& srcRect = pRects[offset + i];
            auto& dstRect = VkRects[i];
            
            // Convert Rect to VkRect2D
            dstRect.offset.x = srcRect.left;
            dstRect.offset.y = srcRect.top;
            dstRect.extent.width = static_cast<uint32_t>(srcRect.right - srcRect.left);
            dstRect.extent.height = static_cast<uint32_t>(srcRect.bottom - srcRect.top);
        }
        
        // Execute this raid
        ::vkCmdSetScissor(m_hCommandBuffer, firstScissor, raidSize, VkRects);
        
        remaining -= raidSize;
        offset += raidSize;
        firstScissor += raidSize;
    }
}

namespace Slim { namespace RHI {
    union WriteDescriptorSetInfoUnion {
        VkDescriptorImageInfo   image;
        VkDescriptorBufferInfo  buffer;
        VkBufferView            view;
    };
}}

void RHI::CVulkanCmdList::PushDescriptorSets(IRHIPipelineLayout* pPipeline, uint32_t count, const PushDescriptorSetDesc* pDesc) noexcept
{
    assert(pPipeline && count <= MAX_PUSH_DESCRIPTOR);
    const auto pipeline = static_cast<CVulkanPipelineLayout*>(pPipeline);
    assert(&pipeline->RefDevice() == &m_pCmdPool->RefDevice());
    assert(m_fnCmdPushDescriptorSetKHR && "TODO: fallback");
    VkWriteDescriptorSet writeDescriptorSets[MAX_PUSH_DESCRIPTOR];
    WriteDescriptorSetInfoUnion infos[MAX_PUSH_DESCRIPTOR];


    for (uint32_t i = 0; i != count; ++i) {
        auto& set = writeDescriptorSets[i];
        const auto& desc = pDesc[i];
        set = {};
        set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set.dstSet = VK_NULL_HANDLE;
        set.dstBinding = pipeline->Binding(i);
        set.descriptorCount = 1;
        set.descriptorType = impl::rhi_vulkan(desc.type);
        switch (desc.type)
        {
            CVulkanBuffer* buffer;
            CVulkanAttachment* view;
        case DESCRIPTOR_TYPE_CONST_BUFFER:
            buffer = static_cast<CVulkanBuffer*>(desc.buffer);
            set.pBufferInfo = &infos[i].buffer;
            infos[i].buffer = { buffer->GetHandle(), 0, buffer->RefDesc().size };
            break;
        case DESCRIPTOR_TYPE_SHADER_RESOURCE:
            view = static_cast<CVulkanAttachment*>(desc.image);
            set.pImageInfo = &infos[i].image;
            infos[i].image = { VK_NULL_HANDLE, view->GetHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            break;
        default:
            assert(!"NOTIMPL");
            break;
        }
    }

    m_fnCmdPushDescriptorSetKHR(
        m_hCommandBuffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipeline->GetHandle(), 0, count,
        writeDescriptorSets
    );

}

RHI::CVulkanCmdPool::CVulkanCmdPool(CVulkanDevice* pDevice
    , VkCommandPool pool) noexcept
    : m_pThisDevice(pDevice)
    , m_hDevice(pDevice->GetHandle())
    , m_hCommandPool(pool)
{
    assert(pDevice && pool);
    pDevice->AddRefCnt();
}


RHI::CVulkanCmdPool::~CVulkanCmdPool() noexcept
{
    if (m_hCommandPool != VK_NULL_HANDLE) {
        const auto device = m_pThisDevice->GetHandle();
        ::vkDestroyCommandPool(device, m_hCommandPool, &gc_sAllocationCallbackDRHI);
        m_hCommandPool = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
}


RHI::CODE RHI::CVulkanCmdPool::Reset() noexcept
{
    auto result = ::vkResetCommandPool(m_hDevice, m_hCommandPool, {});
    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkResetCommandPool failed: %u", result);
    }
    return impl::vulkan_rhi(result);
}

RHI::CODE RHI::CVulkanCmdPool::CreateCommandList(IRHICommandList** ppCmdList) noexcept
{
    if (!ppCmdList)
        return CODE_POINTER;

    VkCommandBuffer hCommandBuffer = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;

    // ALLOCATE COMMAND BUFFER
    if (result == VK_SUCCESS) {
        VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool = m_hCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        result = ::vkAllocateCommandBuffers(m_hDevice, &allocInfo, &hCommandBuffer);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkAllocateCommandBuffers failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        const auto obj = new (std::nothrow) CVulkanCmdList{ this, hCommandBuffer };
        *ppCmdList = obj;
        if (obj) {
            hCommandBuffer = VK_NULL_HANDLE; // Object now owns it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    if (hCommandBuffer != VK_NULL_HANDLE) {
        ::vkFreeCommandBuffers(m_hDevice, m_hCommandPool, 1, &hCommandBuffer);
        hCommandBuffer = VK_NULL_HANDLE;
    }

    return impl::vulkan_rhi(result);
}

RHI::CVulkanCmdQueue::CVulkanCmdQueue(CVulkanDevice* device, VkQueue queue) noexcept
    : m_pThisDevice(device)
    , m_hQueue(queue)
{
    assert(device && queue);
    device->AddRefCnt();
}

RHI::CVulkanCmdQueue::~CVulkanCmdQueue() noexcept
{
    //RHI::SafeDispose(m_pMutex);

    if (m_hTimelineChain != VK_NULL_HANDLE && m_pThisDevice) {
        const auto hDevice = m_pThisDevice->GetHandle();
        ::vkDestroySemaphore(hDevice, m_hTimelineChain, &gc_sAllocationCallbackDRHI);
        m_hTimelineChain = VK_NULL_HANDLE;
    }
    
    m_hQueue = VK_NULL_HANDLE;
    RHI::SafeDispose(m_pThisDevice);
}

VkResult Slim::RHI::CVulkanCmdQueue::Init(CVulkanSwapChain* swapchain) noexcept
{
    m_wrSwapChain.ptr = swapchain;
    VkResult result = VK_SUCCESS;

    // TIME LINE CHAIN
    if (result == VK_SUCCESS) {
        const auto hDevice = m_pThisDevice->GetHandle();

        VkSemaphoreTypeCreateInfo timelineCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = m_u64ChainValue;

        VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        result = ::vkCreateSemaphore(hDevice, &createInfo, &gc_sAllocationCallbackDRHI, &m_hTimelineChain);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateSemaphore failed: %u", result);
        }
    }

    return result;
}

void RHI::CVulkanCmdQueue::AfterAcquire() noexcept
{
    // First Wait = imageAvailable
    assert(m_wrSwapChain.ptr);
    auto& frame = m_wrSwapChain.ptr->CurrentFlight();
    m_hNextWaitRef = frame.imageAvailable;
    //m_u64NextWaitValue = 0;
    //m_eNextWaitFlag = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}

void RHI::CVulkanCmdQueue::BeforePresent() noexcept
{
    //m_bFirstFlag = false;
    //if (m_bLastFlag) {
    //    m_bLastFlag = false;
    //    return;
    //}

    assert(m_wrSwapChain.ptr);
    const auto& frameFt = m_wrSwapChain.ptr->CurrentFlight();
    const auto& frameSc = m_wrSwapChain.ptr->CurrentSwapchain();

    // Wait for timeline semaphore to reach current chain value
    VkTimelineSemaphoreSubmitInfo timelineInfo { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
    VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.pNext = &timelineInfo;

    // Wait for timeline chain to reach current value
    VkSemaphore waitList[] = { m_hTimelineChain };
    VkPipelineStageFlags waitFlags[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
    uint64_t waitValues[] = { m_u64ChainValue };

    // Signal renderFinished semaphore (binary semaphore, value is 0)
    VkSemaphore signalList[] = { frameSc.renderFinished };
    uint64_t signalValues[] = { 0 };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitList;
    submitInfo.pWaitDstStageMask = waitFlags;

    timelineInfo.waitSemaphoreValueCount = 1;
    timelineInfo.pWaitSemaphoreValues = waitValues;

    // Signal renderFinished semaphore before present
    submitInfo.commandBufferCount = 0;
    submitInfo.pCommandBuffers = nullptr;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalList;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = signalValues;

    // Submit with fence to trigger frame.inFlight
    //m_pMutex->Lock();
    const auto result = ::vkQueueSubmit(m_hQueue, 1, &submitInfo, frameFt.inFlight);
    //m_pMutex->Unlock();
    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("BeforePresent-vkQueueSubmit failed %u", result);
    }
}

//bool RHI::CVulkanCmdQueue::check_first(SUBMIT_HINT) noexcept
//{
//    if (!m_bFirstFlag) {
//        m_bFirstFlag = true;
//        return true;
//    }
//    return false;
//}

RHI::CODE RHI::CVulkanCmdQueue::Submit(uint32_t count, IRHICommandList* const* list, const SubmitSynch* synch) noexcept
{
    assert(m_hQueue);
    //const auto first = this->check_first(hints);
    uint32_t bufferNeeded = 0;
    // bufferData
    // ALIGN SORT: [VkCommandBuffer]
    bufferNeeded = uint32_t(sizeof(VkCommandBuffer)) * count;

    // CALCULATE BUFFER SIZE
    if (bufferNeeded) {
        if (!m_vLocalBuffer.resize(bufferNeeded))
            return CODE_OUTOFMEMORY;
    }

    VkTimelineSemaphoreSubmitInfo timelineInfo { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
    VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

    VkSemaphore waitList[SubmitSynch::MAX_SIGNAL_FENCE_COUNT + 2];
    VkPipelineStageFlags waitFlags[SubmitSynch::MAX_SIGNAL_FENCE_COUNT + 2];
    uint64_t waitValues[SubmitSynch::MAX_SIGNAL_FENCE_COUNT + 2];

    VkSemaphore signalList[SubmitSynch::MAX_SIGNAL_FENCE_COUNT + 1];
    uint64_t signalValues[SubmitSynch::MAX_SIGNAL_FENCE_COUNT + 1];

    submitInfo.pWaitSemaphores = waitList;
    submitInfo.pWaitDstStageMask = waitFlags;
    submitInfo.pSignalSemaphores = signalList;
    submitInfo.pNext = &timelineInfo;

    timelineInfo.pSignalSemaphoreValues = signalValues;
    timelineInfo.pWaitSemaphoreValues = waitValues;

    uint32_t signalListLen = 0;
    uint32_t waitListLen = 0;

    VkFence fence = VK_NULL_HANDLE;


    // WAIT PREV CHAIN
    //{
    //    waitList[waitListLen] = m_hTimelineChain;
    //    waitValues[waitListLen] = m_u64ChainValue;
    //    waitFlags[waitListLen] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    //    ++waitListLen;
    //}
    // WAIT FRAME
    if (m_hNextWaitRef) {
        waitList[waitListLen] = m_hNextWaitRef;
        m_hNextWaitRef = VK_NULL_HANDLE;
        waitValues[waitListLen] = 0;
        waitFlags[waitListLen] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ++waitListLen;
    }
    // SIGNAL NEXT CHAIN
    //{
    //    signalList[signalListLen] = m_hTimelineChain;
    //    ++m_u64ChainValue;
    //    signalValues[signalListLen] = m_u64ChainValue;
    //    ++signalListLen;
    //}

    // Next Wait
#if 0
    {
        m_hNextWaitRef = m_hTimelineChain;
        m_u64NextWaitValue = m_u64ChainValue;
        // Use ALL_COMMANDS_BIT as safe default for external fence waits
        m_eNextWaitFlag = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
#endif
    //fence = frame.inFlight;



    // Handle synchronization fences from synch parameter
    if (synch) {
        // Add wait semaphores from synch->waitPairs
        if (synch->waitCount > 0) {
            assert(synch->waitCount <= SubmitSynch::MAX_SIGNAL_FENCE_COUNT);
            const uint32_t waitCountToAdd = synch->waitCount;
            
            for (uint32_t i = 0; i != waitCountToAdd; ++i) {
                const auto& waitPair = synch->waitPairs[i];
                const auto fence = static_cast<CVulkanFence*>(waitPair.fence);
                // SIMPLE RTTI
                assert(&fence->RefDevice() == m_pThisDevice);
                waitList[waitListLen] = fence->GetHandle();
                waitValues[waitListLen] = waitPair.value;
                // Use ALL_COMMANDS_BIT as safe default for external fence waits
                waitFlags[waitListLen] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                ++waitListLen;
            }
        }

        // Add signal semaphores from synch->signalPairs
        if (synch->signalCount > 0) {
            assert(synch->signalCount <= SubmitSynch::MAX_SIGNAL_FENCE_COUNT);
            const uint32_t signalCountToAdd = synch->signalCount;
            
            for (uint32_t i = 0; i != signalCountToAdd; ++i) {
                const auto& signalPair = synch->signalPairs[i];
                const auto fence = static_cast<CVulkanFence*>(signalPair.fence);
                // SIMPLE RTTI
                assert(&fence->RefDevice() == m_pThisDevice);
                signalList[signalListLen] = fence->GetHandle();
                signalValues[signalListLen] = signalPair.value;
                ++signalListLen;
            }
        }
    }

    submitInfo.waitSemaphoreCount = waitListLen;
    submitInfo.signalSemaphoreCount = signalListLen;

    timelineInfo.waitSemaphoreValueCount = waitListLen;
    timelineInfo.signalSemaphoreValueCount = signalListLen;


    assert(count == 1);
    submitInfo.commandBufferCount = count;
    const auto buffer = reinterpret_cast<VkCommandBuffer*>(m_vLocalBuffer.data());
    for (uint32_t i = 0; i != count; ++i) {
        const auto vcmdlist = static_cast<CVulkanCmdList*>(list[i]);
        const auto cmdbuf = vcmdlist->GetHandle();
        buffer[i] = cmdbuf;
    }
    submitInfo.pCommandBuffers = buffer;

    //m_pMutex->Lock();
    const auto result = ::vkQueueSubmit(m_hQueue, 1, &submitInfo, fence);
    //m_pMutex->Unlock();

    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkQueueSubmit failed: %u", result);
    }

    return impl::vulkan_rhi(result);


#if 0
    // vkQueueSubmit2?

    const auto result = ::vkQueueSubmit2(m_hQueue, 1, &submitInfo, VK_NULL_HANDLE);
    
    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkQueueSubmit2 failed: %u", result);
    }

    return impl::vulkan_rhi(result);
#endif


}

RHI::CODE RHI::CVulkanCmdQueue::WaitIdle() noexcept
{
    assert(m_hQueue);
    const auto result = ::vkQueueWaitIdle(m_hQueue);
    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkQueueWaitIdle failed: %u", result);
    }
    return impl::vulkan_rhi(result);
}

#endif