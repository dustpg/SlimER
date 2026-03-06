#include "er_shader_compiler_p.h"
#ifndef SLIM_RHI_NO_SHADER_COMPILER
#ifndef SLIM_RHI_NO_DXC3_SHADER_COMPILER
#include <cassert>
#include <cstring>
#include "er_blob.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>
// FROM WINDOWS SDK
#include <dxcapi.h>
#else
// FROM VULKAN SDK
#include <dxc/WinAdapter.h>
#include <dxc/dxcapi.h>
#endif

namespace Slim { namespace RHI {

    class CDxc3ShaderCompiler final : public impl::ref_count_class<CDxc3ShaderCompiler, IRHIShaderCompiler>  {
    public:

        CDxc3ShaderCompiler(void* handle) noexcept;

        ~CDxc3ShaderCompiler() noexcept;

        CODE Init() noexcept;

    public:

        CODE Compile(
            const void* data, size_t size,
            const ShaderMacro* macros, size_t count,
            const char* target, const char* entry,
            uint32_t flags,
            IRHIBlob** ppCode,
            IRHIBlob** ppError
        ) noexcept override;

    protected:

        void*                       m_pHandle = nullptr;

        DxcCreateInstanceProc       m_fnCreate = nullptr;
        // TODO: DxcCreateInstance2Proc
        //DxcCreateInstance2Proc      m_fnCreate2 = nullptr;

        IDxcCompiler3*              m_pDxcCompiler = nullptr;

        IDxcUtils*                  m_pDxcUtils = nullptr;

    };


    CDxc3ShaderCompiler::CDxc3ShaderCompiler(void* handle) noexcept
        : m_pHandle(handle)
    {
        assert(handle);
    }

    CDxc3ShaderCompiler::~CDxc3ShaderCompiler() noexcept
    {
        if (m_pDxcUtils)
            m_pDxcUtils->Release();
        if (m_pDxcCompiler)
            m_pDxcCompiler->Release();
        RHI::dlclose(m_pHandle);
    }

    CODE CDxc3ShaderCompiler::Init() noexcept
    {

        assert(m_fnCreate == nullptr);
        m_fnCreate = static_cast<DxcCreateInstanceProc>(RHI::dlsym(m_pHandle, "DxcCreateInstance"));
        if (!m_fnCreate)
            return CODE_FILE_NOT_FOUND;

        //m_fnCreate2 = static_cast<DxcCreateInstance2Proc>(RHI::dlsym(m_pHandle, "DxcCreateInstance2"));


        HRESULT hr = m_fnCreate(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void**)&m_pDxcCompiler);
        if (FAILED(hr))
            return CODE(hr);

        hr = m_fnCreate(CLSID_DxcUtils, __uuidof(IDxcUtils), (void**)&m_pDxcUtils);
        if (FAILED(hr)) {
            return CODE(hr);
        }

        return CODE_OK;
    }

    CODE CDxc3ShaderCompiler::Compile(const void* data, size_t size, const ShaderMacro* macros, size_t count, const char* target, const char* entry, uint32_t flags, IRHIBlob** ppCode, IRHIBlob** ppError) noexcept
    {
        if (!data || !size || !target || !entry || !ppCode)
            return CODE_POINTER;

        if (!m_pDxcCompiler || !m_pDxcUtils)
            return CODE_UNEXPECTED;

        // Initialize output pointers
        if (ppCode) *ppCode = nullptr;
        if (ppError) *ppError = nullptr;

        // Create source blob
        IDxcBlobEncoding* pSourceBlob = nullptr;
        HRESULT hr = m_pDxcUtils->CreateBlobFromPinned((LPBYTE)data, (UINT32)size, CP_UTF8, &pSourceBlob);
        if (FAILED(hr))
            return CODE(hr);

        // Helper function to convert char* to wstring (assuming UTF-8 input)
        auto toWString = [](const char* str) -> std::wstring {
            if (!str) return std::wstring();
            int len = (int)strlen(str);
            if (len == 0) return std::wstring();
#ifdef _WIN32
            int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, nullptr, 0);
            if (wlen <= 0) return std::wstring();
            std::wstring result(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str, len, &result[0], wlen);
            return result;
#else
            // On non-Windows, assume UTF-8 and convert
            std::mbstate_t state{};
            const char* p = str;
            std::wstring result;
            while (*p) {
                wchar_t wc;
                size_t converted = std::mbrtowc(&wc, p, MB_CUR_MAX, &state);
                if (converted == (size_t)-1 || converted == (size_t)-2) break;
                if (converted == 0) break;
                result += wc;
                p += converted;
            }
            return result;
#endif
        };

