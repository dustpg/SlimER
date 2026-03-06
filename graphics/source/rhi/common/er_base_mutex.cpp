#include "er_base_mutex.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif


Slim::RHI::CBaseMutex::CBaseMutex() noexcept
{
#ifdef _WIN32
    static_assert(sizeof(SRWLOCK) == SIZE_OF_BASE_MUTEX, "SRWLOCK");
    static_assert(alignof(SRWLOCK) == ALIGN_OF_BASE_MUTEX, "SRWLOCK");
    ::InitializeSRWLock(reinterpret_cast<PSRWLOCK>(m_szBuffer));
#else
    static_assert(false, "TODO");
#endif
}


Slim::RHI::CBaseMutex::~CBaseMutex() noexcept
{
#ifdef _WIN32
    // SRWLock does not require explicit cleanup
#else
    static_assert(false, "TODO");
#endif
}


void Slim::RHI::CBaseMutex::Lock() noexcept
{
#ifdef _WIN32
    ::AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(m_szBuffer));
#else
    static_assert(false, "TODO");
#endif
}


void Slim::RHI::CBaseMutex::UnLock() noexcept
{
#ifdef _WIN32
    ::ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(m_szBuffer));
#else
    static_assert(false, "TODO");
#endif
}