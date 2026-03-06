#pragma once
#include "er_base.h"
// Slim Essential Renderer
#define SLIM_RHI_NO_DXC3_SHADER_COMPILER

namespace Slim { namespace RHI {

    struct ShaderMacro {
        const char*         name;
        const char*         definition;
    };

    enum COMPILE_FLAG : uint32_t {
        COMPILE_FLAG_DEBUG = 1 << 0,
        COMPILE_FLAG_SKIP_OPTIMIZATION = 1 << 1,
    };

    struct COMPILE_FLAGS { uint32_t flags; };

    struct DRHI_NO_VTABLE IRHIBlob : IRHIBase {

        virtual void* GetData() noexcept = 0;

        virtual size_t GetSize() noexcept = 0;

    };

    struct DRHI_NO_VTABLE IRHIShaderCompiler : IRHIBase {

        virtual CODE Compile(
            const void* data, size_t size, 
            const ShaderMacro* macros, size_t count,
            const char* target, const char* entry,
            uint32_t flags,
            IRHIBlob** ppCode, 
            IRHIBlob** ppError = nullptr
        ) noexcept = 0;
    };

}}
