#pragma once
#include <cstdint>

namespace Slim { namespace RHI {

    struct IRHIBlob;

    IRHIBlob* CreateBlob(void*, size_t) noexcept;

}}