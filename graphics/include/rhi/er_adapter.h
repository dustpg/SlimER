#pragma once
#include "er_base.h"

namespace Slim { namespace RHI {

    enum class API : uint32_t;

    enum ADAPTER_FEATURE : uint32_t {
        ADAPTER_FEATURE_NONE    = 0,
        ADAPTER_FEATURE_CPU     = 1 << 0,
        ADAPTER_FEATURE_2D      = 1 << 1,
    };

    enum ADAPTER_VIDEO_FEATURE : uint32_t {
        ADAPTER_VIDEO_FEATURE_NONE = 0,
    };

    struct AdapterDesc {
        uint64_t                vram;
        API                     api;
        ADAPTER_FEATURE         feature;
        ADAPTER_VIDEO_FEATURE   video;
        uint32_t                vendorID;
        uint32_t                deviceID;
        char                    name[256];
    };


    struct DRHI_NO_VTABLE IRHIAdapter : IRHIBase {

        virtual void GetDesc(AdapterDesc*) noexcept = 0;

    };

}}