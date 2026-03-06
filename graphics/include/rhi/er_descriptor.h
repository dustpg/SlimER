#pragma once
#include "er_base.h"


namespace Slim { namespace RHI {

    struct IRHIImage;
    struct IRHIAttachment;
    struct SamplerDesc;
    enum SHADER_VISIBILITY : uint32_t;
    enum VOLATILITY : uint32_t;
    
    // Descriptor types
    enum DESCRIPTOR_TYPE : uint32_t {
        DESCRIPTOR_TYPE_CONST_BUFFER = 0,       // [b] CBV / Uniform Buffer
        DESCRIPTOR_TYPE_SHADER_RESOURCE,        // [t] SRV / Combined Image Sampler / Sampled Image
        DESCRIPTOR_TYPE_UNORDERED_ACCESS,       // [u] UAV / Storage Image / Storage Buffer
        DESCRIPTOR_TYPE_SAMPLER,                // [s] Sampler

        COUNT_OF_DESCRIPTOR_TYPE,

        DESCRIPTOR_TYPE_B = DESCRIPTOR_TYPE_CONST_BUFFER,
        DESCRIPTOR_TYPE_T = DESCRIPTOR_TYPE_SHADER_RESOURCE,
        DESCRIPTOR_TYPE_U = DESCRIPTOR_TYPE_UNORDERED_ACCESS,
        DESCRIPTOR_TYPE_S = DESCRIPTOR_TYPE_SAMPLER,
    };

    struct DescriptorSlotDesc {
        DESCRIPTOR_TYPE             type;           // Type of descriptor
        uint32_t                    binding;        // register
        SHADER_VISIBILITY           visibility;     // Which shader stages can access this range
        VOLATILITY                  volatility;     // Descriptor Volatility
    };

    // for vulkan
    struct DescriptorShiftFvk { uint32_t offset; };
    struct DescriptorShiftDescFvk { 
        DescriptorShiftFvk shift[COUNT_OF_DESCRIPTOR_TYPE]; 
    };

    struct DescriptorBufferDesc {
        uint32_t                    slotCount;
        const DescriptorSlotDesc*   slots;
        DescriptorShiftDescFvk      shift;
    };

    struct BindImageDesc {
        uint32_t                    slot;
        IRHIImage*                  image;
        RHI_FORMAT                  format;         // RHI_FORMAT_UNKNOWN or reinterpret
    };


    struct DRHI_NO_VTABLE IRHIDescriptorBuffer : IRHIBase {

        // Fixed

        virtual void Unbind(uint32_t slot) noexcept = 0;

        virtual CODE BindImage(const BindImageDesc* pDesc) noexcept = 0;

        virtual CODE BindSampler(uint32_t slot, const SamplerDesc* pDesc) noexcept = 0;

        CODE BindImage(const BindImageDesc& desc) noexcept { return BindImage(&desc); };

        CODE BindSampler(uint32_t slot, const SamplerDesc& desc) noexcept { return BindSampler(slot , &desc); };

        // Bindless

    };

}}