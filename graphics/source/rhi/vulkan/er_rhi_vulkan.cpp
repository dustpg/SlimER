#include "er_rhi_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cstring>
#include "er_vulkan_device.h"
#include "er_vulkan_adapter.h"
#include "../common/er_helper_p.h"

using namespace Slim;

namespace Slim { namespace impl {
    void* VKAPI_CALL vkAllocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) noexcept {
        (void)pUserData;
        (void)alignment;
        (void)allocationScope;
        return base_class::imalloc(size);
    }

    void* VKAPI_CALL vkReallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) noexcept {
        (void)pUserData;
        (void)alignment;
        (void)allocationScope;
        return base_class::irealloc(pOriginal, size);
    }

    void VKAPI_CALL vkFree(void* pUserData, void* pMemory) noexcept {
        (void)pUserData;
        base_class::ifree(pMemory);
    }

    void VKAPI_CALL vkInternalAllocation(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) noexcept {
        (void)pUserData;
        (void)size;
        (void)allocationType;
        (void)allocationScope;
    }

    void VKAPI_CALL vkInternalFree(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) noexcept {
        (void)pUserData;
        (void)size;
        (void)allocationType;
        (void)allocationScope;
    }
}}

extern "C" {
    const VkAllocationCallbacks gc_sAllocationCallbackDRHI = {
        nullptr,
        Slim::impl::vkAllocation,
        Slim::impl::vkReallocation,
        Slim::impl::vkFree,
        Slim::impl::vkInternalAllocation,
        Slim::impl::vkInternalFree
    };
}

RHI::CODE Slim::impl::vulkan_rhi(VkResult result) noexcept
{
    // TODO: convert
    switch (result)
    {
    case VK_SUCCESS:
        return RHI::CODE_OK;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        // TODO: VK_ERROR_OUT_OF_DEVICE_MEMORY?
        return RHI::CODE_OUTOFMEMORY;
    default:
        return RHI::CODE_FAILED;
    }
}

VkShaderStageFlags Slim::impl::rhi_vulkan(RHI::SHADER_VISIBILITY visibility) noexcept
{
    VkShaderStageFlags stageFlags{};
    switch (visibility)
    {
    case RHI::SHADER_VISIBILITY_ALL:
        stageFlags = VK_SHADER_STAGE_ALL;
        break;
    case RHI::SHADER_VISIBILITY_VERTEX:
        stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case RHI::SHADER_VISIBILITY_HULL:
        stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case RHI::SHADER_VISIBILITY_DOMAIN:
        stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case RHI::SHADER_VISIBILITY_GEOMETRY:
        stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case RHI::SHADER_VISIBILITY_PIXEL:
        stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case RHI::SHADER_VISIBILITY_AMPLIFICATION:
        stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
        break;
    case RHI::SHADER_VISIBILITY_MESH:
        stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;
        break;
    default:
        stageFlags = VK_SHADER_STAGE_ALL;
        break;
    }
    return stageFlags;
}

