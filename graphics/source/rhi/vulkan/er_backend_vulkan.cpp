#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cassert>
#include <cstdio>
#include "er_vulkan_adapter.h"
#include "er_vulkan_device.h"
#include "er_rhi_vulkan.h"
#include "../common/er_shader_compiler_p.h"

using namespace Slim;



namespace Slim{ namespace impl {

    static bool check_layer(const char* layers[], size_t count) noexcept {
        if (!count)
            return true;
        uint32_t layerCount;
        ::vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        pod::vector<VkLayerProperties> availableLayers;
        if (!availableLayers.resize(layerCount))
            return false;

        ::vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (size_t i = 0; i != count; ++i) {
            bool layerFound = false;
            const auto layerName = layers[i];
            // TODO: std::any_of
            for (const auto& layerProps : availableLayers) {
                if (!std::strcmp(layerName, layerProps.layerName)) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
                return false;
        }
        return true;
    }

    static bool check_ext(const char* exts[], size_t count) noexcept {
        if (!count)
            return true;
        uint32_t layerCount;
        ::vkEnumerateInstanceExtensionProperties(nullptr, &layerCount, nullptr);
        pod::vector<VkExtensionProperties> availableExts;
        if (!availableExts.resize(layerCount))
            return false;

        ::vkEnumerateInstanceExtensionProperties(nullptr, &layerCount, availableExts.data());

        for (size_t i = 0; i != count; ++i) {
            bool extFound = false;
            const auto extName = exts[i];
            // TODO: std::any_of
            for (const auto& extProps : availableExts) {
                if (!std::strcmp(extName, extProps.extensionName)) {
                    extFound = true;
                    break;
                }
            }
            if (!extFound)
                return false;
        }
        return true;
    }

    static bool check_ext(const char* exts[], size_t count, VkPhysicalDevice device) noexcept {
        if (!count)
            return true;
        uint32_t layerCount;
        ::vkEnumerateDeviceExtensionProperties(device, nullptr, &layerCount, nullptr);
        pod::vector<VkExtensionProperties> availableExts;
        if (!availableExts.resize(layerCount))
            return false;

        ::vkEnumerateDeviceExtensionProperties(device, nullptr, &layerCount, availableExts.data());

        for (size_t i = 0; i != count; ++i) {
            bool extFound = false;
            const auto extName = exts[i];
            // TODO: std::any_of
            for (const auto& extProps : availableExts) {
                if (!std::strcmp(extName, extProps.extensionName)) {
                    extFound = true;
                    break;
                }
            }
            if (!extFound)
                return false;
        }
        return true;
    }

}}


void RHI::CVulkanInstance::clear_physical_dDevices() noexcept
{
    m_vPhysicalDevices.clear();
}

RHI::CVulkanInstance::CVulkanInstance() noexcept
{
}

RHI::CVulkanInstance::~CVulkanInstance() noexcept
{
    this->clear_physical_dDevices();
    if (m_hInstance != VK_NULL_HANDLE) {
        ::vkDestroyInstance(m_hInstance, &gc_sAllocationCallbackDRHI);
        m_hInstance = VK_NULL_HANDLE;
    }
}

RHI::CODE RHI::CVulkanInstance::Init(const InstanceDesc* pDesc) noexcept
{
    assert(m_hInstance == VK_NULL_HANDLE);

    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "SLIM RHI APP";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "SLIM RHI";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    // 1.1
    //  - vkGetPhysicalDeviceProperties2
    //  - negative viewport height
    appInfo.apiVersion = VK_API_VERSION_1_1;
    // 1.2
    //  - timeline
    //  - descriptor indexing
    appInfo.apiVersion = VK_API_VERSION_1_2;
    // 1.3 => swiftshader
    //  - submit2. [?]
    //  - dynamic rendering. TODO: fallback?
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;

    // TODO: VK_EXT_conditional_rendering
    // TODO: VK_KHR_imageless_framebuffer 

    const char* neededLayers[] = {
        "VK_LAYER_KHRONOS_validation",  // DEBUG LAYER MUST BE LAST
    };

    const char* neededExts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
        "VK_KHR_win32_surface",
#elif defined(__APPLE__)
        "VK_EXT_metal_surface",
#elif defined(__ANDROID__)
        "VK_KHR_android_surface",
#elif defined(__linux__)
        "VK_KHR_xcb_surface",
#endif
        // TODO: check support
        //VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        "VK_EXT_debug_utils",  // DEBUG UTILS MUST BE LAST

    };

