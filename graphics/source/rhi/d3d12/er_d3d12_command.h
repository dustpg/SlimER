#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_command.h>
#include "../common/er_helper_p.h"

struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct ID3D12GraphicsCommandList4;
struct ID3D12CommandAllocator;

namespace Slim { namespace RHI {

    class CD3D12Device;
    class CD3D12FrameBuffer;

    class CD3D12CommandPool final : public impl::ref_count_class<CD3D12CommandPool, IRHICommandPool> {
    public:

        CD3D12CommandPool(CD3D12Device* pDevice, ID3D12CommandAllocator* pCommandAllocator, uint32_t commandListType) noexcept;

        ~CD3D12CommandPool() noexcept;

        auto&RefDevice() noexcept { return *m_pDevice; }

        auto&RefAllocator() const noexcept { return m_pCommandAllocator; }

    public:

        CODE Reset() noexcept override;

        CODE CreateCommandList(IRHICommandList** ppCmdList) noexcept override;

    protected:

        CD3D12Device*                   m_pDevice = nullptr;

        ID3D12CommandAllocator*         m_pCommandAllocator = nullptr;

        uint32_t                        m_eCommandListType = 0;

    };

    class CD3D12CommandList final : public impl::ref_count_class<CD3D12CommandList, IRHICommandList> {
    public:

        CD3D12CommandList(CD3D12CommandPool* pPool, ID3D12GraphicsCommandList4* pCommandList) noexcept;

        ~CD3D12CommandList() noexcept;

        auto&RefCommandPool() noexcept { return *m_pCommandPool; }

        auto&RefCommandList() noexcept { return *m_pCommandList; }

    public:

        CODE Begin() noexcept override;

        CODE End() noexcept override;

        void BeginRenderPass(const BeginRenderPassDesc* pDesc) noexcept override;

        void EndRenderPass() noexcept override;

        void BindPipeline(IRHIPipeline* pPipeline) noexcept override;

        void BindDescriptorBuffer(IRHIPipelineLayout* pLayout, IRHIDescriptorBuffer* pDescriptorBuffer) noexcept override;

        void BindVertexBuffers(uint32_t firstBinding, uint32_t count, const VertexBufferView* pViews) noexcept override;

        void BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, INDEX_TYPE type) noexcept override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) noexcept override;

        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) noexcept override;

        void ResourceBarriers(uint32_t count, const ResourceBarrierDesc* pList) noexcept override;

        void CopyBuffer(const CopyBufferDesc* pDesc) noexcept override;

        void CopyImage(const CopyImageDesc* pDesc) noexcept override;

        void CopyBufferToImage(const CopyBufferImageDesc* pDesc) noexcept override;

        void SetViewports(uint32_t count, const Viewport* pViewports) noexcept override;

        void SetScissors(uint32_t count, const Rect* pRects) noexcept override;

        void PushDescriptorSets(IRHIPipelineLayout* pLayout, uint32_t count, const PushDescriptorSetDesc* pDesc) noexcept override;

    protected:

        CD3D12CommandPool*                  m_pCommandPool = nullptr;

        ID3D12GraphicsCommandList4*         m_pCommandList = nullptr;

        //CD3D12FrameBuffer*                  m_pPassFrameBuffer = nullptr;

    };

    class CD3D12CommandQueue final : public impl::ref_count_class<CD3D12CommandQueue, IRHICommandQueue> {
    public:

        CD3D12CommandQueue(CD3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) noexcept;

        ~CD3D12CommandQueue() noexcept;

        auto&RefCommandQueue() noexcept { return *m_pCommandQueue; }

    public:

        //void ExecuteCommandLists(uint32_t count, IRHICommandList* const*) noexcept override;

        //CODE SignalFence(IRHIFence* pFence, uint64_t value) noexcept override;

        CODE Submit(uint32_t count, IRHICommandList* const*, const SubmitSynch* synch) noexcept override;

        CODE WaitIdle() noexcept override;

    protected:

        CD3D12Device*                   m_pDevice = nullptr;

        ID3D12CommandQueue*             m_pCommandQueue = nullptr;

        uint64_t                        m_uIdleValue = 0;

        ID3D12Fence*                    m_pIdleFence = nullptr;

        void*                           m_hIdleEvent = nullptr;

    };
}}
#endif

