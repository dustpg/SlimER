#pragma once

#include <rhi/er_base.h>

namespace Slim { namespace RHI {
    struct PipelineLayoutDesc;
}}

namespace Slim { namespace impl {

    uint32_t format_size(RHI::RHI_FORMAT) noexcept;

    void rearrange(
        void* dst, const void* src, 
        uint32_t widthInByte, uint32_t heightInLine,
        uint32_t dstPitch, uint32_t srcPitch
    ) noexcept;

    void calculate_setid(
        const RHI::PipelineLayoutDesc& desc,
        uint32_t& uStaticSamplerSetId,
        uint32_t& uDescriptorBufferSetId
    ) noexcept;


    uint32_t popcount(uint32_t value) noexcept;

}}

