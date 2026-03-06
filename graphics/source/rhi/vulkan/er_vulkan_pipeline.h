#pragma once
#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_pipeline.h>
#include <rhi/er_device.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"

namespace Slim { namespace RHI {

    class CVulkanDevice;
    class CVulkanPipelineLayout;

    class CVulkanShader final : public impl::ref_count_class<CVulkanShader, IRHIShader> {
    public:

        CVulkanShader(CVulkanDevice* pDevice, VkShaderModule shaderModule) noexcept;

        ~CVulkanShader() noexcept;

        CODE SetEntry(const char*) noexcept;

        auto GetEntry() const noexcept { return m_szEntry; }

        auto GetHandle() const noexcept { return m_hShaderModule; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        VkShaderModule              m_hShaderModule = VK_NULL_HANDLE;

        char                        m_szEntry[MAX_SHADER_ENTRY_LENGTH + 1] = "main";

    };

    class CVulkanPipeline final : public impl::ref_count_class<CVulkanPipeline, IRHIPipeline> {
    public:

        CVulkanPipeline(CVulkanPipelineLayout* pLayout, VkPipeline pipeline) noexcept;

        ~CVulkanPipeline() noexcept;

        auto GetHandle() const noexcept { return m_hPipeline; }

        auto&RefLayout() noexcept { return *m_pThisLayout; }

    protected:

        CVulkanPipelineLayout*      m_pThisLayout = nullptr;

        VkPipeline                  m_hPipeline = VK_NULL_HANDLE;


    };

    struct PipelineLayoutDescVulkan : PipelineLayoutDesc {

        VkDescriptorSetLayout           pushDesc;
        VkPipelineLayout                layout;
        VkDescriptorSet                 staticSamplerSet;
        pod::vector_soo< VkSampler, 4>* staticSamplers;
    };

    class CVulkanPipelineLayout final : public impl::ref_count_class<CVulkanPipelineLayout, IRHIPipelineLayout> {
    public:

        CVulkanPipelineLayout(CVulkanDevice* pDevice, const PipelineLayoutDescVulkan& desc) noexcept;

        ~CVulkanPipelineLayout() noexcept;

        auto GetHandle() const noexcept { return m_hPipelineLayout; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto Binding(uint32_t i) const noexcept { return static_cast<uint32_t>(m_szBinding[i]); };

        auto StaticSamplerSetHandle() const noexcept { return m_hStaticSamplerSet; }

        // Descriptor set indices in pipeline layout
        auto StaticSamplerSetId() const noexcept { return m_uStaticSamplerSetId; }

        auto DescriptorBufferSetId() const noexcept { return m_uDescriptorBufferSetId; }
    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        VkDescriptorSetLayout       m_hPushDescriptor = VK_NULL_HANDLE;

        VkPipelineLayout            m_hPipelineLayout = VK_NULL_HANDLE;

        VkDescriptorSet             m_hStaticSamplerSet = VK_NULL_HANDLE;


        uint16_t                    m_szBinding[MAX_PUSH_DESCRIPTOR] = {};

        pod::vector_soo<VkSampler,4>m_vStaticSamplers;

        uint32_t                    m_uStaticSamplerSetId = 0;

        uint32_t                    m_uDescriptorBufferSetId = 0;

    };

#if 0
    class CVulkanRenderPass final : public impl::ref_count_class<CVulkanRenderPass, IRHIRenderPass> {
    public:

        CVulkanRenderPass(CVulkanDevice* pDevice, VkRenderPass renderPass) noexcept;

        ~CVulkanRenderPass() noexcept;

        auto GetHandle() const noexcept { return m_hRenderPass; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        VkRenderPass                m_hRenderPass = VK_NULL_HANDLE;


    };
#endif

}}

#endif
