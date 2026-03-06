#pragma once
#include "er_base.h"


namespace Slim { namespace RHI {

    enum class API : uint32_t {
        NONE = 0,
        VULKAN,
        D3D12,
        //METAL,  // TODO:
        //D3D11,  // TODO:
    };

    struct IRHIAdapter;
    struct IRHIDevice;
    struct IRHIShaderCompiler;

    struct DRHI_NO_VTABLE IRHIInstance : IRHIBase {

        virtual CODE EnumAdapters(uint32_t index, IRHIAdapter** ppAdapter) noexcept = 0;

        virtual CODE GetDefaultAdapter(IRHIAdapter** ppAdapter) noexcept = 0;

        virtual CODE CreateDevice(IRHIAdapter* pAdapter, IRHIDevice** ppDevice) noexcept = 0;
        // Optional Build-in CompilerHelper
        virtual CODE CreateShaderCompiler(const char* dyLib, IRHIShaderCompiler**) noexcept = 0;

    };

    struct InstanceDesc {
        API         api;
        bool        debug;
    };

}}


extern "C" Slim::RHI::CODE 
CreateSlimRHIInstance(const Slim::RHI::InstanceDesc* pDesc, Slim::RHI::IRHIInstance**) noexcept;

namespace Slim { namespace RHI {

    static inline CODE CreateInstance(const Slim::RHI::InstanceDesc& desc, Slim::RHI::IRHIInstance** ppIns) noexcept {
        return ::CreateSlimRHIInstance(&desc, ppIns);
    }
}}
