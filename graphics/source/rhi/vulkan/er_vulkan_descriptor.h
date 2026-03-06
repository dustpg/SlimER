#pragma once
#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_descriptor.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"

namespace Slim { namespace RHI {

    class CVulkanDevice;

    //struct DescriptorBufferDescVulkan : DescriptorBufferDesc {
    //    VkDescriptorPool            pool;
    //    VkDescriptorSetLayout       setLayout;
    //    VkDescriptorSet             set;
    //};

    enum SLOT_TYPE_VK : uint32_t {
        SLOT_TYPE_VK_NULL = 0,
        SLOT_TYPE_VK_IMAGE,
        SLOT_TYPE_VK_SAMPLER,
    };

    struct DescriptorBufferSlotVulkan {
        SLOT_TYPE_VK                type;
        IRHIBase*                   resource;
        union
        {
            VkImageView             image;
            VkSampler               sampler;
        };
    };

    class CVulkanDescriptorBuffer final : public impl::ref_count_class<CVulkanDescriptorBuffer, IRHIDescriptorBuffer> {
    public:

        CVulkanDescriptorBuffer(CVulkanDevice* pDevice) noexcept;

        ~CVulkanDescriptorBuffer() noexcept;

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto GetSetHandle() noexcept { return m_hSet; }

        auto GetLayoutHandle() noexcept { return m_hSetLayout; }

        //auto&RefDesc() const noexcept { return m_sDesc; }

        CODE Init(const DescriptorBufferDesc&) noexcept;

    public:

        void Unbind(uint32_t slot) noexcept override;

        CODE BindImage(const BindImageDesc* pDesc) noexcept override;

        CODE BindSampler(uint32_t slot, const SamplerDesc* pDesc) noexcept override;

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;


        VkDescriptorPool            m_hPool = VK_NULL_HANDLE;

        VkDescriptorSetLayout       m_hSetLayout = VK_NULL_HANDLE;

        VkDescriptorSet             m_hSet = VK_NULL_HANDLE;

        pod::vector<DescriptorBufferSlotVulkan>    
                                    m_data;

        uint32_t                    m_uSlotCount = 0;

    };

}}
#endif