        // Convert macros to DxcDefine (DxcDefine uses LPCWSTR)
        std::vector<DxcDefine> dxcDefines;
        std::vector<std::wstring> macroWStrings;
        if (macros && count > 0) {
            macroWStrings.reserve(count * 2);
            dxcDefines.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                if (macros[i].name) {
                    macroWStrings.push_back(toWString(macros[i].name));
                    std::wstring defValue = macros[i].definition ? toWString(macros[i].definition) : std::wstring();
                    macroWStrings.push_back(defValue);
                    DxcDefine define;
                    define.Name = macroWStrings[macroWStrings.size() - 2].c_str();
                    define.Value = macroWStrings[macroWStrings.size() - 1].empty() ? nullptr : macroWStrings[macroWStrings.size() - 1].c_str();
                    dxcDefines.push_back(define);
                }
            }
        }

        // Build arguments
        std::vector<LPCWSTR> arguments;
        std::vector<std::wstring> wstringStorage;

        // Entry point
        if (entry) {
            wstringStorage.push_back(toWString(entry));
            arguments.push_back(L"-E");
            arguments.push_back(wstringStorage.back().c_str());
        }

        // Target profile
        if (target) {
            wstringStorage.push_back(toWString(target));
            arguments.push_back(L"-T");
            arguments.push_back(wstringStorage.back().c_str());
        }

        // Flags
        if (flags & COMPILE_FLAG_DEBUG) {
            arguments.push_back(L"-Zi"); // Debug info
            arguments.push_back(L"-Od"); // Disable optimizations
        }
        if (flags & COMPILE_FLAG_SKIP_OPTIMIZATION) {
            arguments.push_back(L"-Od"); // Disable optimizations
        }

        // Prepare compile arguments
        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = pSourceBlob->GetBufferPointer();
        sourceBuffer.Size = pSourceBlob->GetBufferSize();
        sourceBuffer.Encoding = 0; // Assume binary

        // Compile
        IDxcResult* pResults = nullptr;
        hr = m_pDxcCompiler->Compile(
            &sourceBuffer,
            arguments.data(),
            (UINT32)arguments.size(),

            //dxcDefines.empty() ? nullptr : dxcDefines.data(),
            //(UINT32)dxcDefines.size(),

            nullptr, // Include handler
            IID_PPV_ARGS(&pResults)
        );

        pSourceBlob->Release();

        if (FAILED(hr)) {
            if (pResults) pResults->Release();
            return CODE(hr);
        }

        // Get status
        HRESULT statusHR = S_OK;
        hr = pResults->GetStatus(&statusHR);
        if (FAILED(hr)) {
            pResults->Release();
            return CODE(hr);
        }

        // Get error/warning blob (if requested)
        if (ppError) {
            IDxcBlobUtf8* pErrorBlob = nullptr;
            if (SUCCEEDED(pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrorBlob), nullptr))) {
                if (pErrorBlob && pErrorBlob->GetStringLength() > 0) {
                    *ppError = RHI::CreateBlob(pErrorBlob->GetBufferPointer(), pErrorBlob->GetStringLength());
                    if (*ppError) {
                        (*ppError)->AddRefCnt();
                    }
                }
                if (pErrorBlob) pErrorBlob->Release();
            }
        }

        // Get compiled code blob (only if compilation succeeded)
        if (SUCCEEDED(statusHR)) {
            IDxcBlob* pCodeBlob = nullptr;
            if (SUCCEEDED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pCodeBlob), nullptr))) {
                if (pCodeBlob && pCodeBlob->GetBufferSize() > 0) {
                    *ppCode = RHI::CreateBlob(pCodeBlob->GetBufferPointer(), pCodeBlob->GetBufferSize());
                    if (*ppCode) {
                        (*ppCode)->AddRefCnt();
                    }
                }
                if (pCodeBlob) pCodeBlob->Release();
            }
        }

        pResults->Release();

        return CODE(statusHR);
    }


}}

CODE CreateCompiler_DXC3(const char* dyLib, IRHIShaderCompiler** ppCompiler) noexcept
{
    return CreateCompiler<CDxc3ShaderCompiler>(dyLib, ppCompiler);
}

#endif

namespace Slim { namespace RHI {

    CODE CreateShaderCompilerImpl(const char* dyLib, IRHIShaderCompiler** ppCompiler) noexcept
    {
        // dxc
#ifndef SLIM_RHI_NO_DXC3_SHADER_COMPILER
        if (!RHI::strncasecmp(dyLib, "dxcompiler", 10)) {
            return CreateCompiler_DXC3(dyLib, ppCompiler);
        }
#endif
        return CODE_INVALIDARG;
    }


    CODE CreateShaderCompiler(const char* dyLib, IRHIShaderCompiler** ppCompiler, const char* type) noexcept
    {
        if (!dyLib || !ppCompiler)
            return CODE_POINTER;
        // TODO: dyLib == ""
        return RHI::CreateShaderCompilerImpl(dyLib, ppCompiler);
    }
}}


#endif
