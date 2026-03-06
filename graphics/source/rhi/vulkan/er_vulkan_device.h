#pragma once
#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_device.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"

VK_DEFINE_HANDLE(VmaAllocator)

namespace Slim { namespace RHI {

    class CVulkanInstance;
    class CVulkanAdapter;


    enum : uint32_t {
        MAX_STATIC_SAMPLER_SET = 1024,
    };


    class CVulkanDevice final : public impl::ref_count_class<CVulkanDevice, IRHIDevice> {
    public:

        CVulkanDevice(CVulkanAdapter*, VkDevice device) noexcept;

        ~CVulkanDevice() noexcept;

        CODE Flush() noexcept override;

        CODE CreateSwapChain(const SwapChainDesc* desc, IRHISwapChain** ppSwapChain) noexcept override;

        CODE CreateCommandPool(const CommandPoolDesc* desc, IRHICommandPool** ppPool) noexcept override;

        CODE CreateCommandQueue(const CommandQueueDesc* desc, IRHICommandQueue** ppQueue) noexcept override;

        CODE CreateFence(uint64_t initialValue, IRHIFence** ppFence) noexcept override;

        CODE CreateBuffer(const BufferDesc* desc, IRHIBuffer** ppBuffer) noexcept override;

        CODE CreateShader(const void* data, size_t size, IRHIShader** ppBuffer, const char* entry) noexcept override;

        CODE CreatePipeline(const PipelineDesc* pDesc, IRHIPipeline** ppPipeline) noexcept override;

        CODE CreatePipelineLayout(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept override;

#if 0
        CODE CreateRenderPass(const RenderPassDesc* pDesc, IRHIRenderPass** ppRenderPass) noexcept override;

        CODE CreateFrameBuffer(const FrameBufferDesc* pDesc, IRHIFrameBuffer** ppFrameBuffer) noexcept override;
#endif

        CODE CreateImage(const ImageDesc* pDesc, IRHIImage** ppImage) noexcept override;

        CODE CreateAttachment(const AttachmentDesc* pDesc, IRHIImage * pImage, IRHIAttachment** ppView) noexcept override;

        CODE CreateDescriptorBuffer(const DescriptorBufferDesc* pDesc, IRHIDescriptorBuffer** ppBuffer) noexcept override;
        
    public:

        CODE Init() noexcept;

        auto&RefAdapter() noexcept { return *m_pAdapter; }

        auto GetHandle() const noexcept { return m_hDevice; }

        auto FnCmdPushDescriptorSet() const noexcept { return m_fnCmdPushDescriptorSetKHR; }

        void DisposeStaticSamplerSet(VkDescriptorSet& set) noexcept;

    protected:

        PFN_vkCmdPushDescriptorSetKHR   m_fnCmdPushDescriptorSetKHR = nullptr;

    protected:

        CVulkanAdapter*             m_pAdapter = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        VmaAllocator                m_hVmaAllocator = VK_NULL_HANDLE;

        VkDescriptorPool            m_hStaticSamplerPool = VK_NULL_HANDLE;

    public:

        //void* debug_data = nullptr;
    };

#if 0
    /*class CVulkanFenceBasic final : public impl::ref_count_class<CVulkanFence, IRHIFence> {
    public:

        CVulkanFenceBasic(CVulkanDevice* pDevice) noexcept;

        ~CVulkanFenceBasic() noexcept;

        void Init(VkFence h) noexcept { m_hFence = h; }

        uint64_t GetCompletedValue() noexcept override;

    protected:

        CVulkanDevice* m_pThisDevice = nullptr;

        VkDevice                m_hDevice = VK_NULL_HANDLE;

        VkFence                 m_hFence = VK_NULL_HANDLE;

    };*/
#endif

    class CVulkanFence final : public impl::ref_count_class<CVulkanFence, IRHIFence> {
    public:

        CVulkanFence(CVulkanDevice* pDevice, VkSemaphore) noexcept;

        ~CVulkanFence() noexcept;

    public:

        auto GetHandle() const noexcept { return m_hTimelineSemaphore; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        WAIT Wait(uint64_t value, uint32_t time) noexcept override;

        uint64_t GetCompletedValue() noexcept override;

    protected:

        CVulkanDevice*          m_pThisDevice = nullptr;

        VkDevice                m_hDevice = VK_NULL_HANDLE;

        VkSemaphore             m_hTimelineSemaphore = VK_NULL_HANDLE;

    };

}}

#endif