#pragma once
#include "er_base.h"
// Slim Essential Renderer

namespace Slim { namespace RHI {

    struct SwapChainDesc {
        void*           windowHandle; 
        uint32_t        width;
        uint32_t        height;
        uint32_t        bufferCount;    // Minimum value; actual count may be larger. Call GetDesc() to get the actual value.
        RHI_FORMAT      format;
    };

    struct FrameContext {
        uint32_t        indexInFlight;
        uint32_t        indexBuffer;
    };
    
    struct IRHICommandQueue;
    struct IRHIImage;

    struct DRHI_NO_VTABLE IRHISwapChain : IRHIBase {

        virtual void GetDesc(SwapChainDesc* pDesc) noexcept = 0;

        virtual void GetBoundQueue(IRHICommandQueue** ppQueue) noexcept = 0;

        virtual CODE AcquireFrame(FrameContext& ctx) noexcept = 0;

        virtual CODE PresentFrame(uint32_t syncInterval) noexcept = 0;

        virtual CODE Resize(uint32_t width, uint32_t height) noexcept = 0;

        virtual CODE GetImage(uint32_t index, IRHIImage** image) noexcept = 0;

    };

}}