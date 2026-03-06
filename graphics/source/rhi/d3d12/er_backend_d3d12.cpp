#include "er_backend_d3d12.h"
#ifndef SLIM_RHI_NO_D3D12
#include <cassert>
#include <cstdio>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgidebug.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "er_rhi_d3d12.h"
#include "er_d3d12_adapter.h"
#include "er_d3d12_device.h"
#include "../common/er_shader_compiler_p.h"

using namespace Slim;

/// <summary>
/// Initializes a new instance of the <see cref="CD3D12Instance"/> class.
/// </summary>
/// <remarks>
/// This constructor creates an uninitialized instance. Call <see cref="CD3D12Instance::Init"/> to initialize
/// the instance with the appropriate Direct3D 12 and DXGI resources.
/// </remarks>
RHI::CD3D12Instance::CD3D12Instance() noexcept
{
}

/// <summary>
/// Finalizes an instance of the <see cref="CD3D12Instance"/> class.
/// </summary>
/// <remarks>
/// This destructor releases the DXGI factory and Direct3D 12 debug interface if they were created.
/// </remarks>
RHI::CD3D12Instance::~CD3D12Instance() noexcept
{
    if (m_pD3d12Debug) {
        // Report live objects before releasing debug interface
        IDXGIDebug1* pDxgiDebug = nullptr;
        if (SUCCEEDED(::DXGIGetDebugInterface1(0, IID_IDXGIDebug1, (void**)&pDxgiDebug))) {
            pDxgiDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL));
        }
        RHI::SafeRelease(pDxgiDebug);
    }
    RHI::SafeRelease(m_pDxgiFactory);
    RHI::SafeRelease(m_pD3d12Debug);
}

/// <summary>
/// Enumerates adapters available on the system.
/// </summary>
/// <param name="index">The zero-based index of the adapter to enumerate.</param>
/// <param name="ppAdapter">A pointer to receive the <see cref="IRHIAdapter"/> interface pointer. The caller is responsible for releasing the interface.</param>
/// <returns>
/// Returns <see cref="CODE_OK"/> if successful; otherwise, returns an error code.
/// Returns <see cref="CODE_UNEXPECTED"/> if the DXGI factory is not initialized.
/// </returns>
/// <remarks>
/// This method creates a <see cref="CD3D12Adapter"/> object for the adapter at the specified index.
/// The adapter object must be released by the caller when no longer needed.
/// </remarks>
RHI::CODE RHI::CD3D12Instance::EnumAdapters(uint32_t index, IRHIAdapter** ppAdapter) noexcept
{
    if (!m_pDxgiFactory)
        return CODE_UNEXPECTED;
    
    IDXGIAdapter1* pDxgiAdapter = nullptr;
    HRESULT hr = m_pDxgiFactory->EnumAdapters1(index, &pDxgiAdapter);
    if (SUCCEEDED(hr)) {
        if (const auto obj = new (std::nothrow) CD3D12Adapter{ this, pDxgiAdapter }) {
            *ppAdapter = obj;
        }
        else {
            hr = E_OUTOFMEMORY;
        }
    }
    // CLEAN UP
    RHI::SafeRelease(pDxgiAdapter);
    return impl::d3d12_rhi(hr);
}

/// <summary>
/// Gets the default adapter (adapter at index 0).
/// </summary>
/// <param name="ppAdapter">A pointer to receive the <see cref="IRHIAdapter"/> interface pointer. The caller is responsible for releasing the interface.</param>
/// <returns>
/// Returns <see cref="CODE_OK"/> if successful; otherwise, returns an error code.
/// </returns>
/// <remarks>
/// This method is a convenience wrapper that calls <see cref="CD3D12Instance::EnumAdapters"/> with index 0.
/// </remarks>
RHI::CODE RHI::CD3D12Instance::GetDefaultAdapter(IRHIAdapter** ppAdapter) noexcept
{
    return EnumAdapters(0, ppAdapter);
}