VkFormat Slim::impl::rhi_vulkan(RHI::RHI_FORMAT fmt) noexcept
{        
    switch (fmt)
    {
    case RHI::RHI_FORMAT_RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case RHI::RHI_FORMAT_BGRA8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case RHI::RHI_FORMAT_RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case RHI::RHI_FORMAT_BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;

    // 16-bit formats
    case RHI::RHI_FORMAT_R16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case RHI::RHI_FORMAT_R16_SNORM:
        return VK_FORMAT_R16_SNORM;
    case RHI::RHI_FORMAT_R16_UINT:
        return VK_FORMAT_R16_UINT;
    case RHI::RHI_FORMAT_R16_SINT:
        return VK_FORMAT_R16_SINT;
    case RHI::RHI_FORMAT_R16_FLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case RHI::RHI_FORMAT_RG16_UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case RHI::RHI_FORMAT_RG16_SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case RHI::RHI_FORMAT_RG16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case RHI::RHI_FORMAT_RG16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case RHI::RHI_FORMAT_RG16_FLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case RHI::RHI_FORMAT_RGBA16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case RHI::RHI_FORMAT_RGBA16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case RHI::RHI_FORMAT_RGBA16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case RHI::RHI_FORMAT_RGBA16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case RHI::RHI_FORMAT_RGBA16_FLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;

    // 32-bit formats
    case RHI::RHI_FORMAT_R32_UINT:
        return VK_FORMAT_R32_UINT;
    case RHI::RHI_FORMAT_R32_SINT:
        return VK_FORMAT_R32_SINT;
    case RHI::RHI_FORMAT_R32_FLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case RHI::RHI_FORMAT_RG32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case RHI::RHI_FORMAT_RG32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case RHI::RHI_FORMAT_RG32_FLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case RHI::RHI_FORMAT_RGB32_UINT:
        return VK_FORMAT_R32G32B32_UINT;
    case RHI::RHI_FORMAT_RGB32_SINT:
        return VK_FORMAT_R32G32B32_SINT;
    case RHI::RHI_FORMAT_RGB32_FLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case RHI::RHI_FORMAT_RGBA32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case RHI::RHI_FORMAT_RGBA32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case RHI::RHI_FORMAT_RGBA32_FLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    // Depth Stencil
    case RHI::RHI_FORMAT_D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case RHI::RHI_FORMAT_D32_FLOAT:
        return VK_FORMAT_D32_SFLOAT;

    }
    return VK_FORMAT_UNDEFINED;
}

