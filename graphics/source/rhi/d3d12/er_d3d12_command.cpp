#include "er_d3d12_command.h"
#ifndef SLIM_RHI_NO_D3D12
#include <cassert>
#include <cstdio>
#include "er_d3d12_device.h"
#include "er_d3d12_pipeline.h"
#include "er_d3d12_resource.h"
#include "er_d3d12_descriptor.h"
#include "er_rhi_d3d12.h"
#include "../common/er_rhi_common.h"
#include <d3d12.h>
using namespace Slim;

RHI::CD3D12CommandPool::CD3D12CommandPool(CD3D12Device* pDevice
    , ID3D12CommandAllocator* pCommandAllocator
    , uint32_t commandListType) noexcept
    : m_pDevice(pDevice)
    , m_pCommandAllocator(pCommandAllocator)
    , m_eCommandListType(commandListType)
{
    assert(pDevice && pCommandAllocator);
    pDevice->AddRefCnt();
    pCommandAllocator->AddRef();
}

RHI::CD3D12CommandPool::~CD3D12CommandPool() noexcept
{
    RHI::SafeRelease(m_pCommandAllocator);
    RHI::SafeDispose(m_pDevice);
}

RHI::CODE RHI::CD3D12CommandPool::Reset() noexcept
{
    auto hr = m_pCommandAllocator->Reset();
    if (FAILED(hr)) {
        DRHI_LOGGER_ERR("Allocator Reset failed: %u", hr);
    }
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12CommandPool::CreateCommandList(IRHICommandList** ppCmdList) noexcept
{
    if (!ppCmdList)
        return CODE_POINTER;

    assert(m_pDevice && m_pCommandAllocator);

    ID3D12GraphicsCommandList4* pCommandList = nullptr;
    CD3D12CommandList* pRHICommandList = nullptr;
    HRESULT hr = S_OK;

    // CREATE D3D12 COMMAND LIST (using CreateCommandList4 to support Render Pass)
    if (SUCCEEDED(hr)) {
        hr = m_pDevice->RefDevice().CreateCommandList1(
            0,  // node mask
            D3D12_COMMAND_LIST_TYPE(m_eCommandListType),
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_ID3D12GraphicsCommandList4, reinterpret_cast<void**>(&pCommandList)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateCommandList1 failed: %u", hr);
        }
        // Note: Command list created in closed state, will be reset when needed
    }

    // CREATE RHI COMMAND LIST
    if (SUCCEEDED(hr)) {
        pRHICommandList = new (std::nothrow) CD3D12CommandList{ this, pCommandList };
        *ppCmdList = nullptr;
        if (pRHICommandList) {
            *ppCmdList = pRHICommandList;
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }

    // CLEAN UP
    RHI::SafeRelease(pCommandList);
    return impl::d3d12_rhi(hr);
}

RHI::CD3D12CommandList::CD3D12CommandList(CD3D12CommandPool* pPool, ID3D12GraphicsCommandList4* pCommandList) noexcept
    : m_pCommandPool(pPool)
    , m_pCommandList(pCommandList)
{
    assert(pPool && pCommandList);
    pPool->AddRefCnt();
    pCommandList->AddRef();
}

RHI::CD3D12CommandList::~CD3D12CommandList() noexcept
{
    RHI::SafeRelease(m_pCommandList);
    RHI::SafeDispose(m_pCommandPool);
}

RHI::CODE RHI::CD3D12CommandList::Begin() noexcept
{
    // TODO: pInitialState
    HRESULT hr = m_pCommandList->Reset(m_pCommandPool->RefAllocator(), nullptr);
    if (FAILED(hr)) {
        DRHI_LOGGER_ERR("CommandList Reset failed: %u", hr);
    }
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12CommandList::End() noexcept
{
    HRESULT hr = m_pCommandList->Close();
    if (FAILED(hr)) {
        DRHI_LOGGER_ERR("CommandList Close failed: %u", hr);
    }
    return impl::d3d12_rhi(hr);
}

void RHI::CD3D12CommandList::BeginRenderPass(const BeginRenderPassDesc* pDesc) noexcept
{
#if 0
    assert(pDesc && pDesc->renderpass && pDesc->framebuffer);
    
    const auto renderPass = static_cast<CD3D12RenderPass*>(pDesc->renderpass);
    const auto framebuffer = static_cast<CD3D12FrameBuffer*>(pDesc->framebuffer);
    
    assert(&renderPass->RefDevice() == &m_pCommandPool->RefDevice());
    assert(&framebuffer->RefDevice() == &m_pCommandPool->RefDevice());
    
    const auto& rpDesc = renderPass->RefDesc();
    const auto& fbDesc = framebuffer->RefDesc();

    // Prepare render pass parameters
    D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDescs[8] = {};
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDsvDesc = nullptr;
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsvDesc = {};
    
    const uint32_t colorCount = rpDesc.colorAttachmentCount;
    
    // Convert load/store operations
    for (uint32_t i = 0; i < colorCount; ++i) {
        const auto& colorAttach = rpDesc.colorAttachment[i];
        const auto view = static_cast<CD3D12Attachment*>(fbDesc.colorViews[i]);
        assert(&view->RefDevice() == &m_pCommandPool->RefDevice());

        rtvDescs[i].cpuDescriptor = { view->GetAddress() };
        rtvDescs[i].BeginningAccess.Type = impl::rhi_d3d12(colorAttach.load);
        rtvDescs[i].EndingAccess.Type = impl::rhi_d3d12(colorAttach.store);
        
        if (colorAttach.load == RHI::PASS_LOAD_OP_CLEAR) {
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Format = impl::rhi_d3d12(rpDesc.colorAttachment[i].format);
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[0] = 0.0f;
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[1] = 0.0f;
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[2] = 0.0f;
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[3] = 1.0f;
        }
    }

    // Handle depth stencil if present
    if (fbDesc.depthStencil) {
        const auto& dsAttach = rpDesc.depthStencil;
        dsvDesc.cpuDescriptor = {};
        dsvDesc.DepthBeginningAccess.Type = impl::rhi_d3d12(dsAttach.depthLoad);
        dsvDesc.DepthEndingAccess.Type = impl::rhi_d3d12(dsAttach.depthStore);
        dsvDesc.StencilBeginningAccess.Type = impl::rhi_d3d12(dsAttach.stencilLoad);
        dsvDesc.StencilEndingAccess.Type = impl::rhi_d3d12(dsAttach.stencilStore);
        
        if (dsAttach.depthLoad == RHI::PASS_LOAD_OP_CLEAR || dsAttach.stencilLoad == RHI::PASS_LOAD_OP_CLEAR) {
            dsvDesc.DepthBeginningAccess.Clear.ClearValue.Format = impl::rhi_d3d12(dsAttach.format);
            dsvDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = 1.0f;
            dsvDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = 0;
        }
        pDsvDesc = &dsvDesc;
    }
    
    m_pCommandList->BeginRenderPass(colorCount, rtvDescs, pDsvDesc, D3D12_RENDER_PASS_FLAG_NONE);
#else
    // dynamic rendering
    assert(pDesc);
    
    // Prepare render pass parameters
    D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDescs[8] = {};
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDsvDesc = nullptr;
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsvDesc = {};
    
    const uint32_t colorCount = pDesc->colorCount;
    assert(colorCount <= 8);
    
    // Setup color attachments
    for (uint32_t i = 0; i < colorCount; ++i) {
        const auto& colorDesc = pDesc->colors[i];
        const auto view = static_cast<CD3D12Attachment*>(colorDesc.attachment);
        assert(&view->RefDevice() == &m_pCommandPool->RefDevice());
        
        rtvDescs[i].cpuDescriptor = { view->GetCpuAddress() };
        rtvDescs[i].BeginningAccess.Type = impl::rhi_d3d12(colorDesc.load);
        rtvDescs[i].EndingAccess.Type = impl::rhi_d3d12(colorDesc.store);
        
        //if (colorDesc.load == RHI::PASS_LOAD_OP_CLEAR) 
        {
            const auto& viewDesc = view->RefDesc();
            const auto& imageDesc = view->RefImage().RefDesc();
            const auto format = (viewDesc.format != RHI::RHI_FORMAT_UNKNOWN) ? viewDesc.format : imageDesc.format;
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Format = impl::rhi_d3d12(format);
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[0] = colorDesc.clear[0];
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[1] = colorDesc.clear[1];
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[2] = colorDesc.clear[2];
            rtvDescs[i].BeginningAccess.Clear.ClearValue.Color[3] = colorDesc.clear[3];
        }
    }
    
    // Handle depth stencil if present
    if (pDesc->depthStencil.attachment) {
        //assert(!"NOTIMPL");
        const auto& dsDesc = pDesc->depthStencil;
        const auto dsView = static_cast<CD3D12Attachment*>(dsDesc.attachment);
        assert(&dsView->RefDevice() == &m_pCommandPool->RefDevice());
        
        dsvDesc.cpuDescriptor = { dsView->GetCpuAddress() };
        dsvDesc.DepthBeginningAccess.Type = impl::rhi_d3d12(dsDesc.depthLoad);
        dsvDesc.DepthEndingAccess.Type = impl::rhi_d3d12(dsDesc.depthStore);
        dsvDesc.StencilBeginningAccess.Type = impl::rhi_d3d12(dsDesc.stencilLoad);
        dsvDesc.StencilEndingAccess.Type = impl::rhi_d3d12(dsDesc.stencilStore);
        
        //if (dsDesc.depthLoad == RHI::PASS_LOAD_OP_CLEAR || dsDesc.stencilLoad == RHI::PASS_LOAD_OP_CLEAR)
        {
            const auto& viewDesc = dsView->RefDesc();
            const auto& imageDesc = dsView->RefImage().RefDesc();
            const auto format = (viewDesc.format != RHI::RHI_FORMAT_UNKNOWN) ? viewDesc.format : imageDesc.format;
            dsvDesc.DepthBeginningAccess.Clear.ClearValue.Format = impl::rhi_d3d12(format);
            dsvDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = dsDesc.depthClear;
            dsvDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = dsDesc.stencilClear;
        }
        
        pDsvDesc = &dsvDesc;
    }
    
    m_pCommandList->BeginRenderPass(colorCount, rtvDescs, pDsvDesc, D3D12_RENDER_PASS_FLAG_NONE);
#endif

}

void RHI::CD3D12CommandList::EndRenderPass() noexcept
{
    m_pCommandList->EndRenderPass();
}

void RHI::CD3D12CommandList::BindPipeline(IRHIPipeline* pPipeline) noexcept
{
    assert(pPipeline);
    const auto pipeline = static_cast<CD3D12Pipeline*>(pPipeline);
    assert(&pipeline->RefDevice() == &m_pCommandPool->RefDevice());
    m_pCommandList->SetPipelineState(&pipeline->RefPipelineState());
    m_pCommandList->SetGraphicsRootSignature(&pipeline->RefRootSignature());
    // TODO: READ DATA FROM pipeline
    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RHI::CD3D12CommandList::BindDescriptorBuffer(IRHIPipelineLayout* pLayout, IRHIDescriptorBuffer* pDescriptorBuffer) noexcept
{
    constexpr uint32_t RAID = 32;
    assert(pLayout && pDescriptorBuffer);
    const auto layout = static_cast<CD3D12PipelineLayout*>(pLayout);
    // SIMPLE RTTI
    const auto descBuffer = static_cast<CD3D12DescriptorBuffer*>(pDescriptorBuffer);
    assert(&layout->RefDevice() == &m_pCommandPool->RefDevice());
    assert(&descBuffer->RefDevice() == &m_pCommandPool->RefDevice());

    ID3D12DescriptorHeap* const szHeaps[] = { 
        descBuffer->NormalHeap(),
        descBuffer->SamplerHeap(),
    };

    assert(szHeaps[0] || szHeaps[1]);


    // SETUP DESCRIPTOR HEAP
    {
        uint32_t count = 0;
        if (szHeaps[0]) count++;
        if (szHeaps[1]) count++;
        m_pCommandList->SetDescriptorHeaps(count, szHeaps[0] ? szHeaps : szHeaps + 1);
    }


    uint32_t rootParameterIndex = layout->DescriptorBufferBase();
    // SRV/CBV/UAV Heap
    if (const auto heap = szHeaps[0]) {
        const auto gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
        m_pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
        rootParameterIndex++;
    }
    // SAMPLER
    if (const auto heap = szHeaps[1]) {
        const auto gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
        m_pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
        rootParameterIndex++;
    }
}

void RHI::CD3D12CommandList::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const VertexBufferView* pViews) noexcept
{
    constexpr uint32_t RAID = 32;
    D3D12_VERTEX_BUFFER_VIEW d3dViews[RAID];
    
    uint32_t remaining = count;
    uint32_t offset = 0;
    uint32_t currentFirstBinding = firstBinding;
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;
        
        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto& view = pViews[offset + i];
            const auto buffer = static_cast<CD3D12Buffer*>(view.buffer);
            // SIMPLE RTTI
            assert(&buffer->RefDevice() == &m_pCommandPool->RefDevice());
            
            d3dViews[i] = {};
            d3dViews[i].BufferLocation = buffer->GetGpuAddress() + view.offset;
            // TODO: as a parameter for SizeInBytes?
            // TODO: over 4GB?
            d3dViews[i].SizeInBytes = static_cast<uint32_t>(buffer->GetSize());
            // Storing stride info in the PSO is possible but increases coupling. 
            // Instead, it's required as a parameter.
            d3dViews[i].StrideInBytes = view.stride;
        }
        
        // Execute this raid
        m_pCommandList->IASetVertexBuffers(currentFirstBinding, raidSize, d3dViews);
        
        remaining -= raidSize;
        offset += raidSize;
        currentFirstBinding += raidSize;
    }
}

void RHI::CD3D12CommandList::BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, INDEX_TYPE type) noexcept
{
    assert(buffer);
    const auto d3dBuffer = static_cast<CD3D12Buffer*>(buffer);
    // SIMPLE RTTI
    assert(&d3dBuffer->RefDevice() == &m_pCommandPool->RefDevice());
    
    D3D12_INDEX_BUFFER_VIEW view = {};
    view.BufferLocation = d3dBuffer->GetGpuAddress() + offset;
    view.SizeInBytes = static_cast<uint32_t>(d3dBuffer->GetSize() - offset);
    view.Format = impl::rhi_d3d12(type);
    
    m_pCommandList->IASetIndexBuffer(&view);
}