    CODE code{};

    // CHECK LAYERS
    if (RHI::Success(code)) {
        constexpr uint32_t layersCountAll = sizeof(neededLayers) / sizeof(neededLayers[0]);
        const uint32_t layersCount = pDesc->debug ? layersCountAll : layersCountAll - 1;
        const auto layersAvailable = impl::check_layer(neededLayers, layersCount);

        if (layersAvailable) {
            createInfo.enabledLayerCount = layersCount;
            createInfo.ppEnabledLayerNames = neededLayers;
        }
        else {
            DRHI_LOGGER_ERR("Validation layers requested but not available!");
            code = CODE_UNEXPECTED;
        }
    }

    // CHECK EXTS
    if (RHI::Success(code)) {
        constexpr auto extsCountAll = sizeof(neededExts) / sizeof(neededExts[0]);
        const uint32_t extsCount = pDesc->debug ? extsCountAll : extsCountAll - 1;
        const auto extsAvailable = impl::check_ext(neededExts, extsCount);

        if (extsAvailable) {
            createInfo.enabledExtensionCount = extsCount;
            createInfo.ppEnabledExtensionNames = neededExts;
        }
        else {
            DRHI_LOGGER_ERR("Extension requested but not available!");
            code = CODE_UNEXPECTED;
        }
    }

    // VULKAN CREATION
    if (RHI::Success(code)) {
        const auto result = ::vkCreateInstance(&createInfo, &gc_sAllocationCallbackDRHI, &m_hInstance);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateInstance but failed!");
            code = impl::vulkan_rhi(result);
        }
    }

    return code;
}


RHI::CODE RHI::CreateInstanceVulkan(const InstanceDesc* pDesc, IRHIInstance** ppIns) noexcept
{
    assert(ppIns && pDesc);
    *ppIns = nullptr;
    if (const auto obj = new(std::nothrow) CVulkanInstance) {

        const auto code = obj->Init(pDesc);
        if (RHI::Success(code)) {
            *ppIns = obj;
            return CODE_OK;
        }
        else {
            obj->Dispose();
            return code;
        }
    }
    return CODE_OUTOFMEMORY;
}


RHI::CODE RHI::CVulkanInstance::EnumAdapters(uint32_t index, IRHIAdapter** ppAdapter) noexcept
{

    if (m_vPhysicalDevices.empty()) {
        uint32_t deviceCount = 0;
        ::vkEnumeratePhysicalDevices(m_hInstance, &deviceCount, nullptr);
        if (!deviceCount) {
            return CODE_RHI_NOT_FOUND;
        }
        if (!m_vPhysicalDevices.resize(deviceCount)) {
            return CODE_OUTOFMEMORY;
        }
        ::vkEnumeratePhysicalDevices(m_hInstance, &deviceCount, m_vPhysicalDevices.data());
    }

    if (index < m_vPhysicalDevices.size()) {
        const auto device = m_vPhysicalDevices[index];
        if (const auto obj = new (std::nothrow) CVulkanAdapter{ this, device }) {
            *ppAdapter = obj;
            return CODE_OK;
        }
        else {
            return CODE_OUTOFMEMORY;
        }
    }

    return CODE_RHI_NOT_FOUND;
}


RHI::CODE RHI::CVulkanInstance::GetDefaultAdapter(IRHIAdapter** ppAdapter) noexcept
{
    return this->EnumAdapters(0, ppAdapter);
}


RHI::CODE RHI::CVulkanInstance::CreateShaderCompiler(const char* dyLib, IRHIShaderCompiler** ppCompiler) noexcept
{
#ifndef SLIM_RHI_NO_SHADER_COMPILER
    return RHI::CreateShaderCompiler(dyLib, ppCompiler, "vulkan");
#else
    return CODE_NOTIMPL;
#endif
}

