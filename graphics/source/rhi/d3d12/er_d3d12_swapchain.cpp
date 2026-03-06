#include "er_d3d12_swapchain.h"
#ifndef SLIM_RHI_NO_D3D12
#include <cassert>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "er_d3d12_device.h"
#include "er_d3d12_command.h"
#include "er_d3d12_resource.h"
#include "er_rhi_d3d12.h"
#include <dxgi1_4.h>
#include <d3d12.h>
using namespace Slim;


RHI::CD3D12SwapChain::CD3D12SwapChain(CD3D12Device* pDevice
    , const SwapChainDesc& desc
    , IDXGISwapChain3* pSwapChain, CD3D12CommandQueue* pQueue) noexcept
    : m_pDevice(pDevice)
    , m_pSwapChain(pSwapChain)
    , m_pPresentQueue(pQueue)
    , m_sDesc(desc)
{
    assert(pDevice && pSwapChain && pQueue);
    pSwapChain->AddRef();
    pDevice->AddRefCnt();
    pQueue->AddRefCnt();
}

RHI::CD3D12SwapChain::~CD3D12SwapChain() noexcept
{
    if (m_hFrameEvent) {
        ::CloseHandle(m_hFrameEvent);
        m_hFrameEvent = nullptr;
    }
    RHI::SafeRelease(m_pFrameCounter);

    // Release swap chain image resources
    const auto count = m_sDesc.bufferCount;
    for (uint32_t i = 0; i < count; ++i) {
        RHI::SafeRelease(m_szSwapChainImages[i].resource);
        m_szSwapChainImages[i].image = {};
    }
    std::memset(m_szSwapChainImages, 0, sizeof(m_szSwapChainImages));

    RHI::SafeDispose(m_pPresentQueue);
    RHI::SafeRelease(m_pSwapChain);
    RHI::SafeDispose(m_pDevice);
}

long Slim::RHI::CD3D12SwapChain::Init() noexcept
{
    HRESULT hr = S_OK;
    // Get swap chain buffers
    const auto count = m_sDesc.bufferCount;
    for (uint32_t i = 0; i < count && SUCCEEDED(hr); ++i) {
        hr = m_pSwapChain->GetBuffer(i, IID_ID3D12Resource, (void**)&m_szSwapChainImages[i].resource);
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("GetBuffer failed for index %u: %u", i, hr);
            break;
        }
    }
    // create frame counter for MAX_FRAMES_IN_FLIGHT
    if (SUCCEEDED(hr)) {
        hr = m_pDevice->RefDevice().CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)&m_pFrameCounter);
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateFence failed: %u", hr);
        }
    }
    // create frame event for MAX_FRAMES_IN_FLIGHT
    if (SUCCEEDED(hr)) {
        BOOL manualReset = FALSE;
        BOOL initialState = FALSE;
        m_hFrameEvent = ::CreateEventW(0, manualReset, initialState, nullptr);
        if (!m_hFrameEvent) {
            hr = HRESULT_FROM_WIN32(::GetLastError());
            DRHI_LOGGER_ERR("CreateEventW failed: %u", hr);
        }
    }
    return hr;
}

void RHI::CD3D12SwapChain::GetDesc(SwapChainDesc* pDesc) noexcept
{
    if (pDesc) *pDesc = m_sDesc;
}

void RHI::CD3D12SwapChain::GetBoundQueue(IRHICommandQueue** ppQueue) noexcept
{
    if (ppQueue) {
        *ppQueue = m_pPresentQueue;
        m_pPresentQueue->AddRefCnt();
    }
}

RHI::CODE RHI::CD3D12SwapChain::AcquireFrame(FrameContext& ctx) noexcept
{

    ctx.indexInFlight = m_uCurrentFlightIndex;
    ctx.indexBuffer = m_pSwapChain->GetCurrentBackBufferIndex();

    // WAIT FRAME IN FLIGHT
    HRESULT hr = S_OK;
    const auto& frame = m_szFlightFrames[m_uCurrentFlightIndex];
    const auto completed = m_pFrameCounter->GetCompletedValue();
    if (completed < frame.value) {
        hr = m_pFrameCounter->SetEventOnCompletion(frame.value, m_hFrameEvent);
        if (SUCCEEDED(hr)) {
            const auto code = ::WaitForSingleObject(m_hFrameEvent, MAX_WAIT_TIMEOUT);
            if (code != WAIT_OBJECT_0) {
                hr = E_FAIL;
                DRHI_LOGGER_ERR("WaitForSingleObject failed: %u", hr);
            }
        }
        else {
            DRHI_LOGGER_ERR("SetEventOnCompletion failed: %u", hr);
        }
    }
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12SwapChain::PresentFrame(uint32_t syncInterval) noexcept
{
    assert(m_pSwapChain && m_pPresentQueue);
    auto& queue = m_pPresentQueue->RefCommandQueue();
    ++m_uFrameCounter;
    m_szFlightFrames[m_uCurrentFlightIndex].value = m_uFrameCounter;
    queue.Signal(m_pFrameCounter, m_uFrameCounter);

    UINT flag = 0;
    HRESULT hr = m_pSwapChain->Present(syncInterval, flag);
    //const auto vv = fence->GetCompletedValue();
    //if (vv < fv) {
    //    assert(handle);
    //    hr = fence->SetEventOnCompletion(fv, handle);
    //    WaitForSingleObject(handle, INFINITE);
    //}

    m_uCurrentFlightIndex = (m_uCurrentFlightIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12SwapChain::Resize(uint32_t width, uint32_t height) noexcept
{
    assert(!"NOTIMPL");
    return CODE();
}

RHI::CODE RHI::CD3D12SwapChain::GetImage(uint32_t index, IRHIImage** image) noexcept
{
    if (!image)
        return CODE_POINTER;
    if (index >= m_sDesc.bufferCount)
        return CODE_INVALIDARG;

    auto& data = m_szSwapChainImages[index];
    if (!data.resource)
        return CODE_HANDLE;

    if (!data.image.ptr) {
        ImageDescD3D12 desc{};
        desc.width = m_sDesc.width;
        desc.height = m_sDesc.height;
        desc.format = m_sDesc.format;
        desc.resource = data.resource;
       // desc.dxgifmt = impl::rhi_d3d12(m_sDesc.format);
        desc.swapchain = this;
        desc.usage.flags = IMAGE_USAGE_TRANSFER_SRC | IMAGE_USAGE_TRANSFER_DST | IMAGE_USAGE_COLOR_ATTACHMENT;
        data.image.ptr = new(std::nothrow) CD3D12Image{ m_pDevice, desc };
        *image = data.image.ptr;
        return data.image.ptr ? CODE_OK : CODE_OUTOFMEMORY;
    }
    data.image.ptr->AddRefCnt();
    *image = data.image.ptr;
    return CODE_OK;
}

void RHI::CD3D12SwapChain::OwnerDispose(CD3D12Image* image) noexcept
{
    assert(image);
    const auto begin = m_szSwapChainImages;
    const auto end = m_szSwapChainImages + m_sDesc.bufferCount;
    const auto itr = std::find_if(begin, end, [=](const SwapChainImageData& d) noexcept {
        return d.image.ptr == image;
    });
    if (itr == end) {
        assert(!"BAD OBJECT");
    }
    else {
        itr->image = {};
    }
}

#endif

