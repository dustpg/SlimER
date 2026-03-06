#include "er_d3d12_device.h"
#ifndef SLIM_RHI_NO_D3D12
#include <cstdio>
#include <cassert>
#include "er_d3d12_adapter.h"
#include "er_d3d12_swapchain.h"
#include "er_d3d12_command.h"
#include "er_d3d12_pipeline.h"
#include "er_d3d12_resource.h"
#include "er_d3d12_descriptor.h"
#include "er_backend_d3d12.h"
#include "er_rhi_d3d12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using namespace Slim;

/// <summary>
/// Initializes a new instance of the <see cref="CD3D12Device"/> class.
/// </summary>
/// <param name="pAdapter">A pointer to the <see cref="CD3D12Adapter"/> that this device is associated with. Must not be null.</param>
/// <param name="pDevice">A pointer to the ID3D12Device4 interface to wrap. Must not be null.</param>
/// <remarks>
/// This constructor takes ownership of the provided adapter and device objects by incrementing their reference counts.
/// The adapter and device pointers are validated using assertions in debug builds.
/// </remarks>
RHI::CD3D12Device::CD3D12Device(CD3D12Adapter* pAdapter, ID3D12Device4* pDevice) noexcept
    : m_pAdapter(pAdapter)
    , m_pDevice(pDevice)
{
    assert(pAdapter && pDevice);
    pAdapter->AddRefCnt();
    pDevice->AddRef();
}

/// <summary>
/// Finalizes an instance of the <see cref="CD3D12Device"/> class.
/// </summary>
/// <remarks>
/// This destructor releases the underlying ID3D12Device4 and disposes of the associated <see cref="CD3D12Adapter"/>.
/// The reference counts are properly managed to ensure safe cleanup.
/// </remarks>
RHI::CD3D12Device::~CD3D12Device() noexcept
{
    RHI::SafeRelease(m_pDevice);
    RHI::SafeDispose(m_pAdapter);
}

/// <summary>
/// Initializes the Direct3D 12 device.
/// </summary>
/// <returns>
/// Returns S_OK (0) if successful; otherwise, returns an HRESULT error code.
/// </returns>
/// <remarks>
/// This method performs any necessary initialization of the Direct3D 12 device after construction.
/// Currently, this method returns S_OK as the device is fully initialized during construction.
/// </remarks>
long RHI::CD3D12Device::Init() noexcept
{
    return S_OK;
}


RHI::CODE RHI::CD3D12Device::Flush() noexcept
{
    assert(!"NOTIMPL");
    return CODE();
}

