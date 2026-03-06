#include "er_backend_d3d12.h"
#ifndef SLIM_RHI_NO_D3D12
#include "er_d3d12_descriptor.h"
#include "er_d3d12_device.h"
#include "er_d3d12_resource.h"
#include "er_rhi_d3d12.h"
#include "../common/er_rhi_common.h"

namespace Slim { namespace impl {

    inline void clean(RHI::DescriptorBufferSlotD3D12& data, ID3D12Device4* d) noexcept
    {
        RHI::SafeDispose(data.resource);
    }
}}

using namespace Slim;

// ----------------------------------------------------------------------------
//                              CD3D12DescriptorBuffer
// ----------------------------------------------------------------------------

RHI::CD3D12DescriptorBuffer::CD3D12DescriptorBuffer(CD3D12Device* pDevice) noexcept
    : m_pThisDevice(pDevice)
{
    assert(pDevice);
    pDevice->AddRefCnt();
    m_pDevice = &pDevice->RefDevice();
}

RHI::CD3D12DescriptorBuffer::~CD3D12DescriptorBuffer() noexcept
{
    for (auto& item : m_data) {
        impl::clean(item, m_pDevice);
    }
    RHI::SafeRelease(m_pSamplerHeap);
    RHI::SafeRelease(m_pNormalHeap);
    RHI::SafeDispose(m_pThisDevice);
    m_pDevice = nullptr;
}