VkImageAspectFlags Slim::impl::rhi_vulkan(RHI::ATTACHMENT_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::ATTACHMENT_TYPE_COLOR:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case RHI::ATTACHMENT_TYPE_DEPTHSTENCIL:
        // TODO: STENCIL
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkImageAspectFlags Slim::impl::rhi_vulkan_aspect_mask(RHI::RHI_FORMAT format) noexcept
{
    switch (format)
    {
    case RHI::RHI_FORMAT_D24_UNORM_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case RHI::RHI_FORMAT_D32_FLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkAttachmentLoadOp Slim::impl::rhi_vulkan(RHI::PASS_LOAD_OP op) noexcept
{
    switch (op)
    {
    case RHI::PASS_LOAD_OP_LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case RHI::PASS_LOAD_OP_CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case RHI::PASS_LOAD_OP_DONT_CARE:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case RHI::PASS_LOAD_OP_NONE:
        return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
    case RHI::PASS_LOAD_OP_PRESERVE_LOCAL:
        return VK_ATTACHMENT_LOAD_OP_NONE_EXT; // Tile-Based, fallback to NONE
    default:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp Slim::impl::rhi_vulkan(RHI::PASS_STORE_OP op) noexcept
{
    switch (op)
    {
    case RHI::PASS_STORE_OP_STORE:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case RHI::PASS_STORE_OP_DONT_CARE:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case RHI::PASS_STORE_OP_NONE:
        return VK_ATTACHMENT_STORE_OP_NONE;
    case RHI::PASS_STORE_OP_PRESERVE_LOCAL:
        return VK_ATTACHMENT_STORE_OP_NONE; // Tile-Based, fallback to NONE
    case RHI::PASS_STORE_OP_RESOLVE:
        return VK_ATTACHMENT_STORE_OP_STORE; // D3D12 MSAA resolve, fallback to STORE
    default:
        return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

VkVertexInputRate Slim::impl::rhi_vulkan(RHI::VERTEX_INPUT_RATE rate) noexcept
{
    static_assert(RHI::VERTEX_INPUT_RATE_VERTEX == VK_VERTEX_INPUT_RATE_VERTEX, "VERTEX_INPUT_RATE_VERTEX must be VK_VERTEX_INPUT_RATE_VERTEX");
    static_assert(RHI::VERTEX_INPUT_RATE_INSTANCE == VK_VERTEX_INPUT_RATE_INSTANCE, "VERTEX_INPUT_RATE_INSTANCE must be VK_VERTEX_INPUT_RATE_INSTANCE");
    return static_cast<VkVertexInputRate>(rate);
}

VkPrimitiveTopology Slim::impl::rhi_vulkan(RHI::PRIMITIVE_TOPOLOGY topology) noexcept
{
    static_assert(RHI::PRIMITIVE_TOPOLOGY_POINT_LIST == VK_PRIMITIVE_TOPOLOGY_POINT_LIST, "PRIMITIVE_TOPOLOGY_POINT_LIST mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_LINE_LIST == VK_PRIMITIVE_TOPOLOGY_LINE_LIST, "PRIMITIVE_TOPOLOGY_LINE_LIST mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_LINE_STRIP == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, "PRIMITIVE_TOPOLOGY_LINE_STRIP mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "PRIMITIVE_TOPOLOGY_TRIANGLE_LIST mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_FAN == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, "PRIMITIVE_TOPOLOGY_TRIANGLE_FAN mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, "PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, "PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, "PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY mismatch");
    static_assert(RHI::PRIMITIVE_TOPOLOGY_PATCH_LIST == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, "PRIMITIVE_TOPOLOGY_PATCH_LIST mismatch");
    return static_cast<VkPrimitiveTopology>(topology);
}

VkSampleCountFlagBits Slim::impl::rhi_vulkan(RHI::SampleCount sampleCount) noexcept
{
    return static_cast<VkSampleCountFlagBits>(sampleCount.count);
}

VkPolygonMode Slim::impl::rhi_vulkan(RHI::POLYGON_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::POLYGON_MODE_FILL:
        return VK_POLYGON_MODE_FILL;
    case RHI::POLYGON_MODE_LINE:
        return VK_POLYGON_MODE_LINE;
    case RHI::POLYGON_MODE_POINT:
        return VK_POLYGON_MODE_POINT;
    default:
        return VK_POLYGON_MODE_FILL;
    }
}

VkCullModeFlags Slim::impl::rhi_vulkan(RHI::CULL_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::CULL_MODE_NONE:
        return VK_CULL_MODE_NONE;
    case RHI::CULL_MODE_FRONT:
        return VK_CULL_MODE_FRONT_BIT;
    case RHI::CULL_MODE_BACK:
        return VK_CULL_MODE_BACK_BIT;
    default:
        return VK_CULL_MODE_NONE;
    }
}

VkFrontFace Slim::impl::rhi_vulkan(RHI::FRONT_FACE face) noexcept
{
    switch (face)
    {
    case RHI::FRONT_FACE_CLOCKWISE:
        return VK_FRONT_FACE_CLOCKWISE;
    case RHI::FRONT_FACE_COUNTER_CLOCKWISE:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkCompareOp Slim::impl::rhi_vulkan(RHI::COMPARE_OP op) noexcept
{
    switch (op)
    {
    case RHI::COMPARE_OP_NEVER:
        return VK_COMPARE_OP_NEVER;
    case RHI::COMPARE_OP_LESS:
        return VK_COMPARE_OP_LESS;
    case RHI::COMPARE_OP_EQUAL:
        return VK_COMPARE_OP_EQUAL;
    case RHI::COMPARE_OP_LESS_OR_EQUAL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case RHI::COMPARE_OP_GREATER:
        return VK_COMPARE_OP_GREATER;
    case RHI::COMPARE_OP_NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;
    case RHI::COMPARE_OP_GREATER_OR_EQUAL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case RHI::COMPARE_OP_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
    default:
        return VK_COMPARE_OP_ALWAYS;
    }
}

VkStencilOp Slim::impl::rhi_vulkan(RHI::STENCIL_OP op) noexcept
{
    switch (op)
    {
    case RHI::STENCIL_OP_KEEP:
        return VK_STENCIL_OP_KEEP;
    case RHI::STENCIL_OP_ZERO:
        return VK_STENCIL_OP_ZERO;
    case RHI::STENCIL_OP_REPLACE:
        return VK_STENCIL_OP_REPLACE;
    case RHI::STENCIL_OP_INCREMENT_AND_CLAMP:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case RHI::STENCIL_OP_DECREMENT_AND_CLAMP:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case RHI::STENCIL_OP_INVERT:
        return VK_STENCIL_OP_INVERT;
    case RHI::STENCIL_OP_INCREMENT_AND_WRAP:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case RHI::STENCIL_OP_DECREMENT_AND_WRAP:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
        return VK_STENCIL_OP_KEEP;
    }
}

VkBool32 Slim::impl::rhi_vulkan(RHI::DEPTH_WRITE_MASK mask) noexcept
{
    switch (mask)
    {
    case RHI::DEPTH_WRITE_MASK_ZERO:
        return VK_FALSE;
    case RHI::DEPTH_WRITE_MASK_ALL:
        return VK_TRUE;
    default:
        return VK_FALSE;
    }
}

VkLogicOp Slim::impl::rhi_vulkan(RHI::LOGIC_OP op) noexcept
{
    switch (op)
    {
    case RHI::LOGIC_OP_CLEAR:
        return VK_LOGIC_OP_CLEAR;
    case RHI::LOGIC_OP_AND:
        return VK_LOGIC_OP_AND;
    case RHI::LOGIC_OP_AND_REVERSE:
        return VK_LOGIC_OP_AND_REVERSE;
    case RHI::LOGIC_OP_COPY:
        return VK_LOGIC_OP_COPY;
    case RHI::LOGIC_OP_AND_INVERTED:
        return VK_LOGIC_OP_AND_INVERTED;
    case RHI::LOGIC_OP_NO_OP:
        return VK_LOGIC_OP_NO_OP;
    case RHI::LOGIC_OP_XOR:
        return VK_LOGIC_OP_XOR;
    case RHI::LOGIC_OP_OR:
        return VK_LOGIC_OP_OR;
    case RHI::LOGIC_OP_NOR:
        return VK_LOGIC_OP_NOR;
    case RHI::LOGIC_OP_EQUIVALENT:
        return VK_LOGIC_OP_EQUIVALENT;
    case RHI::LOGIC_OP_INVERT:
        return VK_LOGIC_OP_INVERT;
    case RHI::LOGIC_OP_OR_REVERSE:
        return VK_LOGIC_OP_OR_REVERSE;
    case RHI::LOGIC_OP_COPY_INVERTED:
        return VK_LOGIC_OP_COPY_INVERTED;
    case RHI::LOGIC_OP_OR_INVERTED:
        return VK_LOGIC_OP_OR_INVERTED;
    case RHI::LOGIC_OP_NAND:
        return VK_LOGIC_OP_NAND;
    case RHI::LOGIC_OP_SET:
        return VK_LOGIC_OP_SET;
    default:
        return VK_LOGIC_OP_COPY;
    }
}

VkBlendFactor Slim::impl::rhi_vulkan(RHI::BLEND_FACTOR factor) noexcept
{
    switch (factor)
    {
    case RHI::BLEND_FACTOR_ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case RHI::BLEND_FACTOR_ONE:
        return VK_BLEND_FACTOR_ONE;
    case RHI::BLEND_FACTOR_SRC_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case RHI::BLEND_FACTOR_DST_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_DST_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case RHI::BLEND_FACTOR_SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case RHI::BLEND_FACTOR_DST_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case RHI::BLEND_FACTOR_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case RHI::BLEND_FACTOR_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case RHI::BLEND_FACTOR_SRC_ALPHA_SATURATE:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case RHI::BLEND_FACTOR_SRC1_COLOR:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case RHI::BLEND_FACTOR_SRC1_ALPHA:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
        return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendOp Slim::impl::rhi_vulkan(RHI::BLEND_OP op) noexcept
{
    switch (op)
    {
    case RHI::BLEND_OP_ADD:
        return VK_BLEND_OP_ADD;
    case RHI::BLEND_OP_SUBTRACT:
        return VK_BLEND_OP_SUBTRACT;
    case RHI::BLEND_OP_REVERSE_SUBTRACT:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case RHI::BLEND_OP_MIN:
        return VK_BLEND_OP_MIN;
    case RHI::BLEND_OP_MAX:
        return VK_BLEND_OP_MAX;
    default:
        return VK_BLEND_OP_ADD;
    }
}

VkColorComponentFlags Slim::impl::rhi_vulkan(RHI::COLOR_COMPONENT_FLAGS flags) noexcept
{
    VkColorComponentFlags vkFlags = 0;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_R) vkFlags |= VK_COLOR_COMPONENT_R_BIT;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_G) vkFlags |= VK_COLOR_COMPONENT_G_BIT;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_B) vkFlags |= VK_COLOR_COMPONENT_B_BIT;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_A) vkFlags |= VK_COLOR_COMPONENT_A_BIT;
    return vkFlags;
}

VkImageLayout Slim::impl::rhi_vulkan(RHI::IMAGE_LAYOUT state) noexcept
{
    switch (state)
    {
    case RHI::IMAGE_LAYOUT_UNDEFINED:
    case RHI::IMAGE_LAYOUT_PRESENT_DST:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case RHI::IMAGE_LAYOUT_PRESENT_SRC:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case RHI::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RHI::IMAGE_LAYOUT_DEPTH_ATTACHMENT_READ:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case RHI::IMAGE_LAYOUT_DEPTH_ATTACHMENT_WRITE:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case RHI::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case RHI::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case RHI::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case RHI::IMAGE_LAYOUT_GENERAL:
        return VK_IMAGE_LAYOUT_GENERAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkDescriptorType Slim::impl::rhi_vulkan(RHI::DESCRIPTOR_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::DESCRIPTOR_TYPE_CONST_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case RHI::DESCRIPTOR_TYPE_SHADER_RESOURCE:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case RHI::DESCRIPTOR_TYPE_UNORDERED_ACCESS:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case RHI::DESCRIPTOR_TYPE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    default:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

VkBufferUsageFlags Slim::impl::rhi_vulkan(RHI::BUFFER_USAGES usages) noexcept
{
    const uint32_t usage = usages.flags;
    VkBufferUsageFlags flags = 0;
    if (usage & RHI::BUFFER_USAGE_VERTEX_BUFFER)
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_INDEX_BUFFER)
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_CONSTANT_BUFFER)
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_INDIRECT_ARGS)
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_STORAGE_BUFFER)
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_UNIFORM_TEXEL)
        flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_STORAGE_TEXEL)
        flags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    if (usage & RHI::BUFFER_USAGE_TRANSFER_SRC)
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & RHI::BUFFER_USAGE_TRANSFER_DST)
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkImageUsageFlags Slim::impl::rhi_vulkan(RHI::IMAGE_USAGES usages) noexcept
{
    const uint32_t usage = usages.flags;
    VkImageUsageFlags flags = 0;
    if (usage & RHI::IMAGE_USAGE_TRANSFER_SRC)
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage & RHI::IMAGE_USAGE_TRANSFER_DST)
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage & RHI::IMAGE_USAGE_COLOR_ATTACHMENT)
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage & RHI::IMAGE_USAGE_DEPTH_STENCIL)
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage & RHI::IMAGE_USAGE_STORAGE)
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;

    {
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    return flags;
}

