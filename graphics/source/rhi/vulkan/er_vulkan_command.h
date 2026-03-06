#pragma once
#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_command.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"


namespace Slim { namespace RHI {

    class CVulkanDevice;
    class CVulkanSwapChain;
    class CVulkanCmdPool;

    class CVulkanCmdList final : public impl::ref_count_class<CVulkanCmdList, IRHICommandList> {
    public:

        CVulkanCmdList(CVulkanCmdPool* pool, VkCommandBuffer buffer) noexcept;

        ~CVulkanCmdList() noexcept;

        auto GetHandle() const noexcept { return m_hCommandBuffer; }

        auto&RefPool() noexcept { return *m_pCmdPool; }

    public:

        CODE Begin() noexcept override;

        CODE End() noexcept override;

        void BeginRenderPass(const BeginRenderPassDesc*) noexcept override;

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

        CVulkanCmdPool*             m_pCmdPool = nullptr;

        //VkDevice                    m_hDevice = VK_NULL_HANDLE;

        VkCommandBuffer             m_hCommandBuffer = VK_NULL_HANDLE;

        PFN_vkCmdPushDescriptorSetKHR   m_fnCmdPushDescriptorSetKHR = nullptr;
    };

    class CVulkanCmdQueue final : public impl::ref_count_class<CVulkanCmdQueue, IRHICommandQueue> {
    public:

        CVulkanCmdQueue(CVulkanDevice*, VkQueue queue) noexcept;

        ~CVulkanCmdQueue() noexcept;

        VkResult Init(CVulkanSwapChain* swapchain = nullptr) noexcept;

    public: // FOR MAIN GRAPHICS QUEUE
        // called before acquire frame if main graphics queue
        void AfterAcquire() noexcept;
        // called before present frame if main graphics queue
        void BeforePresent() noexcept;

    public:

        CODE Submit(uint32_t count, IRHICommandList* const*, const SubmitSynch* synch = nullptr) noexcept override;

        CODE WaitIdle() noexcept override;

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        //IRHIMutex*                  m_pMutex = nullptr;

        VkQueue                     m_hQueue = nullptr;

        VkSemaphore                 m_hTimelineChain = VK_NULL_HANDLE;

        VkSemaphore                 m_hNextWaitRef = VK_NULL_HANDLE;

        uint64_t                    m_u64ChainValue = 0;

        //uint64_t                    m_u64NextWaitValue = 0;

        VkAllocationCallbacks*      m_pAllocator = nullptr;

        impl::weak<CVulkanSwapChain>m_wrSwapChain{};

        pod::vector<uint8_t>        m_vLocalBuffer;

        //uint32_t                    m_eNextWaitFlag = 0;

    };


    class CVulkanCmdPool final : public impl::ref_count_class<CVulkanCmdPool, IRHICommandPool> {
    public:

        CVulkanCmdPool(CVulkanDevice*, VkCommandPool pool) noexcept;

        ~CVulkanCmdPool() noexcept;

        auto GetHandle() const noexcept { return m_hCommandPool; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto GetDeviceHandle() const noexcept { return m_hDevice; }

    public:

        CODE Reset() noexcept override;

        CODE CreateCommandList(IRHICommandList** ppCmdList) noexcept override;

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        VkCommandPool               m_hCommandPool = VK_NULL_HANDLE;

    };
}}

#endif