void RHI::CD3D12CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) noexcept
{
    m_pCommandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RHI::CD3D12CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) noexcept
{
    m_pCommandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void RHI::CD3D12CommandList::ResourceBarriers(uint32_t count, const ResourceBarrierDesc* pList) noexcept
{
    constexpr uint32_t RAID = 32;
    D3D12_RESOURCE_BARRIER d3dResourceBarrier[RAID];
    
    uint32_t remaining = count;
    uint32_t offset = 0;

    // TODO: D3D12_TEXTURE_BARRIER_FLAG_DISCARD, COMMOND != UNDEFINED
    // TODO: UNDEFINED -> read current state?
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;
        
        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto& desc = pList[offset + i];
            const auto image = static_cast<CD3D12Image*>(desc.resource);
            assert(&image->RefDevice() == &m_pCommandPool->RefDevice());

            d3dResourceBarrier[i] = {};
            d3dResourceBarrier[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            d3dResourceBarrier[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            d3dResourceBarrier[i].Transition.pResource = &image->RefResource();
            d3dResourceBarrier[i].Transition.StateBefore = impl::rhi_d3d12(desc.before);
            d3dResourceBarrier[i].Transition.StateAfter = impl::rhi_d3d12(desc.after);
            d3dResourceBarrier[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        
        // Execute this raid
        m_pCommandList->ResourceBarrier(raidSize, d3dResourceBarrier);
        
        remaining -= raidSize;
        offset += raidSize;
    }
}

void RHI::CD3D12CommandList::CopyBuffer(const CopyBufferDesc* pDesc) noexcept
{
    assert(pDesc && pDesc->source && pDesc->destination);
    
    const auto srcBuffer = static_cast<CD3D12Buffer*>(pDesc->source);
    const auto dstBuffer = static_cast<CD3D12Buffer*>(pDesc->destination);
    
    // SIMPLE RTTI
    assert(&srcBuffer->RefDevice() == &m_pCommandPool->RefDevice());
    assert(&dstBuffer->RefDevice() == &m_pCommandPool->RefDevice());
    
    // USAGE CHECK
    assert(srcBuffer->RefDesc().usage.flags & BUFFER_USAGE_TRANSFER_SRC);
    assert(dstBuffer->RefDesc().usage.flags & BUFFER_USAGE_TRANSFER_DST);

    // Get state of destination buffer
    const auto dstInitialState = static_cast<D3D12_RESOURCE_STATES>(dstBuffer->GetResState());
    const auto copyDestState = D3D12_RESOURCE_STATE_COPY_DEST;

    // REF: impl::rhi_d3d12_cvt
    // Transition destination buffer back to initial state if needed
    if (dstInitialState != copyDestState) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = &dstBuffer->RefResource();
        barrier.Transition.StateBefore = dstInitialState;
        barrier.Transition.StateAfter = copyDestState;
        barrier.Transition.Subresource = 0;
        m_pCommandList->ResourceBarrier(1, &barrier);
    }

    m_pCommandList->CopyBufferRegion(
        &dstBuffer->RefResource(),
        pDesc->destinationOffset,
        &srcBuffer->RefResource(),
        pDesc->sourceOffset,
        pDesc->size
    );

    if (dstInitialState != copyDestState) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = &dstBuffer->RefResource();
        barrier.Transition.StateBefore = copyDestState;
        barrier.Transition.StateAfter = dstInitialState;
        barrier.Transition.Subresource = 0;
        m_pCommandList->ResourceBarrier(1, &barrier);
    }
}

void RHI::CD3D12CommandList::CopyImage(const CopyImageDesc* pDesc) noexcept
{
    assert(!"TODO");
}

void RHI::CD3D12CommandList::CopyBufferToImage(const CopyBufferImageDesc* pDesc) noexcept
{
    assert(pDesc && pDesc->source && pDesc->destination && pDesc->sourcePitch);
    assert(pDesc->destinationWidth > 0 && pDesc->destinationHeight > 0);
    
    const auto srcBuffer = static_cast<CD3D12Buffer*>(pDesc->source);
    const auto dstImage = static_cast<CD3D12Image*>(pDesc->destination);

    // SIMPLE RTTI
    assert(&srcBuffer->RefDevice() == &m_pCommandPool->RefDevice());
    assert(&dstImage->RefDevice() == &m_pCommandPool->RefDevice());
    
    // USAGE CHECK
    assert(srcBuffer->RefDesc().usage.flags & BUFFER_USAGE_TRANSFER_SRC);
    assert(dstImage->RefDesc().usage.flags & IMAGE_USAGE_TRANSFER_DST);
    
    // Get image description
    const auto& imageDesc = dstImage->RefDesc();
    
    // Calculate bytes per pixel for the image format
    const uint32_t bytesPerPixel = impl::format_size(imageDesc.format);
    const auto dxfmt = impl::rhi_d3d12(imageDesc.format);
    assert(bytesPerPixel > 0 && "Unknown image format");
    
    // Get image resource description for format
    //D3D12_RESOURCE_DESC resourceDesc;
    //impl::fuckms(&dstImage->RefResource(), &resourceDesc);
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Offset = pDesc->sourceOffset;
    footprint.Footprint.Format = dxfmt;// resourceDesc.Format;
    footprint.Footprint.Width = pDesc->destinationWidth;
    footprint.Footprint.Height = pDesc->destinationHeight;
    footprint.Footprint.Depth = 1;
    footprint.Footprint.RowPitch = pDesc->sourcePitch;
    
    // Setup source location (buffer with PLACED_FOOTPRINT)
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = footprint;
    srcLocation.pResource = &srcBuffer->RefResource();
    
    // Setup destination location (texture with SUBRESOURCE_INDEX)
    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;
    dstLocation.pResource = &dstImage->RefResource();
    
    // Execute copy command
    m_pCommandList->CopyTextureRegion(
        &dstLocation,
        pDesc->destinationX,  // DstX
        pDesc->destinationY,   // DstY
        0,                    // DstZ
        &srcLocation,
        nullptr  // Box (nullptr means copy entire source)
    );
    
    // Note: Caller should transition destination image from COPY_DEST to final state
    // via ResourceBarrier after calling this function.
}

void RHI::CD3D12CommandList::SetViewports(uint32_t count, const Viewport* pViewports) noexcept
{
    static_assert(sizeof(Viewport) == sizeof(D3D12_VIEWPORT), "Viewport size mismatch");
    //static_assert(offsetof(Viewport, x) == offsetof(VkViewport, x), "Viewport layout mismatch");
    //static_assert(offsetof(Viewport, y) == offsetof(VkViewport, y), "Viewport layout mismatch");
    //static_assert(offsetof(Viewport, width) == offsetof(VkViewport, width), "Viewport layout mismatch");
    //static_assert(offsetof(Viewport, height) == offsetof(VkViewport, height), "Viewport layout mismatch");
    //static_assert(offsetof(Viewport, minDepth) == offsetof(VkViewport, minDepth), "Viewport layout mismatch");
    //static_assert(offsetof(Viewport, maxDepth) == offsetof(VkViewport, maxDepth), "Viewport layout mismatch");
    m_pCommandList->RSSetViewports(count, reinterpret_cast<const D3D12_VIEWPORT*>(pViewports));
}

void RHI::CD3D12CommandList::SetScissors(uint32_t count, const Rect* pRects) noexcept
{
    static_assert(sizeof(Rect) == sizeof(D3D12_RECT), "Rect size mismatch");
    // static_assert(offsetof(Rect, left) == offsetof(D3D12_RECT, left), "Rect layout mismatch");
    // static_assert(offsetof(Rect, top) == offsetof(D3D12_RECT, top), "Rect layout mismatch");
    // static_assert(offsetof(Rect, right) == offsetof(D3D12_RECT, right), "Rect layout mismatch");
    // static_assert(offsetof(Rect, bottom) == offsetof(D3D12_RECT, bottom), "Rect layout mismatch");
    m_pCommandList->RSSetScissorRects(count, reinterpret_cast<const D3D12_RECT*>(pRects));
}

void RHI::CD3D12CommandList::PushDescriptorSets(IRHIPipelineLayout* pLayout, uint32_t count, const PushDescriptorSetDesc* pDesc) noexcept
{
    assert(pLayout && count > 0 && pDesc);
    
    const auto pipelineLayout = static_cast<CD3D12PipelineLayout*>(pLayout);
    assert(&pipelineLayout->RefDevice() == &m_pCommandPool->RefDevice());
    
    for (uint32_t i = 0; i < count; ++i) {
        const auto& desc = pDesc[i];
        assert(desc.buffer);
        
        const auto buffer = static_cast<CD3D12Buffer*>(desc.buffer);
        assert(&buffer->RefDevice() == &m_pCommandPool->RefDevice());
        
        // Get GPU address of the buffer
        const auto gpuAddress = buffer->GetGpuAddress();

        const uint32_t rootParameterIndex = pipelineLayout->PushDescriptorBase() + i;
        
        // Set the root descriptor based on type
        switch (desc.type) 
        {
        case RHI::DESCRIPTOR_TYPE_CONST_BUFFER:
            m_pCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, gpuAddress);
            break;
            
        case RHI::DESCRIPTOR_TYPE_SHADER_RESOURCE:
            m_pCommandList->SetGraphicsRootShaderResourceView(rootParameterIndex, gpuAddress);
            break;
            
        case RHI::DESCRIPTOR_TYPE_UNORDERED_ACCESS:
            m_pCommandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, gpuAddress);
            break;
            
        default:
            assert(false && "Unsupported descriptor type for push descriptors");
            break;
        }
    }
}

