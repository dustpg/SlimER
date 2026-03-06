#include "er_system_p.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <string.h>

using namespace Slim;

void* Slim::impl::dlopen(const char* dlLib) noexcept
{
    return ::LoadLibraryA(dlLib);
}

void* Slim::impl::dlsym(void* handle, const char* symbol)
{
    return ::GetProcAddress(static_cast<HMODULE>(handle), symbol);
}

void Slim::impl::dlclose(void* handle) noexcept
{
    ::FreeLibrary(static_cast<HMODULE>(handle));
}

int Slim::impl::strncasecmp(const char* string1, const char* string2, size_t count) noexcept
{
    return ::_strnicmp(string1, string2, count);
}
