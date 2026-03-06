#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_resource.h>
#include <rhi/er_pipeline.h>
#include <rhi/er_command.h>
#include <d3d12.h>
#include "../common/er_rhi_common.h"

namespace Slim { namespace impl {

    inline RHI::CODE d3d12_rhi(HRESULT hr) noexcept {
        return RHI::CODE(hr);
    }

    void fuckms(ID3D12Resource*, D3D12_RESOURCE_DESC*) noexcept;

    DXGI_FORMAT rhi_d3d12(RHI::RHI_FORMAT) noexcept;

    D3D12_SHADER_VISIBILITY rhi_d3d12(RHI::SHADER_VISIBILITY) noexcept;

    D3D_PRIMITIVE_TOPOLOGY rhi_d3d12(RHI::PRIMITIVE_TOPOLOGY) noexcept;

    D3D12_PRIMITIVE_TOPOLOGY_TYPE rhi_d3d12_type(RHI::PRIMITIVE_TOPOLOGY) noexcept;

    DXGI_SAMPLE_DESC rhi_d3d12(RHI::SampleCount) noexcept;

    D3D12_FILL_MODE rhi_d3d12(RHI::POLYGON_MODE) noexcept;

    D3D12_CULL_MODE rhi_d3d12(RHI::CULL_MODE) noexcept;

    BOOL rhi_d3d12(RHI::FRONT_FACE) noexcept;

    D3D12_COMPARISON_FUNC rhi_d3d12(RHI::COMPARE_OP) noexcept;

    D3D12_STENCIL_OP rhi_d3d12(RHI::STENCIL_OP) noexcept;

    D3D12_DEPTH_WRITE_MASK rhi_d3d12(RHI::DEPTH_WRITE_MASK) noexcept;

    D3D12_LOGIC_OP rhi_d3d12(RHI::LOGIC_OP) noexcept;

    UINT8 rhi_d3d12(RHI::COLOR_COMPONENT_FLAGS) noexcept;

    D3D12_BLEND rhi_d3d12(RHI::BLEND_FACTOR) noexcept;

    D3D12_BLEND_OP rhi_d3d12(RHI::BLEND_OP) noexcept;

    D3D12_COMMAND_LIST_TYPE rhi_d3d12(RHI::QUEUE_TYPE) noexcept;

    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE rhi_d3d12(RHI::PASS_LOAD_OP) noexcept;

    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE rhi_d3d12(RHI::PASS_STORE_OP) noexcept;

    D3D12_RESOURCE_STATES rhi_d3d12(RHI::IMAGE_LAYOUT) noexcept;

    D3D12_TEXTURE_ADDRESS_MODE rhi_d3d12(RHI::ADDRESS_MODE) noexcept;

    D3D12_STATIC_BORDER_COLOR rhi_d3d12(RHI::BORDER_COLOR) noexcept;

    // Memory type conversion result
    struct MemoryTypeConversionResult {
        D3D12_HEAP_PROPERTIES heapProperties;
        D3D12_RESOURCE_STATES initialState;
        D3D12_RESOURCE_STATES resState;
    };

    // Convert MEMORY_TYPE to D3D12_HEAP_PROPERTIES and initial resource state
    MemoryTypeConversionResult rhi_d3d12_cvt(RHI::MEMORY_TYPE, RHI::BUFFER_USAGES) noexcept;

    // Convert MEMORY_TYPE and IMAGE_USAGES to D3D12_HEAP_PROPERTIES and initial resource state
    MemoryTypeConversionResult rhi_d3d12_cvt(RHI::MEMORY_TYPE, RHI::IMAGE_USAGES) noexcept;

    // Set debug name for D3D12 objects
    void d3d12_set_debug_name(ID3D12Object* pObject, const char* name) noexcept;

    // Get semantic name string for vertex attribute semantic
    const char* rhi_d3d12_semantic_name(RHI::VERTEX_ATTRIBUTE_SEMANTIC semantic) noexcept;

    DXGI_FORMAT rhi_d3d12(RHI::INDEX_TYPE) noexcept;

    // Convert DESCRIPTOR_TYPE to D3D12_ROOT_PARAMETER_TYPE for root descriptors
    D3D12_ROOT_PARAMETER_TYPE rhi_d3d12(RHI::DESCRIPTOR_TYPE) noexcept;

    // Convert VOLATILITY to D3D12_ROOT_DESCRIPTOR_FLAGS for root descriptors
    D3D12_ROOT_DESCRIPTOR_FLAGS rhi_d3d12_rd_flags(RHI::VOLATILITY) noexcept;

    D3D12_DESCRIPTOR_RANGE_FLAGS rhi_d3d12_rr_flags(RHI::VOLATILITY) noexcept;

    D3D12_TEXTURE_LAYOUT rhi_d3d12_tiling(RHI::MEMORY_TYPE) noexcept;

    // Convert SamplerDesc to D3D12_SAMPLER_DESC
    void rhi_d3d12_cvt(const RHI::SamplerDesc* pDesc, D3D12_SAMPLER_DESC* pOut) noexcept;

}}

namespace Slim { namespace RHI {

    template<typename T>
    void SafeRelease(T*& p) noexcept {
        if (p) { p->Release(); p = nullptr; }
    }

}}

#endif