RHI::CODE RHI::CD3D12DescriptorBuffer::BindImage(const BindImageDesc* pDesc) noexcept
{
    if (!pDesc)
        return CODE_POINTER;
    const auto slot = pDesc->slot;
    if (slot >= m_data.size())
        return CODE_INVALIDARG;

    auto& slotData = m_data[slot];

    if (slotData.bits.type == RHI::DESCRIPTOR_TYPE_SAMPLER) {
        assert(!"CHECK TYPE");
        return CODE_INVALIDARG;
    }


    impl::clean(slotData, m_pDevice);

    HRESULT hr = S_OK;
    // CREATE DESCRIPTOR
    const auto image = static_cast<CD3D12Image*>(pDesc->image);
    // SIMPLE RTTI
    assert(m_pThisDevice == &image->RefDevice());

    // CREATE DESCRIPTOR
    if (SUCCEEDED(hr)) {
        const auto& imageDesc = image->RefDesc();
        // Use format from ImageViewDesc if specified, otherwise use format from Image
        const auto format = (pDesc->format != RHI::RHI_FORMAT_UNKNOWN)
            ? pDesc->format
            : imageDesc.format;

        // Calculate CPU descriptor handle for this slot
        auto cpuHandle = m_pNormalHeap->GetCPUDescriptorHandleForHeapStart();
        const auto index = size_t(slotData.bits.index);
        cpuHandle.ptr += index * size_t(m_uDescriptorSizeNormal);

        // Create SRV descriptor
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = impl::rhi_d3d12(format);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        m_pDevice->CreateShaderResourceView(&image->RefResource(), &srvDesc, cpuHandle);

        //slotData.type = SLOT_TYPE_D3D12_IMAGE;
        slotData.resource = image;
        image->AddRefCnt();
    }

    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12DescriptorBuffer::BindSampler(uint32_t slot, const SamplerDesc* pDesc) noexcept
{
    if (!pDesc)
        return CODE_POINTER;
    if (slot >= m_data.size())
        return CODE_INVALIDARG;
    
    auto& slotData = m_data[slot];
    
    // Verify this slot is for a sampler
    if (slotData.bits.type != RHI::DESCRIPTOR_TYPE_SAMPLER) {
        assert(!"CHECK TYPE");
        return CODE_INVALIDARG;
    }
    
    assert(m_pSamplerHeap);
    
    // Calculate CPU descriptor handle for this slot
    auto cpuHandle = m_pSamplerHeap->GetCPUDescriptorHandleForHeapStart();
    const auto index = size_t(slotData.bits.index);
    cpuHandle.ptr += index * size_t(m_uDescriptorSizeSampler);
    
    // Convert sampler descriptor using helper function
    D3D12_SAMPLER_DESC samplerDesc = {};
    impl::rhi_d3d12_cvt(pDesc, &samplerDesc);
    
    m_pDevice->CreateSampler(&samplerDesc, cpuHandle);
    
    return CODE_OK;
}

void Slim::RHI::CD3D12DescriptorBuffer::Unbind(uint32_t slot) noexcept
{
    // TODO: Implement Unbind
    if (slot < m_data.size()) {
        impl::clean(m_data[slot], m_pDevice);
        //m_data[slot].type = SLOT_TYPE_D3D12_NULL;
        //m_data[slot].resource = nullptr;
    }
}

long RHI::CD3D12DescriptorBuffer::SetupRanges(uint32_t space, CDescriptorBufferRanges& ranges, bool sampler) noexcept
{
    D3D12_DESCRIPTOR_RANGE1 range = {};
    range.NumDescriptors = 1;
    range.RegisterSpace = space;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    constexpr uint16_t MASK_FOR_SAMPLER =
        ~uint16_t(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE |
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    // TODO: combine if check compatibility
    for (const auto& item : m_data) {
        switch (item.bits.type)
        {
        case RHI::DESCRIPTOR_TYPE_B:
            if (sampler)
                continue;
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAGS(item.bits.flags);
            break;
        case RHI::DESCRIPTOR_TYPE_T:
            if (sampler)
                continue;
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAGS(item.bits.flags);
            break;
        case RHI::DESCRIPTOR_TYPE_U:
            if (sampler)
                continue;
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAGS(item.bits.flags);
            break;
        case RHI::DESCRIPTOR_TYPE_S:
            if (!sampler)
                continue;
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAGS(item.bits.flags & MASK_FOR_SAMPLER);
            break;
        }

        range.BaseShaderRegister = item.bits.binding;
        if (!ranges.push_back(range))
            return E_OUTOFMEMORY;
    }

    return S_OK;
}

namespace Slim { namespace impl {

    struct alignas(uint32_t) buffer_info_d3d12 {

        uint16_t        index;

        uint16_t        mask;

        uint16_t        visibility;
    };

} }

RHI::CODE RHI::CD3D12DescriptorBuffer::Init(const DescriptorBufferDesc& desc) noexcept
{
    const auto count = desc.slotCount;
    const auto slots = desc.slots;
    if (!count)
        return CODE_INVALIDARG;
    if (!m_data.resize(count))
        return CODE_OUTOFMEMORY;

    m_uDescriptorSizeNormal = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_uDescriptorSizeSampler = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    HRESULT hr = S_OK;

    // Count descriptor types
    impl::buffer_info_d3d12 infos[2] = {};

    for (uint32_t i = 0; i != count; ++i) {
        const auto& item = slots[i];
        if (item.type >= COUNT_OF_DESCRIPTOR_TYPE)
            return CODE_INVALIDARG;
        auto& bits = m_data[i].bits;
        auto& info = infos[item.type == DESCRIPTOR_TYPE_S];
        // ADD MASK
        info.mask |= uint16_t(1) << uint16_t(item.visibility);
        bits.index = info.index++;
        bits.type = item.type;
        bits.visibility = impl::rhi_d3d12(item.visibility);
        bits.flags = impl::rhi_d3d12_rr_flags(item.volatility);
        bits.binding = item.binding;

        info.visibility = bits.visibility;
    }


    // CREATE CBV_SRV_UAV DESCRIPTOR HEAP
    if (SUCCEEDED(hr) && infos[0].index) {
        m_eNormalVisibility = impl::popcount(infos[0].mask) == 1
            ? D3D12_SHADER_VISIBILITY(infos[0].visibility)
            : D3D12_SHADER_VISIBILITY_ALL
            ;
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = infos[0].index;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        assert(m_pNormalHeap == nullptr);
        hr = m_pDevice->CreateDescriptorHeap(
            &heapDesc, 
            IID_ID3D12DescriptorHeap,
            reinterpret_cast<void**>(&m_pNormalHeap)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateDescriptorHeap(fixed nomral) failed: %u", hr);
        }
    }

    // CREATE CBV_SRV_UAV DESCRIPTOR HEAP
    if (SUCCEEDED(hr) && infos[1].index) {
        m_eSamplerVisibility = impl::popcount(infos[1].mask) == 1
            ? D3D12_SHADER_VISIBILITY(infos[1].visibility)
            : D3D12_SHADER_VISIBILITY_ALL
            ;
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = infos[1].index;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        assert(m_pSamplerHeap == nullptr);
        hr = m_pDevice->CreateDescriptorHeap(
            &heapDesc,
            IID_ID3D12DescriptorHeap,
            reinterpret_cast<void**>(&m_pSamplerHeap)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateDescriptorHeap(fixed sampler) failed: %u", hr);
        }
    }

    return impl::d3d12_rhi(hr);
}

#endif

