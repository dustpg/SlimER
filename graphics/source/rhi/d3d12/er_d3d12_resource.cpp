#include "er_backend_d3d12.h"
#ifndef SLIM_RHI_NO_D3D12
#include "er_d3d12_resource.h"
#include "er_d3d12_device.h"
#include "er_d3d12_swapchain.h"
#include "er_d3d12_pipeline.h"
#include "er_rhi_d3d12.h"
#include <cstring>
#include <cstdio>
#include <d3d12.h>
#include <windows.h>

using namespace Slim;

// ----------------------------------------------------------------------------
//                              CD3D12Buffer
// ----------------------------------------------------------------------------

RHI::CD3D12Buffer::CD3D12Buffer(CD3D12Device* pDevice
    , const BufferDescD3D12& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_pResource(desc.resource)
    , m_sDesc(static_cast<const BufferDesc&>(desc))
    , m_eResState(desc.resState)
{
    assert(pDevice && desc.resource);
    pDevice->AddRefCnt();
    m_pResource->AddRef();
    m_u64GpuAddress = m_pResource->GetGPUVirtualAddress();
}

RHI::CD3D12Buffer::~CD3D12Buffer() noexcept
{
    // Unmap if currently mapped?
    //if (m_pMappedData != nullptr && m_pResource != nullptr) {
    //    m_pResource->Unmap(0, nullptr);
    //    m_pMappedData = nullptr;
    //}
    RHI::SafeRelease(m_pResource);
    RHI::SafeDispose(m_pThisDevice);
}

RHI::CODE RHI::CD3D12Buffer::Map(void** ppData, const BufferRange* rangeHint) noexcept
{
    if (!ppData)
        return CODE_POINTER;
    assert(m_pResource);

    static_assert(sizeof(D3D12_RANGE) == sizeof(BufferRange), "reinterpret_cast here");
    const auto pRange = reinterpret_cast<const D3D12_RANGE*>(rangeHint);
    HRESULT hr = m_pResource->Map(0, pRange, ppData);
    if (SUCCEEDED(hr)) {

    }
    else {
        DRHI_LOGGER_ERR("Map failed: %u", hr);
    }
    return impl::d3d12_rhi(hr);
}

void RHI::CD3D12Buffer::Unmap() noexcept
{
    assert(m_pResource);
    m_pResource->Unmap(0, nullptr);
}

void RHI::CD3D12Buffer::SetDebugName(const char* name) noexcept
{
    impl::d3d12_set_debug_name(m_pResource, name);
}

// ----------------------------------------------------------------------------
//                              CD3D12Image
// ----------------------------------------------------------------------------

RHI::CD3D12Image::CD3D12Image(CD3D12Device* pDevice
    , const ImageDescD3D12& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_sDesc(desc)
    , m_pResource(desc.resource)
    , m_pOwner(desc.swapchain)
    //, m_eDxgiFormat(desc.dxgifmt)
{
    assert(pDevice && desc.resource);
    pDevice->AddRefCnt();
    m_pResource->AddRef();
    if (m_pOwner) {
        m_pOwner->AddRefCnt();
    }
}

RHI::CD3D12Image::~CD3D12Image() noexcept
{
    if (m_pOwner) {
        m_pOwner->OwnerDispose(this);
    }
    RHI::SafeDispose(m_pOwner);
    RHI::SafeDispose(m_pThisDevice);
    RHI::SafeRelease(m_pResource);
}

void RHI::CD3D12Image::SetDebugName(const char* name) noexcept
{
    impl::d3d12_set_debug_name(m_pResource, name);
}


void RHI::CD3D12Image::DisposeDescriptor(size_t ptr) noexcept
{
    assert(!"NOTIMPL");
}

// ----------------------------------------------------------------------------
//                              CD3D12Attachment
// ----------------------------------------------------------------------------

RHI::CD3D12Attachment::CD3D12Attachment(CD3D12Device* pDevice
    , const AttachmentDescD3D12& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_pImage(desc.image)
    , m_pStandaloneHeap(desc.heap)
    , m_hHeapPtr(desc.ptr)
    , m_sDesc(desc)
{
    assert(pDevice && desc.image && desc.ptr);
    pDevice->AddRefCnt();
    m_pImage->AddRefCnt();
    if (m_pStandaloneHeap)
        m_pStandaloneHeap->AddRef();
}

RHI::CD3D12Attachment::~CD3D12Attachment() noexcept
{
    // STANDALONE
    if (m_pStandaloneHeap) {
        RHI::SafeRelease(m_pStandaloneHeap);
    }
    else if (m_pImage) {
        m_pImage->DisposeDescriptor(m_hHeapPtr);
    }
    m_hHeapPtr = 0;
    RHI::SafeDispose(m_pImage);
    RHI::SafeDispose(m_pThisDevice);
}

// ----------------------------------------------------------------------------
//                              CD3D12FrameBuffer
// ----------------------------------------------------------------------------

#if 0
RHI::CD3D12FrameBuffer::CD3D12FrameBuffer(CD3D12Device* pDevice, const FrameBufferDesc& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_sDesc(desc)
{
    assert(pDevice && desc.renderpass);
    pDevice->AddRefCnt();
    desc.renderpass->AddRefCnt();
    
    RHI::SafeRefCnt(desc.depthStencil);
    for (uint32_t i = 0; i < desc.colorViewCount; ++i) {
        desc.colorViews[i]->AddRefCnt();
    }
}

RHI::CD3D12FrameBuffer::~CD3D12FrameBuffer() noexcept
{
    const auto colorCount = m_sDesc.colorViewCount;
    for (uint32_t i = 0; i < colorCount; ++i) {
        RHI::SafeDispose(m_sDesc.colorViews[i]);
    }
    RHI::SafeDispose(m_sDesc.depthStencil);
    RHI::SafeDispose(m_sDesc.renderpass);
    RHI::SafeDispose(m_pThisDevice);
}
#endif

#endif

