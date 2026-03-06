#include "er_rhi_d3d12.h"
#ifndef SLIM_RHI_NO_D3D12
#include "../common/er_helper_p.h"
#include <dxgi1_4.h>
#include <d3d12.h>
#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using namespace Slim;

void impl::fuckms(ID3D12Resource* res, D3D12_RESOURCE_DESC* desc) noexcept
{
    // Microsoft's ABI design has issues with structure return values across different compilers.
    // To ensure ABI compatibility, we access the vtable directly and call GetDesc() through
    // a function pointer, treating it as a function that takes a pointer parameter instead of
    // returning a structure by value.
    
#ifdef _WIN64
    typedef void(* GetDescFunc)(ID3D12Resource*, D3D12_RESOURCE_DESC*);
#else
    typedef void(__stdcall* GetDescFunc)(ID3D12Resource*, D3D12_RESOURCE_DESC*);
#endif
    
    // Get the vtable pointer (first member of COM interface is the vtable pointer)
    void** vtable = *reinterpret_cast<void***>(res);
    
    // 0: QueryInterface (IUnknown)
    // 1: AddRef (IUnknown)
    // 2: Release (IUnknown)
    // 3: GetPrivateData (ID3D12Object)
    // 4: SetPrivateData (ID3D12Object)
    // 5: SetPrivateDataInterface (ID3D12Object)
    // 6: SetName (ID3D12Object)
    // 7: GetDevice (ID3D12DeviceChild)
    // 8: Map (ID3D12Resource)
    // 9: Unmap (ID3D12Resource)
    // 10: GetDesc (ID3D12Resource)
    constexpr int GetDescIndex = 10;
    GetDescFunc getDesc = reinterpret_cast<GetDescFunc>(vtable[GetDescIndex]);
    
    // Call through function pointer, treating it as a function that writes to the pointer parameter
    // This avoids ABI issues with structure return values across different compilers
    getDesc(res, desc);
}

DXGI_FORMAT impl::rhi_d3d12(RHI::RHI_FORMAT fmt) noexcept
{
    switch (fmt)
    {
    case RHI::RHI_FORMAT_RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case RHI::RHI_FORMAT_BGRA8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case RHI::RHI_FORMAT_RGBA8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case RHI::RHI_FORMAT_BGRA8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    // 16-bit formats
    case RHI::RHI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;
    case RHI::RHI_FORMAT_R16_SNORM:
        return DXGI_FORMAT_R16_SNORM;
    case RHI::RHI_FORMAT_R16_UINT:
        return DXGI_FORMAT_R16_UINT;
    case RHI::RHI_FORMAT_R16_SINT:
        return DXGI_FORMAT_R16_SINT;
    case RHI::RHI_FORMAT_R16_FLOAT:
        return DXGI_FORMAT_R16_FLOAT;
    case RHI::RHI_FORMAT_RG16_UNORM:
        return DXGI_FORMAT_R16G16_UNORM;
    case RHI::RHI_FORMAT_RG16_SNORM:
        return DXGI_FORMAT_R16G16_SNORM;
    case RHI::RHI_FORMAT_RG16_UINT:
        return DXGI_FORMAT_R16G16_UINT;
    case RHI::RHI_FORMAT_RG16_SINT:
        return DXGI_FORMAT_R16G16_SINT;
    case RHI::RHI_FORMAT_RG16_FLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case RHI::RHI_FORMAT_RGBA16_UNORM:
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case RHI::RHI_FORMAT_RGBA16_SNORM:
        return DXGI_FORMAT_R16G16B16A16_SNORM;
    case RHI::RHI_FORMAT_RGBA16_UINT:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    case RHI::RHI_FORMAT_RGBA16_SINT:
        return DXGI_FORMAT_R16G16B16A16_SINT;
    case RHI::RHI_FORMAT_RGBA16_FLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;

    // 32-bit formats
    case RHI::RHI_FORMAT_R32_UINT:
        return DXGI_FORMAT_R32_UINT;
    case RHI::RHI_FORMAT_R32_SINT:
        return DXGI_FORMAT_R32_SINT;
    case RHI::RHI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case RHI::RHI_FORMAT_RG32_UINT:
        return DXGI_FORMAT_R32G32_UINT;
    case RHI::RHI_FORMAT_RG32_SINT:
        return DXGI_FORMAT_R32G32_SINT;
    case RHI::RHI_FORMAT_RG32_FLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case RHI::RHI_FORMAT_RGB32_UINT:
        return DXGI_FORMAT_R32G32B32_UINT;
    case RHI::RHI_FORMAT_RGB32_SINT:
        return DXGI_FORMAT_R32G32B32_SINT;
    case RHI::RHI_FORMAT_RGB32_FLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case RHI::RHI_FORMAT_RGBA32_UINT:
        return DXGI_FORMAT_R32G32B32A32_UINT;
    case RHI::RHI_FORMAT_RGBA32_SINT:
        return DXGI_FORMAT_R32G32B32A32_SINT;
    case RHI::RHI_FORMAT_RGBA32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;

    // Depth Stencil
    case RHI::RHI_FORMAT_D24_UNORM_S8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case RHI::RHI_FORMAT_D32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    }
    return DXGI_FORMAT_UNKNOWN;
}

