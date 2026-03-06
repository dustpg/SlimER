#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_pipeline.h>
#include <rhi/er_command.h>
#include <rhi/er_resource.h>
#include <vulkan/vulkan.h>
#include <type_traits>
#include "../common/er_rhi_common.h"

namespace Slim { namespace RHI { class CVulkanDevice; } }

extern "C" {
    extern const VkAllocationCallbacks gc_sAllocationCallbackDRHI;
}

namespace Slim { namespace impl {

    RHI::CODE vulkan_rhi(VkResult) noexcept;

    VkFormat rhi_vulkan(RHI::RHI_FORMAT) noexcept;

    VkImageAspectFlags rhi_vulkan(RHI::ATTACHMENT_TYPE) noexcept;

    VkImageAspectFlags rhi_vulkan_aspect_mask(RHI::RHI_FORMAT) noexcept;

    VkShaderStageFlags rhi_vulkan(RHI::SHADER_VISIBILITY) noexcept;

    VkAttachmentLoadOp rhi_vulkan(RHI::PASS_LOAD_OP) noexcept;

    VkAttachmentStoreOp rhi_vulkan(RHI::PASS_STORE_OP) noexcept;

    VkVertexInputRate rhi_vulkan(RHI::VERTEX_INPUT_RATE) noexcept;

    VkPrimitiveTopology rhi_vulkan(RHI::PRIMITIVE_TOPOLOGY) noexcept;

    VkSampleCountFlagBits rhi_vulkan(RHI::SampleCount) noexcept;

    VkPolygonMode rhi_vulkan(RHI::POLYGON_MODE) noexcept;

    VkCullModeFlags rhi_vulkan(RHI::CULL_MODE) noexcept;

    VkFrontFace rhi_vulkan(RHI::FRONT_FACE) noexcept;

    VkCompareOp rhi_vulkan(RHI::COMPARE_OP) noexcept;

    VkStencilOp rhi_vulkan(RHI::STENCIL_OP) noexcept;

    VkBool32 rhi_vulkan(RHI::DEPTH_WRITE_MASK) noexcept;

    VkLogicOp rhi_vulkan(RHI::LOGIC_OP) noexcept;

    VkBlendFactor rhi_vulkan(RHI::BLEND_FACTOR) noexcept;

    VkBlendOp rhi_vulkan(RHI::BLEND_OP) noexcept;

    VkColorComponentFlags rhi_vulkan(RHI::COLOR_COMPONENT_FLAGS) noexcept;

    VkImageLayout rhi_vulkan(RHI::IMAGE_LAYOUT) noexcept;

    // Descriptor type conversion (CBV/SRV/UAV/Sampler -> Vulkan descriptor)
    VkDescriptorType rhi_vulkan(RHI::DESCRIPTOR_TYPE) noexcept;

    VkBufferUsageFlags rhi_vulkan(RHI::BUFFER_USAGES) noexcept;

    VkImageUsageFlags rhi_vulkan(RHI::IMAGE_USAGES) noexcept;

    VkImageTiling rhi_vulkan_tiling(RHI::MEMORY_TYPE) noexcept;

    VkIndexType rhi_vulkan(RHI::INDEX_TYPE) noexcept;

    VkFilter rhi_vulkan(RHI::FILTER_MODE) noexcept;

    VkSamplerAddressMode rhi_vulkan(RHI::ADDRESS_MODE) noexcept;

    VkSamplerMipmapMode rhi_vulkan(RHI::MIPMAP_MODE) noexcept;

    VkBorderColor rhi_vulkan(RHI::BORDER_COLOR) noexcept;

    // Convert SamplerDesc to VkSamplerCreateInfo
    void rhi_vulkan_cvt(const RHI::SamplerDesc* pDesc, VkSamplerCreateInfo* pOut) noexcept;

    //VkBufferUsageFlags rhi_vulkan(RHI::BUFFER_TYPE) noexcept;

    // Set debug name for Vulkan objects using VK_EXT_debug_utils
    void vulkan_set_debug_name(RHI::CVulkanDevice* device, VkObjectType objectType, uint64_t objectHandle, const char* name) noexcept;

    template<typename T> inline
    uint64_t vulkan_handle(T handle) noexcept {
        if constexpr (std::is_pointer_v<VkBuffer>)
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
        else 
            return static_cast<uint64_t>(handle);
    }
}}
#endif