/// <summary>
/// Creates a Direct3D 12 device from the specified adapter.
/// </summary>
/// <param name="pAdapter">A pointer to the <see cref="IRHIAdapter"/> interface representing the adapter to create the device from. Must not be null.</param>
/// <param name="ppDevice">A pointer to receive the <see cref="IRHIDevice"/> interface pointer. The caller is responsible for releasing the interface.</param>
/// <returns>
/// Returns <see cref="CODE_OK"/> if successful; otherwise, returns an error code.
/// Returns <see cref="CODE_POINTER"/> if pAdapter or ppDevice is null.
/// </returns>
/// <remarks>
/// This method creates a <see cref="CD3D12Device"/> object that wraps an ID3D12Device4.
/// The device is created with D3D_FEATURE_LEVEL_11_0. The device object must be released by the caller when no longer needed.
/// </remarks>
RHI::CODE RHI::CD3D12Instance::CreateDevice(IRHIAdapter* pAdapter, IRHIDevice** ppDevice) noexcept
{
    if (!pAdapter || !ppDevice)
        return CODE_POINTER;

    RHI::AdapterDesc desc;
    pAdapter->GetDesc(&desc);
    if (desc.api != API::D3D12)
        return CODE_INVALIDARG;

    // SIMPLE RTTI
    const auto pD3d12Adapter = static_cast<CD3D12Adapter*>(pAdapter);
    assert(this == &pD3d12Adapter->RefInstance());

    HRESULT hr = S_OK;
    ID3D12Device4* pDevice = nullptr;

    // CREATE D3D DEVICE
    if (SUCCEEDED(hr)) {
        hr = ::D3D12CreateDevice(
            &pD3d12Adapter->RefAdapter(),
            D3D_FEATURE_LEVEL_11_0,
            IID_ID3D12Device4,
            (void**)&pDevice
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("D3D12CreateDevice failed: %u", hr);
        }
    }
    // CREATE RHI DEVICE
    if (SUCCEEDED(hr)) {
        const auto obj = new(std::nothrow) CD3D12Device{ pD3d12Adapter, pDevice };
        if (obj) {  
            hr = obj->Init();
            if (SUCCEEDED(hr)) {
                *ppDevice = obj;
            }
            else {
                obj->Dispose();
                *ppDevice = nullptr;
            }
        }
    }
    // CLEAN UP
    RHI::SafeRelease(pDevice);
    return impl::d3d12_rhi(hr);
}

/// <summary>
/// Initializes the Direct3D 12 instance with the specified description.
/// </summary>
/// <param name="pDesc">A pointer to an <see cref="InstanceDesc"/> structure that describes the instance configuration. Must not be null.</param>
/// <returns>
/// Returns <see cref="CODE_OK"/> if successful; otherwise, returns an error code.
/// </returns>
/// <remarks>
/// This method initializes the Direct3D 12 debug layer (if enabled) and creates the DXGI factory.
/// The debug layer is enabled if pDesc->debug is true. The DXGI factory is created with debug flags if debugging is enabled.
/// </remarks>
RHI::CODE RHI::CD3D12Instance::Init(const InstanceDesc* pDesc) noexcept
{
    HRESULT hr = S_OK;
    // DEBUG LAYER
    if (pDesc->debug) {
        hr = ::D3D12GetDebugInterface(IID_ID3D12Debug, (void**)&m_pD3d12Debug);
        if (SUCCEEDED(hr)) {
            m_pD3d12Debug->EnableDebugLayer();
        }
        else {
            DRHI_LOGGER_ERR("D3D12GetDebugInterface failed: %u", hr);
        }
    }
    // DXGI Factory
    if (SUCCEEDED(hr)) {
        UINT flags = 0;
        if (pDesc->debug) 
            flags |= DXGI_CREATE_FACTORY_DEBUG;

        hr = ::CreateDXGIFactory2(flags, IID_IDXGIFactory4, (void**)&m_pDxgiFactory);
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateDXGIFactory2 failed: %u", hr);
        }
    }
    return impl::d3d12_rhi(hr);
}


RHI::CODE RHI::CD3D12Instance::CreateShaderCompiler(const char* dyLib, IRHIShaderCompiler** ppCompiler) noexcept
{
#ifndef SLIM_RHI_NO_SHADER_COMPILER
    return RHI::CreateShaderCompiler(dyLib, ppCompiler, "d3d12");
#else
    return CODE_NOTIMPL;
#endif
}

/// <summary>
/// Creates a Direct3D 12 RHI instance.
/// </summary>
/// <param name="pDesc">A pointer to an <see cref="InstanceDesc"/> structure that describes the instance configuration. Must not be null.</param>
/// <param name="ppIns">A pointer to receive the <see cref="IRHIInstance"/> interface pointer. The caller is responsible for releasing the interface.</param>
/// <returns>
/// Returns <see cref="CODE_OK"/> if successful; otherwise, returns an error code.
/// Returns <see cref="CODE_OUTOFMEMORY"/> if memory allocation fails.
/// </returns>
/// <remarks>
/// This function creates a new <see cref="CD3D12Instance"/> object and initializes it with the provided description.
/// The instance object must be released by the caller when no longer needed.
/// </remarks>
RHI::CODE RHI::CreateInstanceD3D12(const InstanceDesc* pDesc, IRHIInstance** ppIns) noexcept
{
    assert(ppIns && pDesc);
    *ppIns = nullptr;
    if (const auto obj = new(std::nothrow) CD3D12Instance) {
        const auto code = obj->Init(pDesc);
        if (RHI::Success(code)) {
            *ppIns = obj;
            return CODE_OK;
        }
        else {
            obj->Dispose();
            return code;
        }
    }
    return CODE_OUTOFMEMORY;
}

#endif