D3D12_SHADER_VISIBILITY impl::rhi_d3d12(RHI::SHADER_VISIBILITY visibility) noexcept
{
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_ALL) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_ALL), "SHADER_VISIBILITY_ALL mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_VERTEX) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_VERTEX), "SHADER_VISIBILITY_VERTEX mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_HULL) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_HULL), "SHADER_VISIBILITY_HULL mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_DOMAIN) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_DOMAIN), "SHADER_VISIBILITY_DOMAIN mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_GEOMETRY) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_GEOMETRY), "SHADER_VISIBILITY_GEOMETRY mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_PIXEL) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_PIXEL), "SHADER_VISIBILITY_PIXEL mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_AMPLIFICATION) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_AMPLIFICATION), "SHADER_VISIBILITY_AMPLIFICATION mismatch");
    static_assert(static_cast<uint32_t>(RHI::SHADER_VISIBILITY_MESH) == static_cast<uint32_t>(D3D12_SHADER_VISIBILITY_MESH), "SHADER_VISIBILITY_MESH mismatch");
    return static_cast<D3D12_SHADER_VISIBILITY>(visibility);
#if 0
    switch (visibility)
    {
    case RHI::SHADER_VISIBILITY_ALL:
        return D3D12_SHADER_VISIBILITY_ALL;
    case RHI::SHADER_VISIBILITY_VERTEX:
        return D3D12_SHADER_VISIBILITY_VERTEX;
    case RHI::SHADER_VISIBILITY_HULL:
        return D3D12_SHADER_VISIBILITY_HULL;
    case RHI::SHADER_VISIBILITY_DOMAIN:
        return D3D12_SHADER_VISIBILITY_DOMAIN;
    case RHI::SHADER_VISIBILITY_GEOMETRY:
        return D3D12_SHADER_VISIBILITY_GEOMETRY;
    case RHI::SHADER_VISIBILITY_PIXEL:
        return D3D12_SHADER_VISIBILITY_PIXEL;
    case RHI::SHADER_VISIBILITY_AMPLIFICATION:
        return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
    case RHI::SHADER_VISIBILITY_MESH:
        return D3D12_SHADER_VISIBILITY_MESH;
    default:
        return D3D12_SHADER_VISIBILITY_ALL;
    }
    #endif
}

