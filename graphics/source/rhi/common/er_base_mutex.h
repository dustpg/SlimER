#pragma once
#include "er_helper_p.h"
#ifdef _WIN32
namespace Slim { namespace RHI {
    enum : size_t { SIZE_OF_BASE_MUTEX = sizeof(void*) };
    enum : size_t { ALIGN_OF_BASE_MUTEX = alignof(void*) };
}}
#else

#endif

namespace Slim { namespace RHI {

    class CBaseMutex final {

    public:

        CBaseMutex() noexcept;

        ~CBaseMutex() noexcept;

        void Lock() noexcept;

        void UnLock() noexcept;

    private:

        alignas(ALIGN_OF_BASE_MUTEX) char   m_szBuffer[SIZE_OF_BASE_MUTEX];

    };


}}