RHI::CD3D12CommandQueue::CD3D12CommandQueue(CD3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) noexcept
    : m_pDevice(pDevice)
    , m_pCommandQueue(pCommandQueue)
{
    assert(pDevice && pCommandQueue);
    pDevice->AddRefCnt();
    pCommandQueue->AddRef();
}

RHI::CD3D12CommandQueue::~CD3D12CommandQueue() noexcept
{
    if (m_hIdleEvent) {
        ::CloseHandle(m_hIdleEvent);
        m_hIdleEvent = nullptr;
    }
    RHI::SafeRelease(m_pIdleFence);
    RHI::SafeRelease(m_pCommandQueue);
    RHI::SafeDispose(m_pDevice);
}

RHI::CODE RHI::CD3D12CommandQueue::Submit(uint32_t count, IRHICommandList* const* list, const SubmitSynch* synch) noexcept
{
    HRESULT hr = S_OK;

    // Wait for fences before executing command lists
    if (synch && synch->waitCount > 0) {
        assert(synch->waitCount <= SubmitSynch::MAX_SIGNAL_FENCE_COUNT);
        const auto waitCount = synch->waitCount;
        for (uint32_t i = 0; i < waitCount; ++i) {
            const auto& waitPair = synch->waitPairs[i];
            assert(waitPair.fence);
            const auto fence = static_cast<CD3D12Fence*>(waitPair.fence);
            // SIMPLE RTTI
            assert(&fence->RefDevice() == m_pDevice);
            const auto thr = m_pCommandQueue->Wait(fence->GetHandle(), waitPair.value);
            if (FAILED(thr)) {
                hr = thr;
                DRHI_LOGGER_ERR("CommandQueue Wait failed: %u", thr);
            }
        }
    }
    
    // Convert all command lists to D3D12 command lists and execute in raides
    constexpr uint32_t RAID = 32;
    ID3D12CommandList* szpD3d12CmdList[RAID];
    uint32_t remaining = count;
    uint32_t offset = 0;
    
    while (remaining > 0) {
        const uint32_t raidSize = (remaining > RAID) ? RAID : remaining;

        for (uint32_t i = 0; i < raidSize; ++i) {
            const auto cmdList = static_cast<CD3D12CommandList*>(list[offset + i]);
            // SIMPLE RTTI
            //assert(cmdList->RefCommandList);
            szpD3d12CmdList[i] = &cmdList->RefCommandList();
        }
        
        // Execute this raid
        m_pCommandQueue->ExecuteCommandLists(raidSize, szpD3d12CmdList);
        
        remaining -= raidSize;
        offset += raidSize;
    }
    
    // Signal fences after executing all command lists
    if (synch && synch->signalCount > 0) {
        assert(synch->signalCount <= SubmitSynch::MAX_SIGNAL_FENCE_COUNT);
        const auto signalCount = synch->signalCount;
        for (uint32_t i = 0; i < signalCount; ++i) {
            const auto& signalPair = synch->signalPairs[i];
            assert(signalPair.fence);
            const auto fence = static_cast<CD3D12Fence*>(signalPair.fence);
            // SIMPLE RTTI
            assert(&fence->RefDevice() == m_pDevice);
            const auto thr = m_pCommandQueue->Signal(fence->GetHandle(), signalPair.value);
            if (FAILED(thr)) {
                hr = thr;
                DRHI_LOGGER_ERR("CommandQueue Signal failed: %u", thr);
                break;
            }
        }
    }

    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12CommandQueue::WaitIdle() noexcept
{
    assert(m_pCommandQueue && m_pDevice);
    
    HRESULT hr = S_OK;
    
    // Lazy create the idle fence if it doesn't exist
    if (!m_pIdleFence) {
        hr = m_pDevice->RefDevice().CreateFence(
            m_uIdleValue,
            D3D12_FENCE_FLAG_NONE,
            IID_ID3D12Fence,
            reinterpret_cast<void**>(&m_pIdleFence)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateFence failed: %u", hr);
        }
    }
    // Lazy create the idle event if it doesn't exist
    if (!m_hIdleEvent) {
        m_hIdleEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_hIdleEvent) {
            hr = HRESULT_FROM_WIN32(::GetLastError());
            DRHI_LOGGER_ERR("CreateEventW failed: %u", hr);
        }
    }
    // Increment the idle value and signal the fence
    if (SUCCEEDED(hr)) {
        ++m_uIdleValue;
        hr = m_pCommandQueue->Signal(m_pIdleFence, m_uIdleValue);
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CommandQueue Signal failed: %u", hr);
        }
    }
    
    // Wait for the fence to complete
    if (SUCCEEDED(hr)) {
        if (m_pIdleFence->GetCompletedValue() < m_uIdleValue) {
            hr = m_pIdleFence->SetEventOnCompletion(m_uIdleValue, m_hIdleEvent);
            if (SUCCEEDED(hr)) {
                ::WaitForSingleObject(m_hIdleEvent, INFINITE);
            }
            else {
                DRHI_LOGGER_ERR("SetEventOnCompletion failed: %u", hr);
            }
        }
    }
    
    return impl::d3d12_rhi(hr);
}

#endif

