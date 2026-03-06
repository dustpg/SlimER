#pragma once

#include <cstdint>
#include <cassert>
#include <utility>

#if defined(__clang__)
// [[clang::novtable]]
#define DRHI_NO_VTABLE __declspec(novtable)
#elif defined(_MSC_VER)
#define DRHI_NO_VTABLE __declspec(novtable)
#else
#define DRHI_NO_VTABLE
#endif

namespace Slim { namespace RHI {

    enum CODE : int32_t {
        CODE_OK = 0,
        CODE_FALSE = 1,
    
        CODE_NOTIMPL        = (int32_t)0x80004001,
        CODE_NOINTERFACE    = (int32_t)0x80004002,
        CODE_POINTER        = (int32_t)0x80004003,
        CODE_ABORT          = (int32_t)0x80004004,
        CODE_FAILED         = (int32_t)0x80004005,
        CODE_UNEXPECTED     = (int32_t)0x8000FFFF,
        CODE_FILE_NOT_FOUND = (int32_t)0x80070002,
        CODE_HANDLE         = (int32_t)0x80070006,
        CODE_INVALIDARG     = (int32_t)0x80070057,
        CODE_OUTOFMEMORY    = (int32_t)0x8007000E,
        CODE_SMALLBUFFER    = (int32_t)0x8007007A,
        CODE_ENDOFSTREAM    = (int32_t)0x80070026,
        CODE_TIMEOUT        = (int32_t)0x8001011F,
        
        CODE_RHI_NOT_FOUND  = (int32_t)0x887A0002,
        CODE_RHI_UNSUPPORTED= (int32_t)0x887A0004,
    };

    enum WAIT : uint32_t {
        //WAIT_CODE_INDEX_0 = 0,
        WAIT_CODE_SUCCESS = 0,
        WAIT_CODE_TIME_OUT = uint32_t(-1),
        WAIT_CODE_FAILED = uint32_t(-2),
        WAIT_CODE_ABANDONED = uint32_t(-3),
    };
    // 
    enum : uint32_t {
        WAIT_FOREVER = ~uint32_t(0),
        // D3D12 ROOT SIGNATURE = 256, ROOT DESCRIPTOR = 8, MAX = 32. BUT < 16 RECOMMENDED
        MAX_PUSH_DESCRIPTOR = 32,
        // VULKAN
        MAX_PUSH_CONSTANT_SIZE = 128,
    };

    // Customizable Constant
    enum : uint32_t {
        MAX_FRAMES_IN_FLIGHT = 2, // 1~3
        SWAPCHAIN_MAX_BUFFER_COUNT = 8,
        MAX_WAIT_TIMEOUT = 10 * 1000,
    };

    inline bool Success(CODE code) noexcept { return int32_t(code) >= 0; }

    inline bool Failure(CODE code) noexcept { return int32_t(code) < 0; }

    struct DRHI_NO_VTABLE IRHIBase {

        virtual long Dispose() noexcept = 0;

        virtual long AddRefCnt() noexcept = 0;

        virtual CODE InnerQuery(const void* data, size_t len, void** output) noexcept = 0;

    };

    struct DRHI_NO_VTABLE IRHIDeviceResource : IRHIBase {

        virtual void SetDebugName(const char* name) noexcept = 0;

    };

    enum RHI_FORMAT : uint32_t {
        RHI_FORMAT_UNKNOWN = 0,

        // Main Texture Format
        RHI_FORMAT_RGBA8_UNORM,
        RHI_FORMAT_BGRA8_UNORM,
        RHI_FORMAT_RGBA8_SRGB,
        RHI_FORMAT_BGRA8_SRGB,

        // 16-bit formats
        RHI_FORMAT_R16_UNORM,
        RHI_FORMAT_R16_SNORM,
        RHI_FORMAT_R16_UINT,
        RHI_FORMAT_R16_SINT,
        RHI_FORMAT_R16_FLOAT,
        RHI_FORMAT_RG16_UNORM,
        RHI_FORMAT_RG16_SNORM,
        RHI_FORMAT_RG16_UINT,
        RHI_FORMAT_RG16_SINT,
        RHI_FORMAT_RG16_FLOAT,
        RHI_FORMAT_RGBA16_UNORM,
        RHI_FORMAT_RGBA16_SNORM,
        RHI_FORMAT_RGBA16_UINT,
        RHI_FORMAT_RGBA16_SINT,
        RHI_FORMAT_RGBA16_FLOAT,

        // 32-bit formats
        RHI_FORMAT_R32_UINT,
        RHI_FORMAT_R32_SINT,
        RHI_FORMAT_R32_FLOAT,
        RHI_FORMAT_RG32_UINT,
        RHI_FORMAT_RG32_SINT,
        RHI_FORMAT_RG32_FLOAT,
        RHI_FORMAT_RGB32_UINT,
        RHI_FORMAT_RGB32_SINT,
        RHI_FORMAT_RGB32_FLOAT,
        RHI_FORMAT_RGBA32_UINT,
        RHI_FORMAT_RGBA32_SINT,
        RHI_FORMAT_RGBA32_FLOAT,

        // Depth Stencil
        // TODO: check support
        RHI_FORMAT_D24_UNORM_S8_UINT,
        RHI_FORMAT_D32_FLOAT,

        // VIDEO FORMAT
#if 0
        RHI_FORMAT_NV12,            // YUV 4:2:0
        RHI_FORMAT_YUY2,            // YUV 4:2:2
#endif
    };


    template<typename T>
    inline auto SafeRefCnt(T* p) noexcept { if (p) p->AddRefCnt(); return p; }

    template<typename T>
    inline void SafeDispose(T*& p) noexcept { if (p) { p->Dispose(); p = nullptr; } }

    template<typename T>
    struct base_ptr {
        base_ptr() noexcept {};
        ~base_ptr() noexcept { SafeDispose(ptr_); }
        base_ptr(const base_ptr<T>& ptr) noexcept : ptr_(SafeRefCnt(ptr.ptr_)) {}
        base_ptr(base_ptr<T>&& ptr) noexcept : ptr_(ptr.ptr_) { ptr.ptr_ = nullptr; }
        base_ptr<T>& operator=(base_ptr<T>&& ptr) noexcept { assert(this != &ptr); std::swap(ptr_, ptr.ptr_); return*this; }
        base_ptr<T>& operator=(const base_ptr<T>& ptr) noexcept = delete;
        T* operator->() const noexcept { return ptr_; }
        T** as_get() noexcept { SafeDispose(ptr_); return &ptr_; };
        T*const* as_set() const noexcept { return &ptr_; };
        operator T*() const noexcept { return ptr_; }
    protected:
        T* ptr_ = nullptr;
    };
    
#ifndef DRHI_LOGGER_ERR
#define DRHI_LOGGER_ERR(fmt, ...) std::printf("[LOG] " fmt "\n", ##__VA_ARGS__)
#endif
}}
