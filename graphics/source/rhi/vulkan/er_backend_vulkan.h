#pragma once

#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"
#include <vulkan/vulkan.h>

namespace Slim { namespace RHI {

    class CVulkanInstance final : public impl::ref_count_class<CVulkanInstance, IRHIInstance> {
    public:

        CVulkanInstance() noexcept;

        ~CVulkanInstance() noexcept;

        CODE Init(const InstanceDesc*) noexcept;

    public:

        CODE EnumAdapters(uint32_t index, IRHIAdapter**) noexcept override;

        CODE GetDefaultAdapter(IRHIAdapter**) noexcept override;

        CODE CreateDevice(IRHIAdapter* adapter, IRHIDevice** outDevice) noexcept override;

        CODE CreateShaderCompiler(const char* dyLib, IRHIShaderCompiler**) noexcept override;

    public:

        auto GetHandle() const noexcept { return m_hInstance; }

    protected:

        void clear_physical_dDevices() noexcept;

    protected:

        VkInstance                          m_hInstance = VK_NULL_HANDLE;

        pod::vector<VkPhysicalDevice>       m_vPhysicalDevices;

        uint32_t                            m_uMaxPushDescriptors = 0;

    };


    CODE CreateInstanceVulkan(const InstanceDesc* pDesc, IRHIInstance** ptr) noexcept;


}}

#endif