D3D_PRIMITIVE_TOPOLOGY impl::rhi_d3d12(RHI::PRIMITIVE_TOPOLOGY topology) noexcept
{
    switch (topology)
    {
    case RHI::PRIMITIVE_TOPOLOGY_POINT_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case RHI::PRIMITIVE_TOPOLOGY_LINE_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case RHI::PRIMITIVE_TOPOLOGY_LINE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        return D3D_PRIMITIVE_TOPOLOGY(6);
        //return D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN;
    case RHI::PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
    case RHI::PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
    case RHI::PRIMITIVE_TOPOLOGY_PATCH_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST; // Default to 1 control point, caller should adjust
    default:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE impl::rhi_d3d12_type(RHI::PRIMITIVE_TOPOLOGY topology) noexcept
{
    switch (topology)
    {
    case RHI::PRIMITIVE_TOPOLOGY_POINT_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case RHI::PRIMITIVE_TOPOLOGY_LINE_LIST:
    case RHI::PRIMITIVE_TOPOLOGY_LINE_STRIP:
    case RHI::PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    case RHI::PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    case RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case RHI::PRIMITIVE_TOPOLOGY_PATCH_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

DXGI_SAMPLE_DESC impl::rhi_d3d12(RHI::SampleCount sampleCount) noexcept
{
    return { sampleCount.count , 0};
}

D3D12_FILL_MODE impl::rhi_d3d12(RHI::POLYGON_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::POLYGON_MODE_FILL:
        return D3D12_FILL_MODE_SOLID;
    case RHI::POLYGON_MODE_LINE:
        return D3D12_FILL_MODE_WIREFRAME;
    case RHI::POLYGON_MODE_POINT:
        // D3D12 doesn't support point mode directly, fallback to wireframe
        return D3D12_FILL_MODE_WIREFRAME;
    default:
        return D3D12_FILL_MODE_SOLID;
    }
}

D3D12_CULL_MODE impl::rhi_d3d12(RHI::CULL_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::CULL_MODE_NONE:
        return D3D12_CULL_MODE_NONE;
    case RHI::CULL_MODE_FRONT:
        return D3D12_CULL_MODE_FRONT;
    case RHI::CULL_MODE_BACK:
        return D3D12_CULL_MODE_BACK;
    default:
        return D3D12_CULL_MODE_NONE;
    }
}

BOOL impl::rhi_d3d12(RHI::FRONT_FACE face) noexcept
{
    // D3D12: TRUE = FrontCounterClockwise, FALSE = FrontClockwise
    switch (face)
    {
    case RHI::FRONT_FACE_CLOCKWISE:
        return FALSE; // FrontClockwise
    case RHI::FRONT_FACE_COUNTER_CLOCKWISE:
        return TRUE; // FrontCounterClockwise
    default:
        return TRUE; // Default to FrontCounterClockwise
    }
}

D3D12_COMPARISON_FUNC impl::rhi_d3d12(RHI::COMPARE_OP op) noexcept
{
    switch (op)
    {
    case RHI::COMPARE_OP_NEVER:
        return D3D12_COMPARISON_FUNC_NEVER;
    case RHI::COMPARE_OP_LESS:
        return D3D12_COMPARISON_FUNC_LESS;
    case RHI::COMPARE_OP_EQUAL:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case RHI::COMPARE_OP_LESS_OR_EQUAL:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case RHI::COMPARE_OP_GREATER:
        return D3D12_COMPARISON_FUNC_GREATER;
    case RHI::COMPARE_OP_NOT_EQUAL:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case RHI::COMPARE_OP_GREATER_OR_EQUAL:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case RHI::COMPARE_OP_ALWAYS:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    default:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

D3D12_STENCIL_OP impl::rhi_d3d12(RHI::STENCIL_OP op) noexcept
{
    switch (op)
    {
    case RHI::STENCIL_OP_KEEP:
        return D3D12_STENCIL_OP_KEEP;
    case RHI::STENCIL_OP_ZERO:
        return D3D12_STENCIL_OP_ZERO;
    case RHI::STENCIL_OP_REPLACE:
        return D3D12_STENCIL_OP_REPLACE;
    case RHI::STENCIL_OP_INCREMENT_AND_CLAMP:
        return D3D12_STENCIL_OP_INCR_SAT;
    case RHI::STENCIL_OP_DECREMENT_AND_CLAMP:
        return D3D12_STENCIL_OP_DECR_SAT;
    case RHI::STENCIL_OP_INVERT:
        return D3D12_STENCIL_OP_INVERT;
    case RHI::STENCIL_OP_INCREMENT_AND_WRAP:
        return D3D12_STENCIL_OP_INCR;
    case RHI::STENCIL_OP_DECREMENT_AND_WRAP:
        return D3D12_STENCIL_OP_DECR;
    default:
        return D3D12_STENCIL_OP_KEEP;
    }
}

D3D12_DEPTH_WRITE_MASK impl::rhi_d3d12(RHI::DEPTH_WRITE_MASK mask) noexcept
{
    switch (mask)
    {
    case RHI::DEPTH_WRITE_MASK_ZERO:
        return D3D12_DEPTH_WRITE_MASK_ZERO;
    case RHI::DEPTH_WRITE_MASK_ALL:
        return D3D12_DEPTH_WRITE_MASK_ALL;
    default:
        return D3D12_DEPTH_WRITE_MASK_ZERO;
    }
}

D3D12_LOGIC_OP impl::rhi_d3d12(RHI::LOGIC_OP op) noexcept
{
    switch (op)
    {
    case RHI::LOGIC_OP_CLEAR:
        return D3D12_LOGIC_OP_CLEAR;
    case RHI::LOGIC_OP_AND:
        return D3D12_LOGIC_OP_AND;
    case RHI::LOGIC_OP_AND_REVERSE:
        return D3D12_LOGIC_OP_AND_REVERSE;
    case RHI::LOGIC_OP_COPY:
        return D3D12_LOGIC_OP_COPY;
    case RHI::LOGIC_OP_AND_INVERTED:
        return D3D12_LOGIC_OP_AND_INVERTED;
    case RHI::LOGIC_OP_NO_OP:
        return D3D12_LOGIC_OP_NOOP;
    case RHI::LOGIC_OP_XOR:
        return D3D12_LOGIC_OP_XOR;
    case RHI::LOGIC_OP_OR:
        return D3D12_LOGIC_OP_OR;
    case RHI::LOGIC_OP_NOR:
        return D3D12_LOGIC_OP_NOR;
    case RHI::LOGIC_OP_EQUIVALENT:
        return D3D12_LOGIC_OP_EQUIV;
    case RHI::LOGIC_OP_INVERT:
        return D3D12_LOGIC_OP_INVERT;
    case RHI::LOGIC_OP_OR_REVERSE:
        return D3D12_LOGIC_OP_OR_REVERSE;
    case RHI::LOGIC_OP_COPY_INVERTED:
        return D3D12_LOGIC_OP_COPY_INVERTED;
    case RHI::LOGIC_OP_OR_INVERTED:
        return D3D12_LOGIC_OP_OR_INVERTED;
    case RHI::LOGIC_OP_NAND:
        return D3D12_LOGIC_OP_NAND;
    case RHI::LOGIC_OP_SET:
        return D3D12_LOGIC_OP_SET;
    default:
        return D3D12_LOGIC_OP_COPY;
    }
}

UINT8 impl::rhi_d3d12(RHI::COLOR_COMPONENT_FLAGS flags) noexcept
{
    UINT8 d3d12Mask = 0;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_R) d3d12Mask |= D3D12_COLOR_WRITE_ENABLE_RED;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_G) d3d12Mask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_B) d3d12Mask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (flags.flag & RHI::COLOR_COMPONENT_FLAG_A) d3d12Mask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    return d3d12Mask;
}

D3D12_BLEND impl::rhi_d3d12(RHI::BLEND_FACTOR factor) noexcept
{
    switch (factor)
    {
    case RHI::BLEND_FACTOR_ZERO:
        return D3D12_BLEND_ZERO;
    case RHI::BLEND_FACTOR_ONE:
        return D3D12_BLEND_ONE;
    case RHI::BLEND_FACTOR_SRC_COLOR:
        return D3D12_BLEND_SRC_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
        return D3D12_BLEND_INV_SRC_COLOR;
    case RHI::BLEND_FACTOR_DST_COLOR:
        return D3D12_BLEND_DEST_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_DST_COLOR:
        return D3D12_BLEND_INV_DEST_COLOR;
    case RHI::BLEND_FACTOR_SRC_ALPHA:
        return D3D12_BLEND_SRC_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case RHI::BLEND_FACTOR_DST_ALPHA:
        return D3D12_BLEND_DEST_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        return D3D12_BLEND_INV_DEST_ALPHA;
    case RHI::BLEND_FACTOR_CONSTANT_COLOR:
        return D3D12_BLEND_BLEND_FACTOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
        return D3D12_BLEND_INV_BLEND_FACTOR;
    case RHI::BLEND_FACTOR_CONSTANT_ALPHA:
        return D3D12_BLEND_BLEND_FACTOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
        return D3D12_BLEND_INV_BLEND_FACTOR;
    case RHI::BLEND_FACTOR_SRC_ALPHA_SATURATE:
        return D3D12_BLEND_SRC_ALPHA_SAT;
    case RHI::BLEND_FACTOR_SRC1_COLOR:
        return D3D12_BLEND_SRC1_COLOR;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
        return D3D12_BLEND_INV_SRC1_COLOR;
    case RHI::BLEND_FACTOR_SRC1_ALPHA:
        return D3D12_BLEND_SRC1_ALPHA;
    case RHI::BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
        return D3D12_BLEND_INV_SRC1_ALPHA;
    default:
        return D3D12_BLEND_ONE;
    }
}

D3D12_BLEND_OP impl::rhi_d3d12(RHI::BLEND_OP op) noexcept
{
    switch (op)
    {
    case RHI::BLEND_OP_ADD:
        return D3D12_BLEND_OP_ADD;
    case RHI::BLEND_OP_SUBTRACT:
        return D3D12_BLEND_OP_SUBTRACT;
    case RHI::BLEND_OP_REVERSE_SUBTRACT:
        return D3D12_BLEND_OP_REV_SUBTRACT;
    case RHI::BLEND_OP_MIN:
        return D3D12_BLEND_OP_MIN;
    case RHI::BLEND_OP_MAX:
        return D3D12_BLEND_OP_MAX;
    default:
        return D3D12_BLEND_OP_ADD;
    }
}

D3D12_COMMAND_LIST_TYPE impl::rhi_d3d12(RHI::QUEUE_TYPE queueType) noexcept
{
    switch (queueType)
    {
    case RHI::QUEUE_TYPE_GRAPHICS:
        return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case RHI::QUEUE_TYPE_COMPUTE:
        return D3D12_COMMAND_LIST_TYPE_COMPUTE;
    case RHI::QUEUE_TYPE_COPY:
        return D3D12_COMMAND_LIST_TYPE_COPY;
    case RHI::QUEUE_TYPE_VIDEO_DECODE:
        return D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE;
    case RHI::QUEUE_TYPE_VIDEO_ENCODE:
        return D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE;
    default:
        return D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
}

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE impl::rhi_d3d12(RHI::PASS_LOAD_OP loadOp) noexcept
{
    switch (loadOp)
    {
    case RHI::PASS_LOAD_OP_LOAD:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    case RHI::PASS_LOAD_OP_CLEAR:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    case RHI::PASS_LOAD_OP_DONT_CARE:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
    case RHI::PASS_LOAD_OP_NONE:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
    case RHI::PASS_LOAD_OP_PRESERVE_LOCAL:
        // TODO: _RENDER _SRV _UAV
        //return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE_LOCAL_RENDER;
    default:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    }
}

D3D12_RENDER_PASS_ENDING_ACCESS_TYPE impl::rhi_d3d12(RHI::PASS_STORE_OP storeOp) noexcept
{
    switch (storeOp)
    {
    case RHI::PASS_STORE_OP_STORE:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    case RHI::PASS_STORE_OP_DONT_CARE:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
    case RHI::PASS_STORE_OP_NONE:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
    case RHI::PASS_STORE_OP_PRESERVE_LOCAL:
        // TODO: _RENDER _SRV _UAV
        //return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE_LOCAL_RENDER;
    case RHI::PASS_STORE_OP_RESOLVE:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
    default:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    }
}

D3D12_RESOURCE_STATES impl::rhi_d3d12(RHI::IMAGE_LAYOUT layout) noexcept
{
    switch (layout)
    {
    case RHI::IMAGE_LAYOUT_UNDEFINED:
        return D3D12_RESOURCE_STATE_COMMON;
    case RHI::IMAGE_LAYOUT_PRESENT_DST:
    case RHI::IMAGE_LAYOUT_PRESENT_SRC:
        return D3D12_RESOURCE_STATE_PRESENT;
    case RHI::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case RHI::IMAGE_LAYOUT_DEPTH_ATTACHMENT_READ:
        return D3D12_RESOURCE_STATE_DEPTH_READ;
    case RHI::IMAGE_LAYOUT_DEPTH_ATTACHMENT_WRITE:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case RHI::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case RHI::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case RHI::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case RHI::IMAGE_LAYOUT_GENERAL:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

impl::MemoryTypeConversionResult impl::rhi_d3d12_cvt(RHI::MEMORY_TYPE type, RHI::BUFFER_USAGES usage) noexcept
{
    MemoryTypeConversionResult result = {};
    const uint32_t usageFlags = usage.flags;

    uint32_t initialState = 0;
    uint32_t resState = 0;
    
    switch (type) 
    {
    case RHI::MEMORY_TYPE_DEFAULT:
        result.heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        // Determine initial state based on buffer usage
        if (usageFlags & (RHI::BUFFER_USAGE_VERTEX_BUFFER | RHI::BUFFER_USAGE_CONSTANT_BUFFER)) {
            /*
            D3D12 WARNING : ID3D12Device::CreateCommittedResource :
                Ignoring InitialState D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER.
                Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON.
                [STATE_CREATION WARNING #1328: CREATERESOURCE_STATE_IGNORED]
            */
            resState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        } 
        if (usageFlags & RHI::BUFFER_USAGE_INDEX_BUFFER) {
            resState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        } 
        if (usageFlags & RHI::BUFFER_USAGE_INDIRECT_ARGS) {
            resState |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        } 
        if (usageFlags & RHI::BUFFER_USAGE_STORAGE_BUFFER) {
            resState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        if (usageFlags & RHI::BUFFER_USAGE_TRANSFER_SRC) {
            resState |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        } 
        // WRITE FLAGS CANNOT COMBINE WITH READ FLAGS
        if (resState == 0 && (usageFlags & RHI::BUFFER_USAGE_TRANSFER_DST)) {
            initialState = D3D12_RESOURCE_STATE_COPY_DEST;
            resState = D3D12_RESOURCE_STATE_COPY_DEST;
        } 
        result.initialState = D3D12_RESOURCE_STATES(initialState);
        result.resState = D3D12_RESOURCE_STATES(resState);
        break;
    case RHI::MEMORY_TYPE_UPLOAD:
        result.heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        result.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        // D3D12_RESOURCE_STATE_COPY_SOURCE < D3D12_RESOURCE_STATE_GENERIC_READ
        break;
    case RHI::MEMORY_TYPE_READBACK:
        result.heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        // READBACK heap resources are always in COPY_DEST state
        result.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
        break;
    default:
        // Default to DEFAULT heap type for unknown types
        result.heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        result.initialState = D3D12_RESOURCE_STATE_COMMON;
        break;
    }
    
    return result;
}

impl::MemoryTypeConversionResult impl::rhi_d3d12_cvt(RHI::MEMORY_TYPE type, RHI::IMAGE_USAGES usage) noexcept
{
    MemoryTypeConversionResult result = {};
    const uint32_t usageFlags = usage.flags;

    uint32_t initialState = 0;
    uint32_t resState = 0;
    
    switch (type) 
    {
    case RHI::MEMORY_TYPE_DEFAULT:
        result.heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
#if 0
        // Determine initial state based on image usage
        if (usageFlags & RHI::IMAGE_USAGE_TRANSFER_DST) {
            initialState = D3D12_RESOURCE_STATE_COPY_DEST;
            resState = D3D12_RESOURCE_STATE_COPY_DEST;
        }
        else if (usageFlags & RHI::IMAGE_USAGE_TRANSFER_SRC) {
            initialState = D3D12_RESOURCE_STATE_COPY_SOURCE;
            resState = D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
        else if (usageFlags & RHI::IMAGE_USAGE_COLOR_ATTACHMENT) {
            initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            resState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        else if (usageFlags & RHI::IMAGE_USAGE_DEPTH_STENCIL) {
            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            resState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
        else {
            initialState = D3D12_RESOURCE_STATE_COMMON;
            resState = D3D12_RESOURCE_STATE_COMMON;
        }
        result.initialState = D3D12_RESOURCE_STATES(initialState);
        result.resState = D3D12_RESOURCE_STATES(resState);
#else
        // always common(vulkan=undefined)
        result.initialState = D3D12_RESOURCE_STATE_COMMON;
        result.resState = D3D12_RESOURCE_STATE_COMMON;
        
        if (usageFlags & RHI::IMAGE_USAGE_DEPTH_STENCIL) {
            result.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            result.resState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
#endif
        break;
    case RHI::MEMORY_TYPE_UPLOAD:
        result.heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        result.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        result.resState = D3D12_RESOURCE_STATE_GENERIC_READ;
        break;
    case RHI::MEMORY_TYPE_READBACK:
        result.heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        // READBACK heap resources are always in COPY_DEST state
        result.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
        result.resState = D3D12_RESOURCE_STATE_COPY_DEST;
        break;
    default:
        // Default to DEFAULT heap type for unknown types
        result.heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        result.heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        result.heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        result.initialState = D3D12_RESOURCE_STATE_COMMON;
        result.resState = D3D12_RESOURCE_STATE_COMMON;
        break;
    }
    
    return result;
}

void impl::d3d12_set_debug_name(ID3D12Object* pObject, const char* name) noexcept
{
    if (!pObject || !name)
        return;

    // Convert UTF-8 to UTF-16 (wide string)
    int size_needed = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (size_needed > 0) {
        wchar_t* wide_name = static_cast<wchar_t*>(base_class::imalloc(size_needed * sizeof(wchar_t)));
        if (wide_name) {
            ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, size_needed);
            pObject->SetName(wide_name);
            base_class::ifree(wide_name);
        }
    }
}

const char* impl::rhi_d3d12_semantic_name(RHI::VERTEX_ATTRIBUTE_SEMANTIC semantic) noexcept
{
    switch (semantic)
    {
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_POSITION: 
        return "POSITION";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_NORMAL: 
        return "NORMAL";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_COLOR: 
        return "COLOR";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_TEXCOORD: 
        return "TEXCOORD";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_TANGENT: 
        return "TANGENT";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_BINORMAL:
        return "BINORMAL";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_BLENDWEIGHT: 
        return "BLENDWEIGHT";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_BLENDINDICES: 
        return "BLENDINDICES";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_PSIZE: 
        return "PSIZE";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_FOG: 
        return "FOG";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_DEPTH: 
        return "DEPTH";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_MATRIX: 
        return "MATRIX";
    case RHI::VERTEX_ATTRIBUTE_SEMANTIC_CUSTOM:
    default: 
        return "CUSTOM";
    }
}

DXGI_FORMAT impl::rhi_d3d12(RHI::INDEX_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::INDEX_TYPE_UINT16:
        return DXGI_FORMAT_R16_UINT;
    case RHI::INDEX_TYPE_UINT32:
        return DXGI_FORMAT_R32_UINT;
    default:
        return DXGI_FORMAT_R16_UINT;
    }
}

D3D12_ROOT_PARAMETER_TYPE impl::rhi_d3d12(RHI::DESCRIPTOR_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::DESCRIPTOR_TYPE_CONST_BUFFER:
        return D3D12_ROOT_PARAMETER_TYPE_CBV;
    case RHI::DESCRIPTOR_TYPE_SHADER_RESOURCE:
        return D3D12_ROOT_PARAMETER_TYPE_SRV;
    case RHI::DESCRIPTOR_TYPE_UNORDERED_ACCESS:
        return D3D12_ROOT_PARAMETER_TYPE_UAV;
    default:
        return D3D12_ROOT_PARAMETER_TYPE_CBV;
    }
}

D3D12_ROOT_DESCRIPTOR_FLAGS impl::rhi_d3d12_rd_flags(RHI::VOLATILITY volatility) noexcept
{
    switch (volatility)
    {
    case RHI::VOLATILITY_DEFAULT:
        return D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    case RHI::VOLATILITY_VOLATILE:
        return D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
    case RHI::VOLATILITY_STATIC:
        return D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
    case RHI::VOLATILITY_STATIC_AT_EXECUTE:
        return D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
    default:
        return D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    }
}

D3D12_DESCRIPTOR_RANGE_FLAGS impl::rhi_d3d12_rr_flags(RHI::VOLATILITY volatility) noexcept
{
    switch (volatility)
    {
    case RHI::VOLATILITY_DEFAULT:
        return D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    case RHI::VOLATILITY_VOLATILE:
        return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    case RHI::VOLATILITY_STATIC:
        return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    case RHI::VOLATILITY_STATIC_AT_EXECUTE:
        return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
    default:
        return D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    }
}

D3D12_TEXTURE_LAYOUT impl::rhi_d3d12_tiling(RHI::MEMORY_TYPE type) noexcept
{
    switch (type)
    {
    case RHI::MEMORY_TYPE_DEFAULT:
        return D3D12_TEXTURE_LAYOUT_UNKNOWN;
    case RHI::MEMORY_TYPE_UPLOAD:
        return D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    case RHI::MEMORY_TYPE_READBACK:
        return D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    default:
        return D3D12_TEXTURE_LAYOUT_UNKNOWN;
    }
}

D3D12_TEXTURE_ADDRESS_MODE impl::rhi_d3d12(RHI::ADDRESS_MODE mode) noexcept
{
    switch (mode)
    {
    case RHI::ADDRESS_MODE_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case RHI::ADDRESS_MODE_MIRRORED_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case RHI::ADDRESS_MODE_CLAMP_TO_EDGE:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case RHI::ADDRESS_MODE_CLAMP_TO_BORDER:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    case RHI::ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    default:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12_STATIC_BORDER_COLOR impl::rhi_d3d12(RHI::BORDER_COLOR color) noexcept
{
    switch (color)
    {
    case RHI::BORDER_COLOR_TRANSPARENT_BLACK:
        return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    case RHI::BORDER_COLOR_OPAQUE_BLACK:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    case RHI::BORDER_COLOR_OPAQUE_WHITE:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    default:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    }
}

namespace {
    // Convert filter modes to D3D12_FILTER
    // D3D12_FILTER combines min/mag/mip filter types
    // Note: Comparison mode is controlled by ComparisonFunc field, not Filter flags
    inline D3D12_FILTER rhi_d3d12_filter(RHI::FILTER_MODE minFilter, RHI::FILTER_MODE magFilter, RHI::MIPMAP_MODE mipmapMode, bool anisotropyEnable) noexcept
    {
        // If anisotropy is enabled, use anisotropic filter
        if (anisotropyEnable) {
            return D3D12_FILTER_ANISOTROPIC;
        }
        
        // Combine all 8 possible combinations (2x2x2)
        const bool minLinear = (minFilter == RHI::FILTER_MODE_LINEAR);
        const bool magLinear = (magFilter == RHI::FILTER_MODE_LINEAR);
        const bool mipLinear = (mipmapMode == RHI::MIPMAP_MODE_LINEAR);
        
        // D3D12_FILTER values:
        // MIN_MAG_MIP_POINT = 0x0
        // MIN_MAG_POINT_MIP_LINEAR = 0x1
        // MIN_POINT_MAG_LINEAR_MIP_POINT = 0x4
        // MIN_POINT_MAG_MIP_LINEAR = 0x5
        // MIN_LINEAR_MAG_MIP_POINT = 0x10
        // MIN_LINEAR_MAG_POINT_MIP_LINEAR = 0x11
        // MIN_MAG_LINEAR_MIP_POINT = 0x14
        // MIN_MAG_MIP_LINEAR = 0x15
        
        if (!minLinear && !magLinear && !mipLinear)
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
        if (!minLinear && !magLinear && mipLinear)
            return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        if (!minLinear && magLinear && !mipLinear)
            return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        if (!minLinear && magLinear && mipLinear)
            return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        if (minLinear && !magLinear && !mipLinear)
            return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        if (minLinear && !magLinear && mipLinear)
            return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        if (minLinear && magLinear && !mipLinear)
            return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        // minLinear && magLinear && mipLinear
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }

    // Convert BORDER_COLOR enum to float[4] for D3D12_SAMPLER_DESC
    inline void rhi_d3d12_border_color(RHI::BORDER_COLOR color, float out[4]) noexcept
    {
        switch (color)
        {
        case RHI::BORDER_COLOR_TRANSPARENT_BLACK:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 0.0f;
            break;
        //case RHI::BORDER_COLOR_TRANSPARENT_WHITE:
        //    out[0] = 1.0f; out[1] = 1.0f; out[2] = 1.0f; out[3] = 0.0f;
        //    break;
        case RHI::BORDER_COLOR_OPAQUE_BLACK:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
            break;
        case RHI::BORDER_COLOR_OPAQUE_WHITE:
            out[0] = 1.0f; out[1] = 1.0f; out[2] = 1.0f; out[3] = 1.0f;
            break;
        default:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
            break;
        }
    }

}

void impl::rhi_d3d12_cvt(const RHI::SamplerDesc* pDesc, D3D12_SAMPLER_DESC* pOut) noexcept
{
    pOut->Filter = rhi_d3d12_filter(pDesc->minFilter, pDesc->magFilter, pDesc->mipmapMode, pDesc->anisotropyEnable);
    pOut->AddressU = impl::rhi_d3d12(pDesc->addressModeU);
    pOut->AddressV = impl::rhi_d3d12(pDesc->addressModeV);
    pOut->AddressW = impl::rhi_d3d12(pDesc->addressModeW);
    pOut->MipLODBias = pDesc->mipLodBias;
    pOut->MaxAnisotropy = pDesc->anisotropyEnable ? static_cast<UINT>(pDesc->maxAnisotropy) : 1;
    // Comparison mode is controlled by ComparisonFunc field
    // If compareEnable is false, set to NEVER to disable comparison
    pOut->ComparisonFunc = pDesc->compareEnable ? impl::rhi_d3d12(pDesc->compareOp) : D3D12_COMPARISON_FUNC_NEVER;
    rhi_d3d12_border_color(pDesc->borderColor, pOut->BorderColor);
    pOut->MinLOD = pDesc->minLod;
    pOut->MaxLOD = pDesc->maxLod;
}

#endif