VkImageTiling Slim::impl::rhi_vulkan_tiling(RHI::MEMORY_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::MEMORY_TYPE_DEFAULT:
        return VK_IMAGE_TILING_OPTIMAL;
    case RHI::MEMORY_TYPE_UPLOAD:
        return VK_IMAGE_TILING_LINEAR;
    case RHI::MEMORY_TYPE_READBACK:
        return VK_IMAGE_TILING_LINEAR;
    default:
        return VK_IMAGE_TILING_OPTIMAL;
    }
}

VkIndexType Slim::impl::rhi_vulkan(RHI::INDEX_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::INDEX_TYPE_UINT16:
        return VK_INDEX_TYPE_UINT16;
    case RHI::INDEX_TYPE_UINT32:
        return VK_INDEX_TYPE_UINT32;
    default:
        return VK_INDEX_TYPE_UINT16;
    }
}

//VkBufferUsageFlags Slim::impl::rhi_vulkan(RHI::BUFFER_TYPE type) noexcept
//{
//    switch (type)
//    {
//    case RHI::BUFFER_TYPE_VERTEX:
//        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//    case RHI::BUFFER_TYPE_INDEX:
//        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//    case RHI::BUFFER_TYPE_CONSTANT:
//        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//    case RHI::BUFFER_TYPE_UPLOAD:
//        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//    default:
//        return 0;
//    }
//}

