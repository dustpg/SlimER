#pragma once

namespace Slim { namespace impl {

    void*dlopen(const char* dlLib) noexcept;

    void*dlsym(void* handle, const char* symbol);

    void dlclose(void* handle) noexcept;

    int strncasecmp(const char* string1, const char* string2, size_t count) noexcept;

}}