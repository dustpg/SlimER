#include "er_backend_d3d12.h"
#ifndef SLIM_RHI_NO_D3D12
#include <cassert>
#include "er_d3d12_adapter.h"
#include "er_rhi_d3d12.h"
#include <dxgi1_2.h>
#include <d3d12.h>
using namespace Slim;

/// <summary>
/// Initializes a new instance of the <see cref="CD3D12Adapter"/> class.
/// </summary>
/// <param name="pIns">A pointer to the <see cref="CD3D12Instance"/> that this adapter is associated with. Must not be null.</param>
/// <param name="pAdatper">A pointer to the IDXGIAdapter1 interface to wrap. Must not be null.</param>
/// <remarks>
/// This constructor takes ownership of the provided instance and adapter objects by incrementing their reference counts.
/// It queries the adapter description and initializes the adapter descriptor. The adapter is validated by attempting
/// to create a D3D12 device to check for D3D_FEATURE_LEVEL_11_0 support.
/// </remarks>
Slim::RHI::CD3D12Adapter::CD3D12Adapter(CD3D12Instance* pIns, IDXGIAdapter1* pAdatper) noexcept
    : m_pInstance(pIns)
    , m_pDxgiAdapter(pAdatper)
{
    assert(pAdatper && pIns);
    pIns->AddRefCnt();
    pAdatper->AddRef();

    DXGI_ADAPTER_DESC1 desc;
    pAdatper->GetDesc1(&desc);
    m_sDesc.api = API::D3D12;
    m_sDesc.deviceID = desc.DeviceId;
    m_sDesc.vendorID = desc.VendorId;
    m_sDesc.vram = desc.DedicatedVideoMemory;
    ::WideCharToMultiByte(
        CP_UTF8, 0,
        desc.Description, -1,
        m_sDesc.name, sizeof(m_sDesc.name),
        nullptr, nullptr
    );

    // CPU (Software adapter)
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        m_sDesc.feature = ADAPTER_FEATURE(m_sDesc.feature | ADAPTER_FEATURE_CPU);

#if 1
    // DXGI_FORMAT_B8G8R8A8_UNORM and DXGI_FORMAT_B8G8R8A8_UNORM_SRGB). 
    // All 10level9 and higher hardware with WDDM 1.1+ drivers support BGRA formats.

    // D3D12: D3D_FEATURE_LEVEL_11_0 NEEDED
    const auto hr = ::D3D12CreateDevice(
        m_pDxgiAdapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_ID3D12Device,
        nullptr
    );
    // D3D12: D3D_FEATURE_LEVEL_11_0 NEEDED
    if (SUCCEEDED(hr)) {
        m_sDesc.feature = ADAPTER_FEATURE(m_sDesc.feature | ADAPTER_FEATURE_2D);
    }
#else
    m_sDesc.feature = ADAPTER_FEATURE(pDesc->feature | ADAPTER_FEATURE_2D);
#endif
}

/// <summary>
/// Gets the description of the adapter.
/// </summary>
/// <param name="pDesc">A pointer to an <see cref="AdapterDesc"/> structure to receive the adapter description. If null, the method does nothing.</param>
/// <remarks>
/// This method copies the cached adapter description to the provided structure.
/// The description includes vendor ID, device ID, video memory, adapter name, and supported features.
/// </remarks>
void RHI::CD3D12Adapter::GetDesc(AdapterDesc* pDesc) noexcept
{
    if (pDesc) *pDesc = m_sDesc;
}

/// <summary>
/// Finalizes an instance of the <see cref="CD3D12Adapter"/> class.
/// </summary>
/// <remarks>
/// This destructor releases the underlying IDXGIAdapter1 and disposes of the associated <see cref="CD3D12Instance"/>.
/// The reference counts are properly managed to ensure safe cleanup.
/// </remarks>
RHI::CD3D12Adapter::~CD3D12Adapter() noexcept
{
    RHI::SafeRelease(m_pDxgiAdapter);
    RHI::SafeDispose(m_pInstance);
}

#endif