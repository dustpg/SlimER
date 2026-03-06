#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_adapter.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"

struct IDXGIAdapter1;

namespace Slim { namespace RHI {

    class CD3D12Instance;

    /// <summary>
    /// Represents a Direct3D 12 adapter implementation of the RHI adapter interface.
    /// </summary>
    /// <remarks>
    /// This class wraps an IDXGIAdapter1 and provides adapter information for Direct3D 12.
    /// It is created by <see cref="CD3D12Instance::EnumAdapters"/> and manages the lifetime
    /// of the underlying DXGI adapter and its associated instance.
    /// </remarks>
    class CD3D12Adapter final : public impl::ref_count_class<CD3D12Adapter, IRHIAdapter> {
    public:

        CD3D12Adapter(CD3D12Instance* pIns, IDXGIAdapter1*) noexcept;

        ~CD3D12Adapter() noexcept;

        void GetDesc(AdapterDesc*) noexcept override;

        auto&RefAdapter() noexcept { return *m_pDxgiAdapter; }

        auto&RefInstance() noexcept { return *m_pInstance; }

    protected:

        CD3D12Instance*             m_pInstance = nullptr;

        IDXGIAdapter1*              m_pDxgiAdapter = nullptr;

        AdapterDesc                 m_sDesc{};

    };
}}
#endif