RHI::CODE RHI::CVulkanInstance::CreateDevice(IRHIAdapter* pAdapter, IRHIDevice** ppDevice) noexcept
{
    if (!pAdapter || !ppDevice)
        return CODE_POINTER;
    RHI::AdapterDesc desc;
    pAdapter->GetDesc(&desc);
    if (desc.api != API::VULKAN)
        return CODE_INVALIDARG;
    // TODO: SAFE SELF RTTI
    const auto pVulkanAdapter = static_cast<CVulkanAdapter*>(pAdapter);
    if (this != &pVulkanAdapter->RefInstance())
        return CODE_INVALIDARG;

    const auto handle = pVulkanAdapter->GetHandle();
    const char* neededExts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        // TODO: 1) check support 2) fallback
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        // TODO: 1) check support 2) fallback
        //VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        //VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
    };
    constexpr uint32_t length = sizeof(neededExts) / sizeof(neededExts[0]);
    if (!impl::check_ext(neededExts, length, handle)) {
        DRHI_LOGGER_ERR("CreateDevice - check_ext but failed!");
        return CODE_RHI_UNSUPPORTED;
    }

    // Descriptor Indexing

    // Query push descriptor support count
    VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR };
    //pushDescriptorProps.pNext = &indexingFeatures;
    VkPhysicalDeviceProperties2 deviceProps2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    deviceProps2.pNext = &pushDescriptorProps;
    ::vkGetPhysicalDeviceProperties2(handle, &deviceProps2);
    m_uMaxPushDescriptors = pushDescriptorProps.maxPushDescriptors;

    //VkPhysicalDeviceProperties props;
    //::vkGetPhysicalDeviceProperties(handle, &props);

    const auto graphicsFamilyIndex = static_cast<uint32_t>(pVulkanAdapter->GetGraphicsIndex());

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCreateInfo.queueFamilyIndex = graphicsFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDevice device = VK_NULL_HANDLE;
    // Vulkan 1.3 features
    //  - dynamic rendering
    VkPhysicalDeviceVulkan13Features features3{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features3.dynamicRendering = VK_TRUE;
    //features3.synchronization2 = VK_TRUE;

    // Vulkan 1.2 TIMELINE SEMAPHORE
    VkPhysicalDeviceVulkan12Features features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    // TODO: compatibility layer if TIMELINE SEMAPHORE not supported?
    features2.timelineSemaphore = VK_TRUE;
    // TODO: compatibility layer if DESCRIPTOR INDEXING not supported?
    features2.descriptorIndexing = VK_TRUE;
    features2.runtimeDescriptorArray = VK_TRUE;
    features2.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features2.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    features2.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    features2.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
    features2.shaderInputAttachmentArrayNonUniformIndexing = VK_TRUE;
    features2.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
    features2.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;
    features2.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    features2.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features2.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    features2.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    features2.descriptorBindingPartiallyBound = VK_TRUE;
    features2.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features2.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    features2.pNext = &features3;



    VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.pNext = &features2;

    deviceCreateInfo.enabledExtensionCount = sizeof(neededExts) / sizeof(neededExts[0]);
    deviceCreateInfo.ppEnabledExtensionNames = neededExts;

    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;

    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    deviceCreateInfo.pEnabledFeatures = nullptr;

    VkResult result = ::vkCreateDevice(
        handle,
        &deviceCreateInfo,
        &gc_sAllocationCallbackDRHI,
        &device
    );

    if (result != VK_SUCCESS) {
        DRHI_LOGGER_ERR("vkCreateDevice but failed!");
        return impl::vulkan_rhi(result);
    }

    CODE code = CODE_OUTOFMEMORY;
    // vulkan device handle --LIFE-->  CVulkanDevice
    if (const auto obj = new(std::nothrow) CVulkanDevice{ pVulkanAdapter, device }) {
        code = obj->Init();
        if (RHI::Success(code)) {
            *ppDevice = obj;
        }
        else {
            obj->Dispose();
            *ppDevice = nullptr;
        }
    }

    return code;
}

#endif