void Slim::impl::vulkan_set_debug_name(RHI::CVulkanDevice* device, VkObjectType objectType, uint64_t objectHandle, const char* name) noexcept
{
    // TODO: check debug if on : device->RefAdapter().RefInstance().IsDebug();
    
    assert(name && device && objectHandle && "check params");
    const auto hDevice = device->GetHandle();

    // Set debug name using VK_EXT_debug_utils
    VkDebugUtilsObjectNameInfoEXT nameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;

    // vkSetDebugUtilsObjectNameEXT may not be available if extension is not loaded
    // This is safe to call - it will just be ignored if the function pointer is null
    auto pfnSetDebugUtilsObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        ::vkGetDeviceProcAddr(hDevice, "vkSetDebugUtilsObjectNameEXT"));
    
    if (pfnSetDebugUtilsObjectName) {
        pfnSetDebugUtilsObjectName(hDevice, &nameInfo);
    }
}

VkFilter Slim::impl::rhi_vulkan(RHI::FILTER_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::FILTER_MODE_NEAREST:
        return VK_FILTER_NEAREST;
    case RHI::FILTER_MODE_LINEAR:
        return VK_FILTER_LINEAR;
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode Slim::impl::rhi_vulkan(RHI::ADDRESS_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::ADDRESS_MODE_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case RHI::ADDRESS_MODE_MIRRORED_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case RHI::ADDRESS_MODE_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case RHI::ADDRESS_MODE_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case RHI::ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkSamplerMipmapMode Slim::impl::rhi_vulkan(RHI::MIPMAP_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::MIPMAP_MODE_NEAREST:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case RHI::MIPMAP_MODE_LINEAR:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

VkBorderColor Slim::impl::rhi_vulkan(RHI::BORDER_COLOR color) noexcept
{
    switch (color)
    {
    case RHI::BORDER_COLOR_TRANSPARENT_BLACK:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case RHI::BORDER_COLOR_OPAQUE_BLACK:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case RHI::BORDER_COLOR_OPAQUE_WHITE:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    default:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
}

void Slim::impl::rhi_vulkan_cvt(const RHI::SamplerDesc* pDesc, VkSamplerCreateInfo* pOut) noexcept
{
    pOut->sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    pOut->pNext = nullptr;
    pOut->flags = 0;
    pOut->magFilter = impl::rhi_vulkan(pDesc->magFilter);
    pOut->minFilter = impl::rhi_vulkan(pDesc->minFilter);
    pOut->mipmapMode = impl::rhi_vulkan(pDesc->mipmapMode);
    pOut->addressModeU = impl::rhi_vulkan(pDesc->addressModeU);
    pOut->addressModeV = impl::rhi_vulkan(pDesc->addressModeV);
    pOut->addressModeW = impl::rhi_vulkan(pDesc->addressModeW);
    pOut->mipLodBias = pDesc->mipLodBias;
    pOut->anisotropyEnable = pDesc->anisotropyEnable ? VK_TRUE : VK_FALSE;
    pOut->maxAnisotropy = pDesc->anisotropyEnable ? pDesc->maxAnisotropy : 1.0f;
    pOut->compareEnable = pDesc->compareEnable ? VK_TRUE : VK_FALSE;
    pOut->compareOp = pDesc->compareEnable ? impl::rhi_vulkan(pDesc->compareOp) : VK_COMPARE_OP_ALWAYS;
    pOut->minLod = pDesc->minLod;
    pOut->maxLod = pDesc->maxLod;
    pOut->borderColor = impl::rhi_vulkan(pDesc->borderColor);
    pOut->unnormalizedCoordinates = VK_FALSE;
}

#endif