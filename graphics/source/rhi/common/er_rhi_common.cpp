#include "er_rhi_common.h"
#include <rhi/er_pipeline.h>
#include <cassert>
#include <cstring>
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Slim { namespace impl {

    uint32_t format_size(RHI::RHI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case RHI::RHI_FORMAT_RGBA8_UNORM:
        case RHI::RHI_FORMAT_BGRA8_UNORM:
        case RHI::RHI_FORMAT_RGBA8_SRGB:
        case RHI::RHI_FORMAT_BGRA8_SRGB:
            return 4; // 4 * 8-bit = 4 bytes

            // 16-bit formats
        case RHI::RHI_FORMAT_R16_UNORM:
        case RHI::RHI_FORMAT_R16_SNORM:
        case RHI::RHI_FORMAT_R16_UINT:
        case RHI::RHI_FORMAT_R16_SINT:
        case RHI::RHI_FORMAT_R16_FLOAT:
            return 2; // 1 * 16-bit = 2 bytes
        case RHI::RHI_FORMAT_RG16_UNORM:
        case RHI::RHI_FORMAT_RG16_SNORM:
        case RHI::RHI_FORMAT_RG16_UINT:
        case RHI::RHI_FORMAT_RG16_SINT:
        case RHI::RHI_FORMAT_RG16_FLOAT:
            return 4; // 2 * 16-bit = 4 bytes
        case RHI::RHI_FORMAT_RGBA16_UNORM:
        case RHI::RHI_FORMAT_RGBA16_SNORM:
        case RHI::RHI_FORMAT_RGBA16_UINT:
        case RHI::RHI_FORMAT_RGBA16_SINT:
        case RHI::RHI_FORMAT_RGBA16_FLOAT:
            return 8; // 4 * 16-bit = 8 bytes

            // 32-bit formats
        case RHI::RHI_FORMAT_R32_UINT:
        case RHI::RHI_FORMAT_R32_SINT:
        case RHI::RHI_FORMAT_R32_FLOAT:
            return 4; // 1 * 32-bit = 4 bytes
        case RHI::RHI_FORMAT_RG32_UINT:
        case RHI::RHI_FORMAT_RG32_SINT:
        case RHI::RHI_FORMAT_RG32_FLOAT:
            return 8; // 2 * 32-bit = 8 bytes
        case RHI::RHI_FORMAT_RGB32_UINT:
        case RHI::RHI_FORMAT_RGB32_SINT:
        case RHI::RHI_FORMAT_RGB32_FLOAT:
            return 12; // 3 * 32-bit = 12 bytes
        case RHI::RHI_FORMAT_RGBA32_UINT:
        case RHI::RHI_FORMAT_RGBA32_SINT:
        case RHI::RHI_FORMAT_RGBA32_FLOAT:
            return 16; // 4 * 32-bit = 16 bytes

        // Depth Stencil
        case RHI::RHI_FORMAT_D24_UNORM_S8_UINT:
            return 4; // 24-bit depth + 8-bit stencil = 4 bytes
        case RHI::RHI_FORMAT_D32_FLOAT:
            return 4; // 32-bit depth = 4 bytes

        default:
            return 0; // Unknown format
        }

    }

    void rearrange(
        void* dst, const void* src, 
        uint32_t widthInByte, uint32_t heightInLine, 
        uint32_t dstPitch, uint32_t srcPitch) noexcept
    {
        assert(dst && src && widthInByte && heightInLine && dstPitch && srcPitch);
        
        const uint8_t* srcPtr = static_cast<const uint8_t*>(src);
        uint8_t* dstPtr = static_cast<uint8_t*>(dst);
        
        // Copy each row, handling different pitches
        for (uint32_t y = 0; y < heightInLine; ++y) {
            std::memcpy(dstPtr + size_t(y) * size_t(dstPitch), srcPtr + size_t(y) * size_t(srcPitch), widthInByte);
        }
    }

    // Calculate descriptor set / space indices for static sampler and descriptor buffer
    // Layout rule (keep consistent with Vulkan backend):
    //   [push] [static] [buffer]           when !staticSamplerOnBack
    //   [push] [buffer] [static]           when staticSamplerOnBack
    // push: uses dedicated mechanism (root constants / root descriptors),
    // static/buffer: mapped to space = set index.
    void calculate_setid(
        const RHI::PipelineLayoutDesc& desc,
        uint32_t& uStaticSamplerSpace,
        uint32_t& uDescriptorBufferSpace
    ) noexcept {
        // Calculate descriptor set indices: [push] [static] [buffer]
        uint32_t setIndex = 0;

        if (desc.pushDescriptorCount)
            setIndex = 1;

        const bool hasStaticSampler = desc.staticSamplerCount != 0;
        const bool hasDescriptorBuffer = desc.descriptorBuffer != nullptr;

        uStaticSamplerSpace = 0;
        uDescriptorBufferSpace = 0;

        if (hasStaticSampler && hasDescriptorBuffer) {
            // Both static sampler set and descriptor buffer are present
            if (desc.staticSamplerOnBack) {
                // Layout: [push] [buffer] [static]
                uDescriptorBufferSpace = setIndex;
                uStaticSamplerSpace = setIndex + 1;
            }
            else {
                // Layout: [push] [static] [buffer]
                uStaticSamplerSpace = setIndex;
                uDescriptorBufferSpace = setIndex + 1;
            }
        }
        else if (hasStaticSampler && !hasDescriptorBuffer) {
            // Only static sampler set
            uStaticSamplerSpace = setIndex;
        }
        else if (!hasStaticSampler && hasDescriptorBuffer) {
            // Only descriptor buffer
            uDescriptorBufferSpace = setIndex;
        }
    }

    uint32_t popcount_fallback(uint32_t x) noexcept {
        uint32_t c = 0;
        while (x) {
            x &= x - 1;
            ++c;
        }
        return c;
    }

    uint32_t popcount(uint32_t value) noexcept {
#ifdef _MSC_VER
        return __popcnt(value);
#elif defined(__GNUC__) || defined(__clang__)
        return __builtin_popcount(value);
#else
        return popcount_fallback(value);
#endif
    }

}}

