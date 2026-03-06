#pragma once
#include <rhi/er_shader_compiler.h>
#ifndef SLIM_RHI_NO_SHADER_COMPILER
#include "../common/er_system_p.h"
#include "../common/er_helper_p.h"


namespace Slim { namespace RHI {
#ifndef SLIM_RHI_NO_DXC3_SHADER_COMPILER
    CODE CreateCompiler_DXC3(const char* dyLib, IRHIShaderCompiler** ppCompiler) noexcept;
#endif
    template<typename T>
    CODE CreateCompiler(const char* dyLib, IRHIShaderCompiler** ppCompiler)  noexcept {
        CODE code = CODE_OK;
        void* handle = nullptr;
        T* object = nullptr;

        // LOAD LIBFILE
        if (RHI::Success(code)) {
            handle = impl::dlopen(dyLib);
            if (!handle)
                code = CODE_FILE_NOT_FOUND;
        }

        // CREATE RHI OBJECT
        if (RHI::Success(code)) {
            object = new(std::nothrow) T{ handle };
            if (!object)
                code = CODE_OUTOFMEMORY;
        }

        // INIT
        if (RHI::Success(code)) {
            handle = nullptr;
            code = object->Init();
        }

        // OUTPUT
        if (RHI::Success(code)) {
            *ppCompiler = object;
            object = nullptr;
        }

        // CLEAN UP
        if (handle) impl::dlclose(handle);
        RHI::SafeDispose(object);

        return code;
    }

    CODE CreateShaderCompiler(
        const char* dyLib,
        IRHIShaderCompiler** ppCompiler,
        const char* type
    ) noexcept;


}}
#endif
