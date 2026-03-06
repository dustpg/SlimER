#pragma once
#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"

struct ID3D12Debug;
struct IDXGIFactory4;

namespace Slim { namespace RHI {

    /// <summary>
    /// Represents a Direct3D 12 instance implementation of the RHI instance interface.
    /// </summary>
    /// <remarks>
    /// This class manages the Direct3D 12 debug layer and DXGI factory, and provides methods
    /// to enumerate adapters and create devices. It is created by <see cref="CreateInstanceD3D12"/>.
    /// </remarks>
    class CD3D12Instance final : public impl::ref_count_class<CD3D12Instance, IRHIInstance> {
    public:

        CD3D12Instance() noexcept;

        ~CD3D12Instance() noexcept;

        CODE Init(const InstanceDesc* pDesc) noexcept;

        auto&RefFactory() noexcept { return *m_pDxgiFactory; }

        //auto IsDebug() const noexcept { return m_pD3d12Debug; }

    public:

        CODE EnumAdapters(uint32_t index, IRHIAdapter**) noexcept override;

        CODE GetDefaultAdapter(IRHIAdapter**) noexcept override;

        CODE CreateDevice(IRHIAdapter* pAdapter, IRHIDevice** ppDevice) noexcept override;

        CODE CreateShaderCompiler(const char* dyLib, IRHIShaderCompiler**) noexcept override;

    protected:

        ID3D12Debug*                m_pD3d12Debug = nullptr;

        IDXGIFactory4*              m_pDxgiFactory = nullptr;

    };

    CODE CreateInstanceD3D12(const InstanceDesc* pDesc, IRHIInstance** ptr) noexcept;
}}
#endif