RHI::CODE RHI::CD3D12Device::CreateSwapChain(const SwapChainDesc* pDesc, IRHISwapChain** ppSwapChain) noexcept
{
    if (!pDesc || !ppSwapChain)
        return CODE_POINTER;

    assert(m_pDevice);
    IDXGISwapChain1* pDxgiSwapChain1 = nullptr;
    IDXGISwapChain3* pDxgiSwapChain3 = nullptr;
    ID3D12CommandQueue* pD3d12CommandQueue = nullptr;
    CD3D12CommandQueue* pRHICommandQueue = nullptr;
    HRESULT hr = S_OK;

    // CREATE D3D12 GRAPHICS QUEUE
    if (SUCCEEDED(hr)) {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr = m_pDevice->CreateCommandQueue(
            &desc, 
            IID_ID3D12CommandQueue, (void**)&pD3d12CommandQueue
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateCommandQueue failed: %u", hr);
        }
    }
    // CREATE D3D12 SWAP CHAIN
    if (SUCCEEDED(hr)) {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = pDesc->width;
        desc.Height = pDesc->height;
        desc.Format = impl::rhi_d3d12(pDesc->format);
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = pDesc->bufferCount;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        auto& factory = m_pAdapter->RefInstance().RefFactory();
        hr = factory.CreateSwapChainForHwnd(
            pD3d12CommandQueue,
            reinterpret_cast<HWND>(pDesc->windowHandle),
            &desc,
            nullptr,
            nullptr,
            &pDxgiSwapChain1
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateSwapChainForHwnd failed: %u", hr);
        }
    }
    // GHT D3D12 SWAP CHAIN 3
    if (SUCCEEDED(hr)) {
        hr = pDxgiSwapChain1->QueryInterface(
            IID_IDXGISwapChain3,
            (void**)&pDxgiSwapChain3
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("QueryInterface(IDXGISwapChain3) failed: %u", hr);
        }
    }
    // CREATE RHI COMMAND QUEUE
    if (SUCCEEDED(hr)) {
        pRHICommandQueue = new(std::nothrow) CD3D12CommandQueue{ this, pD3d12CommandQueue };
        if (!pRHICommandQueue) {
            hr = E_OUTOFMEMORY;
        }
    }
    // CREATE RHI SWAP CHAIN
    if (SUCCEEDED(hr)) {
        const auto obj = new (std::nothrow) CD3D12SwapChain{
            this, *pDesc, pDxgiSwapChain3, pRHICommandQueue
        };
        *ppSwapChain = nullptr;
        if (obj) {
            hr = obj->Init();
            if (SUCCEEDED(hr)) {
                *ppSwapChain = obj;
            }
            else {
                DRHI_LOGGER_ERR("CD3D12SwapChain::Init failed");
                obj->Dispose();
            }
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }

    // CLEAN UP
    RHI::SafeDispose(pRHICommandQueue);
    RHI::SafeRelease(pDxgiSwapChain3);
    RHI::SafeRelease(pDxgiSwapChain1);
    RHI::SafeRelease(pD3d12CommandQueue);
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12Device::CreateCommandPool(const CommandPoolDesc* pDesc, IRHICommandPool** ppPool) noexcept
{
    if (!pDesc || !ppPool)
        return CODE_POINTER;

    assert(m_pDevice);

    // Convert QUEUE_TYPE to D3D12_COMMAND_LIST_TYPE
    D3D12_COMMAND_LIST_TYPE commandListType = impl::rhi_d3d12(pDesc->queueType);

    ID3D12CommandAllocator* pCommandAllocator = nullptr;
    CD3D12CommandPool* pRHICommandPool = nullptr;
    HRESULT hr = S_OK;

    // CREATE D3D12 COMMAND ALLOCATOR
    if (SUCCEEDED(hr)) {
        hr = m_pDevice->CreateCommandAllocator(
            commandListType,
            IID_ID3D12CommandAllocator,
            (void**)&pCommandAllocator
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateCommandAllocator failed: %u", hr);
        }
    }

    // CREATE RHI COMMAND POOL
    if (SUCCEEDED(hr)) {
        pRHICommandPool = new (std::nothrow) CD3D12CommandPool{ this, pCommandAllocator, uint32_t(commandListType)};
        *ppPool = nullptr;
        if (pRHICommandPool) {
            *ppPool = pRHICommandPool;
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }

    // CLEAN UP
    RHI::SafeRelease(pCommandAllocator);
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12Device::CreateCommandQueue(const CommandQueueDesc* pDesc, IRHICommandQueue** ppQueue) noexcept
{
    assert(!"NOTIMPL");
    return CODE();
}

RHI::CODE RHI::CD3D12Device::CreateFence(uint64_t initialValue, IRHIFence** ppFence) noexcept
{
    if (!ppFence)
        return CODE_POINTER;

    assert(m_pDevice);

    ID3D12Fence* pD3d12Fence = nullptr;
    CD3D12Fence* pRHIFence = nullptr;
    HRESULT hr = S_OK;

    // CREATE D3D12 FENCE
    if (SUCCEEDED(hr)) {
        hr = m_pDevice->CreateFence(
            initialValue,
            D3D12_FENCE_FLAG_NONE,
            IID_ID3D12Fence,
            (void**)&pD3d12Fence
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateFence failed: %u", hr);
        }
    }

    // CREATE RHI FENCE
    if (SUCCEEDED(hr)) {
        pRHIFence = new (std::nothrow) CD3D12Fence{ this, pD3d12Fence, initialValue };
        *ppFence = nullptr;
        if (pRHIFence) {
            *ppFence = pRHIFence;
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }

    // CLEAN UP
    RHI::SafeRelease(pD3d12Fence);
    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12Device::CreateBuffer(const BufferDesc* pDesc, IRHIBuffer** ppBuffer) noexcept
{
    if (!pDesc || !ppBuffer)
        return CODE_POINTER;
    if (!pDesc->size)
        return CODE_INVALIDARG;

    assert(m_pDevice);
    ID3D12Resource* pD3d12Resource = nullptr;
    HRESULT hr = S_OK;

    // VALIDATE MEMORY_TYPE
    assert(pDesc->type <= RHI::MEMORY_TYPE_READBACK);
    // CONVERT MEMORY_TYPE TO D3D12_HEAP_TYPE
    const auto memoryConversion = impl::rhi_d3d12_cvt(pDesc->type, pDesc->usage);

    // CONVERT BUFFER_USAGES TO D3D12_RESOURCE_FLAGS
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    if (SUCCEEDED(hr)) {
        const uint32_t usage = pDesc->usage.flags;
        if (usage & BUFFER_USAGE_STORAGE_BUFFER) {
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
    }

    // CREATE D3D12 RESOURCE
    if (SUCCEEDED(hr)) {
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = pDesc->alignment;
        resourceDesc.Width = pDesc->size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = resourceFlags;

        hr = m_pDevice->CreateCommittedResource(
            &memoryConversion.heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            memoryConversion.initialState,
            nullptr,
            IID_ID3D12Resource,
            reinterpret_cast<void**>(&pD3d12Resource)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateCommittedResource failed: %u", hr);
        }
    }

    // CREATE RHI BUFFER
    if (SUCCEEDED(hr)) {
        BufferDescD3D12 descD3D12 {};
        static_cast<BufferDesc&>(descD3D12) = *pDesc;
        descD3D12.resource = pD3d12Resource;
        descD3D12.resState = memoryConversion.resState;
        const auto pRHIBuffer = new (std::nothrow) CD3D12Buffer{ this, descD3D12 };
        *ppBuffer = nullptr;
        if (pRHIBuffer) {
            *ppBuffer = pRHIBuffer;
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }

    // CLEAN UP
    RHI::SafeRelease(pD3d12Resource);

    return impl::d3d12_rhi(hr);
}


RHI::CODE RHI::CD3D12Device::CreateShader(const void* data, size_t size, IRHIShader** ppShader, const char*) noexcept
{
    if (!data || !ppShader)
        return CODE_POINTER;
    if (!size)
        return CODE_INVALIDARG;

    // CREATE RHI OBJECT
    auto obj = new(std::nothrow) CD3D12Shader{ this, data, size };
    // OUTOFMEMORY#1
    if (obj) {
        // OUTOFMEMORY#2
        if (!obj->GetBytecodeSize()) {
            obj->Dispose();
            obj = nullptr;
        }
    }
    *ppShader = obj;
    return obj ? CODE_OK : CODE_OUTOFMEMORY;
}

RHI::CODE RHI::CD3D12Device::CreateImage(const ImageDesc* pDesc, IRHIImage** ppImage) noexcept
{
    if (!pDesc || !ppImage)
        return CODE_POINTER;
    if (!pDesc->width || !pDesc->height)
        return CODE_INVALIDARG;

    assert(m_pDevice);
    ID3D12Resource* pD3d12Resource = nullptr;
    HRESULT hr = S_OK;
    D3D12_RESOURCE_DESC resourceDesc;
    // CONVERT MEMORY_TYPE AND IMAGE_USAGES TO D3D12_HEAP_PROPERTIES AND RESOURCE STATE
    const auto memoryConversion = impl::rhi_d3d12_cvt(RHI::MEMORY_TYPE_DEFAULT, pDesc->usage);

    // CONVERT IMAGE_USAGES TO D3D12_RESOURCE_FLAGS
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    const uint32_t usageFlags = pDesc->usage.flags;
    if (usageFlags & RHI::IMAGE_USAGE_COLOR_ATTACHMENT) {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (usageFlags & RHI::IMAGE_USAGE_DEPTH_STENCIL) {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    if (usageFlags & RHI::IMAGE_USAGE_STORAGE) {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // CREATE D3D12 RESOURCE
    if (SUCCEEDED(hr)) {
        resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0; // Let D3D12 choose alignment
        resourceDesc.Width = pDesc->width;
        resourceDesc.Height = pDesc->height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = impl::rhi_d3d12(pDesc->format);
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // Left D3D12 choose layout
        resourceDesc.Flags = resourceFlags;


        hr = m_pDevice->CreateCommittedResource(
            &memoryConversion.heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            memoryConversion.initialState,
            nullptr,
            IID_ID3D12Resource,
            reinterpret_cast<void**>(&pD3d12Resource)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateCommittedResource failed: %u", hr);
        }
    }

    // CREATE RHI IMAGE
    if (SUCCEEDED(hr)) {
        ImageDescD3D12 descD3D12{};
        static_cast<ImageDesc&>(descD3D12) = *pDesc;
        descD3D12.resource = pD3d12Resource;
        descD3D12.swapchain = nullptr;
        //descD3D12.dxgifmt = resourceDesc.Format;
        const auto pRHIImage = new (std::nothrow) CD3D12Image{ this, descD3D12 };
        *ppImage = nullptr;
        if (pRHIImage) {
            *ppImage = pRHIImage;
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }

    // CLEAN UP
    RHI::SafeRelease(pD3d12Resource);

    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12Device::CreateAttachment(const AttachmentDesc* pDesc, IRHIImage * pImage, IRHIAttachment** ppView) noexcept
{
    if (!pDesc || !pImage || !ppView)
        return CODE_POINTER;
    
    const auto image = static_cast<CD3D12Image*>(pImage);
    // SIMPLE RTTI
    assert(this == &image->RefDevice());

    AttachmentDescD3D12 desc{};
    static_cast<AttachmentDesc&>(desc) = *pDesc;

    HRESULT hr = S_OK;

    // Determine heap type based on attachment type
    const bool isDepthStencil = (pDesc->type == RHI::ATTACHMENT_TYPE_DEPTHSTENCIL);
    const D3D12_DESCRIPTOR_HEAP_TYPE heapType = isDepthStencil 
        ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV 
        : D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    // Create Standalone Heap as Fallback
    if (SUCCEEDED(hr)) {
        // TODO: Swapchain/Owner own the Heap
        D3D12_DESCRIPTOR_HEAP_DESC dhDesc = { };
        dhDesc.NumDescriptors = 1;
        dhDesc.Type = heapType;
        dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = m_pDevice->CreateDescriptorHeap(
            &dhDesc,
            IID_ID3D12DescriptorHeap,
            (void**)&desc.heap
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateDescriptorHeap failed: %u", hr);
        }
    }   

    // Use format from AttachmentDesc if specified, otherwise use format from Image
    const auto& imageDesc = image->RefDesc();
    desc.format = (pDesc->format != RHI::RHI_FORMAT_UNKNOWN) ? pDesc->format : imageDesc.format;
    desc.image = image;

    // Create Attachment
    if (SUCCEEDED(hr)) {
        desc.ptr = desc.heap->GetCPUDescriptorHandleForHeapStart().ptr;
        
        if (isDepthStencil) {
            // Create Depth Stencil View
            m_pDevice->CreateDepthStencilView(
                &image->RefResource(),
                nullptr,
                { desc.ptr }
            );
        }
        else {
            // Create Render Target View
            m_pDevice->CreateRenderTargetView(
                &image->RefResource(),
                nullptr,
                { desc.ptr }
            );
        }
    }
    // CREATE RHI OBJECT
    if (SUCCEEDED(hr)) {
        const auto pAttachmentObj = new (std::nothrow) CD3D12Attachment{ this, desc };
        *ppView = pAttachmentObj;
        if (pAttachmentObj) {

        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }
    // Clean Up
    RHI::SafeRelease(desc.heap);

    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12Device::CreateDescriptorBuffer(const DescriptorBufferDesc* pDesc, IRHIDescriptorBuffer** ppBuffer) noexcept
{
    if (!pDesc || !ppBuffer)
        return CODE_POINTER;
    if (!pDesc->slotCount)
        return CODE_INVALIDARG;

    const auto obj = new (std::nothrow) CD3D12DescriptorBuffer{ this };
    if (!obj)
        return CODE_OUTOFMEMORY;

    const auto code = obj->Init(*pDesc);

    if (Failure(code)) {
        obj->Dispose();
        *ppBuffer = nullptr;
    }
    else {
        *ppBuffer = obj;
    }
    return code;
}

#if 0
RHI::CODE RHI::CD3D12Device::CreateFrameBuffer(const FrameBufferDesc* pDesc, IRHIFrameBuffer** ppFrameBuffer) noexcept
{
    if (!pDesc || !ppFrameBuffer)
        return CODE_POINTER;

    if (!pDesc->renderpass) {
        return CODE_INVALIDARG;
    }

    constexpr uint32_t MAX_COLOR_VIEWS = sizeof(pDesc->colorViews) / sizeof(pDesc->colorViews[0]);
    if (pDesc->colorViewCount > MAX_COLOR_VIEWS) {
        return CODE_INVALIDARG;
    }
#ifndef NDEBUG
    // Validate render pass belongs to this device
    const auto pRenderPass = static_cast<CD3D12RenderPass*>(pDesc->renderpass);
    assert(this == &pRenderPass->RefDevice());

    // Validate color views belong to this device
    for (uint32_t i = 0; i < pDesc->colorViewCount; ++i) {
        if (pDesc->colorViews[i]) {
            const auto pColorView = static_cast<CD3D12Attachment*>(pDesc->colorViews[i]);
            assert(this == &pColorView->RefDevice());
        }
    }

    // Validate depth stencil view belongs to this device
    if (pDesc->depthStencil) {
        const auto pDepthStencil = static_cast<CD3D12Attachment*>(pDesc->depthStencil);
        assert(this == &pDepthStencil->RefDevice());
    }
#endif
    // D3D12 doesn't have a FrameBuffer object, we just store the references
    // The information will be used when BeginRenderPass is called
    const auto pFrameBuffer = new (std::nothrow) CD3D12FrameBuffer{ this, *pDesc };
    if (!pFrameBuffer) {
        DRHI_LOGGER_ERR("Failed to allocate CD3D12FrameBuffer");
        return CODE_OUTOFMEMORY;
    }

    *ppFrameBuffer = pFrameBuffer;
    return CODE_OK;
}
#endif

/// <summary>
/// Initializes a new instance of the <see cref="CD3D12Fence"/> class.
/// </summary>
/// <param name="pDevice">A pointer to the <see cref="CD3D12Device"/> that this fence is associated with. Must not be null.</param>
/// <param name="pFence">A pointer to the ID3D12Fence interface to wrap. Must not be null.</param>
/// <param name="initialValue">The initial value for the fence.</param>
/// <remarks>
/// This constructor takes ownership of the provided device and fence objects by incrementing their reference counts.
/// It also creates an event handle for waiting on the fence.
/// </remarks>
RHI::CD3D12Fence::CD3D12Fence(CD3D12Device* pDevice, ID3D12Fence* pFence, uint64_t initialValue) noexcept
    : m_pThisDevice(pDevice)
    , m_pFence(pFence)
{
    assert(pDevice && pFence);
    pDevice->AddRefCnt();
    pFence->AddRef();

    // Create an event handle for waiting on the fence
    BOOL manualReset = FALSE;
    BOOL initialState = FALSE;
    m_hEvent = ::CreateEventW(nullptr, manualReset, initialState, nullptr);
    if (!m_hEvent) {
        DRHI_LOGGER_ERR("CreateEventW failed");
    }

    // Set the initial value if needed
    if (initialValue > 0) {
        // Note: D3D12 fence initial value is set when creating the fence, not here
        // But we can signal it if needed
    }
}

/// <summary>
/// Finalizes an instance of the <see cref="CD3D12Fence"/> class.
/// </summary>
/// <remarks>
/// This destructor releases the underlying ID3D12Fence, closes the event handle, and disposes of the associated <see cref="CD3D12Device"/>.
/// The reference counts are properly managed to ensure safe cleanup.
/// </remarks>
RHI::CD3D12Fence::~CD3D12Fence() noexcept
{
    if (m_hEvent) {
        ::CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }
    RHI::SafeRelease(m_pFence);
    RHI::SafeDispose(m_pThisDevice);
}

/// <summary>
/// Gets the current completed value of the fence.
/// </summary>
/// <returns>
/// Returns the current completed value of the fence.
/// </returns>
uint64_t RHI::CD3D12Fence::GetCompletedValue() noexcept
{
    assert(m_pFence);
    return m_pFence->GetCompletedValue();
}

/// <summary>
/// Waits for the fence to reach the specified value.
/// </summary>
/// <param name="value">The value to wait for.</param>
/// <param name="time">The timeout in milliseconds. Use WAIT_FOREVER for infinite wait.</param>
/// <returns>
/// Returns WAIT_INDEX_0 if the wait succeeded, or WAIT_TIME_OUT if the wait timed out.
/// </returns>
RHI::WAIT RHI::CD3D12Fence::Wait(uint64_t value, uint32_t time) noexcept
{
    assert(m_pFence && m_hEvent);

    // Check if the value is already completed
    const uint64_t completedValue = m_pFence->GetCompletedValue();
    if (completedValue >= value) {
        return WAIT_CODE_SUCCESS;
    }

    // Set up the event to be signaled when the fence reaches the specified value
    HRESULT hr = m_pFence->SetEventOnCompletion(value, m_hEvent);
    if (FAILED(hr)) {
        DRHI_LOGGER_ERR("SetEventOnCompletion failed: %u", hr);
        return WAIT_CODE_FAILED;
    }

    // Wait for the event
    static_assert(WAIT_FOREVER == INFINITE, "WAIT_FOREVER");
    DWORD waitResult = ::WaitForSingleObject(m_hEvent, time);

    switch (waitResult)
    {
    case WAIT_OBJECT_0: return WAIT_CODE_SUCCESS;
    case WAIT_TIMEOUT: return WAIT_CODE_TIME_OUT;
    case WAIT_ABANDONED: return WAIT_CODE_ABANDONED;
    default: return WAIT_CODE_FAILED;
    }

}

#endif

