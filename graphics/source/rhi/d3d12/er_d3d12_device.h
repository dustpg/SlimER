#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_device.h>
#include "../common/er_helper_p.h"

struct ID3D12Device4;
struct ID3D12Fence;

namespace Slim { namespace RHI {

    class CD3D12Adapter;
    class CD3D12Device;

    /// <summary>
    /// Represents a Direct3D 12 device implementation of the RHI device interface.
    /// </summary>
    /// <remarks>
    /// This class wraps an ID3D12Device4 and provides RHI functionality for Direct3D 12.
    /// It is created by <see cref="CD3D12Instance::CreateDevice"/> and manages the lifetime
    /// of the underlying D3D12 device and its associated adapter.
    /// </remarks>
    class CD3D12Device final : public impl::ref_count_class<CD3D12Device, IRHIDevice> {
    public:

        CD3D12Device(CD3D12Adapter* pAdapter, ID3D12Device4* pDevice) noexcept;

        ~CD3D12Device() noexcept;
        // HRESULT
        long Init() noexcept;

        auto&RefDevice() noexcept { return *m_pDevice; }

    public:

        CODE Flush() noexcept override;

        CODE CreateSwapChain(const SwapChainDesc* pDesc, IRHISwapChain** ppSwapChain) noexcept override;

        CODE CreateCommandPool(const CommandPoolDesc* pDesc, IRHICommandPool** ppPool) noexcept override;

        CODE CreateCommandQueue(const CommandQueueDesc* pDesc, IRHICommandQueue** ppQueue) noexcept override;

        CODE CreateFence(uint64_t initialValue, IRHIFence** ppFence) noexcept override;

        CODE CreateBuffer(const BufferDesc* pDesc, IRHIBuffer** ppBuffer) noexcept override;

        CODE CreateShader(const void* data, size_t size, IRHIShader** ppShader, const char*) noexcept override;

        CODE CreatePipeline(const PipelineDesc* pDesc, IRHIPipeline** ppPipeline) noexcept override;

        CODE CreatePipelineLayout(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept override;

#if 0
        CODE CreateRenderPass(const RenderPassDesc* pDesc, IRHIRenderPass** ppRenderPass) noexcept override;

        CODE CreateFrameBuffer(const FrameBufferDesc* pDesc, IRHIFrameBuffer** ppFrameBuffer) noexcept override;
#endif

        CODE CreateImage(const ImageDesc* pDesc, IRHIImage** ppImage) noexcept override;

        CODE CreateAttachment(const AttachmentDesc* pDesc, IRHIImage * pImage, IRHIAttachment** ppView) noexcept override;

        CODE CreateDescriptorBuffer(const DescriptorBufferDesc* pDesc, IRHIDescriptorBuffer** ppBuffer) noexcept override;
        
    protected:

        CODE create_pipeline_layout_1_0(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept;

        CODE create_pipeline_layout_1_1(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept;

    protected:

        CD3D12Adapter*             m_pAdapter = nullptr;

        ID3D12Device4*             m_pDevice = nullptr;

    };

    /// <summary>
    /// Represents a Direct3D 12 fence implementation of the RHI fence interface.
    /// </summary>
    /// <remarks>
    /// This class wraps an ID3D12Fence and provides RHI functionality for Direct3D 12.
    /// It is created by <see cref="CD3D12Device::CreateFence"/> and manages the lifetime
    /// of the underlying D3D12 fence.
    /// </remarks>
    class CD3D12Fence final : public impl::ref_count_class<CD3D12Fence, IRHIFence> {
    public:

        CD3D12Fence(CD3D12Device* pDevice, ID3D12Fence* pFence, uint64_t initialValue) noexcept;

        ~CD3D12Fence() noexcept;

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto GetHandle() const noexcept { return m_pFence; }

    public:

        uint64_t GetCompletedValue() noexcept override;

        WAIT Wait(uint64_t value, uint32_t time) noexcept override;

    protected:

        CD3D12Device*              m_pThisDevice = nullptr;

        ID3D12Fence*               m_pFence = nullptr;

        void*                     m_hEvent = nullptr;

    };

}}
#endif