#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include "er_vulkan_adapter.h"
#include "../common/er_rhi_common.h"

using namespace Slim;

namespace Slim{ namespace impl {


    static int32_t get_nice_index_graph(const VkQueueFamilyProperties* props, size_t len, uint32_t bit) noexcept {
        const auto len32 = uint32_t(len);
        for (uint32_t i = 0; i != len32; ++i) {
            const auto& family = props[i];
            if (family.queueFlags & bit)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    static int32_t get_nice_index(const VkQueueFamilyProperties* props, size_t len, uint32_t bit) noexcept {
        const auto len32 = uint32_t(len);
        // 1..3
        for (uint32_t j = 1; j < 4; ++j) {
            for (uint32_t i = 0; i != len32; ++i) {
                const auto& family = props[i];
                if ((family.queueFlags & bit) && j == popcount(family.queueFlags))
                    return static_cast<int32_t>(i);
            }
        }
        // 3+
        return get_nice_index_graph(props, len, bit);
    }
}}


RHI::CVulkanAdapter::CVulkanAdapter(CVulkanInstance* ins, VkPhysicalDevice h) noexcept
    : m_pInstance(ins)
    , m_hAdapter(h)
{
    VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    assert(ins && h);
    ins->AddRefCnt();
    VkPhysicalDeviceProperties props{};
    ::vkGetPhysicalDeviceProperties(h, &props);
    VkPhysicalDeviceMemoryProperties memProps{};
    ::vkGetPhysicalDeviceMemoryProperties(h, &memProps);
    static_assert(sizeof(m_sDesc.name) <= sizeof(props.deviceName), "SIZE");
    std::memcpy(m_sDesc.name, props.deviceName, sizeof(m_sDesc.name));
#if 0
    if constexpr (sizeof(m_sDesc.name) < sizeof(props.deviceName))
        m_sDesc.name[sizeof(m_sDesc.name) / sizeof(m_sDesc.name[0]) - 1] = 0;
#endif
    m_sDesc.api = API::VULKAN;
    m_sDesc.deviceID = props.deviceID;
    m_sDesc.vendorID = props.vendorID;

    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        const auto& heap = memProps.memoryHeaps[i];
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_sDesc.vram = heap.size;
            break;
        }
    }

    uint32_t queueFamilyCount = 0;
    ::vkGetPhysicalDeviceQueueFamilyProperties(h, &queueFamilyCount, nullptr);
    if (!m_vQueueProperties.resize(queueFamilyCount))
        return;
    ::vkGetPhysicalDeviceQueueFamilyProperties(h, &queueFamilyCount, m_vQueueProperties.data());

    // GRAPHICS
    m_iGraphicsIndex = impl::get_nice_index_graph(
        m_vQueueProperties.data(),
        m_vQueueProperties.size(),
        VK_QUEUE_GRAPHICS_BIT
    );

    m_iTransferIndex = impl::get_nice_index(
        m_vQueueProperties.data(),
        m_vQueueProperties.size(),
        VK_QUEUE_TRANSFER_BIT
    );

    m_iComputeIndex = impl::get_nice_index(
        m_vQueueProperties.data(),
        m_vQueueProperties.size(),
        VK_QUEUE_COMPUTE_BIT
    );

    // CPU 
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        m_sDesc.feature = ADAPTER_FEATURE(m_sDesc.feature | ADAPTER_FEATURE_CPU);

    // 2D FEATURE = GRAPHICS
    if (m_iGraphicsIndex >= 0)
        m_sDesc.feature = ADAPTER_FEATURE(m_sDesc.feature | ADAPTER_FEATURE_2D);

}

RHI::CVulkanAdapter::~CVulkanAdapter() noexcept
{
    RHI::SafeDispose(m_pInstance);
}

void RHI::CVulkanAdapter::GetDesc(AdapterDesc* pDesc) noexcept
{
    if (pDesc)
        *pDesc = m_sDesc;
}
#endif