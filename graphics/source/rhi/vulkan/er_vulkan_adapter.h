#pragma once
#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_adapter.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"


namespace Slim { namespace RHI {

    class CVulkanInstance;

    class CVulkanAdapter final : public impl::ref_count_class<CVulkanAdapter, IRHIAdapter>  {
    public:

        CVulkanAdapter(CVulkanInstance* ins, VkPhysicalDevice) noexcept;

        ~CVulkanAdapter() noexcept;

        void GetDesc(AdapterDesc*) noexcept override;

        auto&RefInstance() noexcept { return *m_pInstance; }

        auto GetHandle() const noexcept { return m_hAdapter; }

        auto GetGraphicsIndex() const noexcept { return m_iGraphicsIndex; }

        auto GetComputeIndex() const noexcept { return m_iComputeIndex; }

        auto GetTransferIndex() const noexcept { return m_iTransferIndex; }

        const auto& RefProperties() const noexcept { return m_vQueueProperties; }

    protected:

        CVulkanInstance*        m_pInstance = nullptr;

        AdapterDesc             m_sDesc{};

        VkPhysicalDevice        m_hAdapter = VK_NULL_HANDLE;

        int32_t                 m_iGraphicsIndex = -1;

        int32_t                 m_iTransferIndex = -1;

        int32_t                 m_iComputeIndex = -1;

        pod::vector<VkQueueFamilyProperties>    m_vQueueProperties;
    };

}}


#endif
