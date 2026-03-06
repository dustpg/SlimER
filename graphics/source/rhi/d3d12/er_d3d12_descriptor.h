#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_descriptor.h>
#include <d3d12.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"

namespace Slim { namespace RHI {

    class CD3D12Device;

    struct alignas(uint32_t) SlotBitsD3D12 {
        uint16_t                    index;
        uint16_t                    binding;
        uint8_t                     type;       // DESCRIPTOR_TYPE
        uint8_t                     visibility; // D3D12_SHADER_VISIBILITY
        uint16_t                    flags;      // D3D12_DESCRIPTOR_RANGE_FLAGS
    };


    struct DescriptorBufferSlotD3D12 {
        IRHIBase*                   resource;
        SlotBitsD3D12               bits;
    };

    using CDescriptorBufferRanges = pod::vector_soo<D3D12_DESCRIPTOR_RANGE1, 16>;

    class CD3D12DescriptorBuffer final : public impl::ref_count_class<CD3D12DescriptorBuffer, IRHIDescriptorBuffer> {
    public:

        CD3D12DescriptorBuffer(CD3D12Device* pDevice) noexcept;

        ~CD3D12DescriptorBuffer() noexcept;

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        CODE Init(const DescriptorBufferDesc&) noexcept;

    public:

        auto NormalHeap() const noexcept { return m_pNormalHeap; }

        auto SamplerHeap() const noexcept { return m_pSamplerHeap; }

        auto GetNormalVisibility() const noexcept { return m_eNormalVisibility; }

        auto GetSamplerVisibility() const noexcept { return m_eSamplerVisibility; }

        long SetupRanges(uint32_t space, CDescriptorBufferRanges& ranges, bool sampler) noexcept;

    public:

        void Unbind(uint32_t slot) noexcept override;

        CODE BindImage(const BindImageDesc* pDesc) noexcept override;

        CODE BindSampler(uint32_t slot, const SamplerDesc* pDesc) noexcept override;

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        ID3D12Device4*              m_pDevice = nullptr;

        ID3D12DescriptorHeap*       m_pNormalHeap = nullptr;

        ID3D12DescriptorHeap*       m_pSamplerHeap = nullptr;

        uint32_t                    m_uDescriptorSizeNormal = 0;

        uint32_t                    m_uDescriptorSizeSampler = 0;

        pod::vector<DescriptorBufferSlotD3D12>    
                                    m_data;

        uint64_t                    m_u64NormalGpuAddress = 0;

        uint64_t                    m_u64SamplerGpuAddress = 0;

        D3D12_SHADER_VISIBILITY     m_eNormalVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_SHADER_VISIBILITY     m_eSamplerVisibility = D3D12_SHADER_VISIBILITY_ALL;


    };

}}
#endif

