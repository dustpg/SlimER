#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cstdio>
#include "er_vulkan_descriptor.h"
#include "er_vulkan_device.h"
#include "er_vulkan_resource.h"
#include "er_rhi_vulkan.h"
#include "../common/er_rhi_common.h"

namespace Slim { namespace impl {

    static void clean(RHI::DescriptorBufferSlotVulkan& data, VkDevice d, const VkAllocationCallbacks* a) noexcept
    {
        RHI::SafeDispose(data.resource);
        switch (data.type)
        {
        case RHI::SLOT_TYPE_VK_IMAGE:
            ::vkDestroyImageView(d, data.image, a);
            break;
        case RHI::SLOT_TYPE_VK_SAMPLER:
            ::vkDestroySampler(d, data.sampler, a);
            break;
        }
    }
}}

using namespace Slim;

// ----------------------------------------------------------------------------
//                              CVulkanDescriptorBuffer
// ----------------------------------------------------------------------------

RHI::CVulkanDescriptorBuffer::CVulkanDescriptorBuffer(CVulkanDevice* pDevice) noexcept
    : m_pThisDevice(pDevice)
    //, m_sDesc(desc)
{
    assert(pDevice);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
}

RHI::CVulkanDescriptorBuffer::~CVulkanDescriptorBuffer() noexcept
{
    for (auto& item : m_data) {
        impl::clean(item, m_hDevice, &gc_sAllocationCallbackDRHI);
    }
    //if (m_hSet != VK_NULL_HANDLE && m_hPool != VK_NULL_HANDLE) {
    //    ::vkFreeDescriptorSets(m_hDevice, m_hPool, 1, &m_hSet);
    //    m_hSet = VK_NULL_HANDLE;
    //}
    if (m_hSetLayout != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorSetLayout(m_hDevice, m_hSetLayout, &gc_sAllocationCallbackDRHI);
        m_hSetLayout = VK_NULL_HANDLE;
    }
    if (m_hPool != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorPool(m_hDevice, m_hPool, &gc_sAllocationCallbackDRHI);
        m_hPool = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

RHI::CODE RHI::CVulkanDescriptorBuffer::BindImage(const BindImageDesc* pDesc) noexcept
{
    if (!pDesc)
        return CODE_POINTER;
    const auto slot = pDesc->slot;
    if (slot >= m_uSlotCount)
        return CODE_INVALIDARG;
    if (slot >= m_data.size()) {
        if (!m_data.resize(slot + 1))
            return CODE_OUTOFMEMORY;
    }

    //pod::vector_soo<VkDescriptorImageInfo, 32> imageInfos;
    //if (!imageInfos.resize(count))
    //    return CODE_OUTOFMEMORY;

    auto& slotData = m_data[slot];
    impl::clean(slotData, m_hDevice, &gc_sAllocationCallbackDRHI);

    VkResult result = VK_SUCCESS;
    // CREATE IMAGE VIEW
    const auto image = static_cast<CVulkanImage*>(pDesc->image);
    // SIMPLE RTTI
    assert(m_pThisDevice == &image->RefDevice());

    // CREATE IMAGE VIEW
    if (result == VK_SUCCESS) {
        const auto& imageDesc = image->RefDesc();
        // Use format from ImageViewDesc if specified, otherwise use format from Image
        const auto format = (pDesc->format != RHI::RHI_FORMAT_UNKNOWN)
            ? pDesc->format
            : imageDesc.format;

        VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = image->GetHandle();
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = impl::rhi_vulkan(format);

        // TODO: components/subresourceRange
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        result = ::vkCreateImageView(m_hDevice, &createInfo, &gc_sAllocationCallbackDRHI, &slotData.image);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateImageView(Fixed DescriptorBuffer) failed: %u", result);
        }
    }

    if (result == VK_SUCCESS) {
        slotData.type = SLOT_TYPE_VK_IMAGE;
        slotData.resource = image;
        image->AddRefCnt();

        //const auto slot = pDesc->slot;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageView = slotData.image;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_hSet;
        descriptorWrite.dstBinding = slot;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        ::vkUpdateDescriptorSets(m_hDevice, 1, &descriptorWrite, 0, nullptr);

    }

    return impl::vulkan_rhi(result);

}

RHI::CODE RHI::CVulkanDescriptorBuffer::BindSampler(uint32_t slot, const SamplerDesc* pDesc) noexcept
{
    if (!pDesc)
        return CODE_POINTER;
    if (slot >= m_uSlotCount)
        return CODE_INVALIDARG;
    if (slot >= m_data.size()) {
       if (!m_data.resize(slot + 1))
           return CODE_OUTOFMEMORY;
    }

    auto& slotData = m_data[slot];
    impl::clean(slotData, m_hDevice, &gc_sAllocationCallbackDRHI);

    VkResult result = VK_SUCCESS;
    // CREATE SAMPLER
    if (result == VK_SUCCESS) {
        VkSamplerCreateInfo samplerCI{};
        impl::rhi_vulkan_cvt(pDesc, &samplerCI);

        result = ::vkCreateSampler(m_hDevice, &samplerCI, &gc_sAllocationCallbackDRHI, &slotData.sampler);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateSampler(Fixed DescriptorBuffer) failed: %u", result);
        }
    }

    if (result == VK_SUCCESS) {
        slotData.type = SLOT_TYPE_VK_SAMPLER;
        slotData.resource = nullptr; // Sampler doesn't have a resource reference

        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = slotData.sampler;
        samplerInfo.imageView = VK_NULL_HANDLE;
        samplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_hSet;
        descriptorWrite.dstBinding = slot;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &samplerInfo;

        ::vkUpdateDescriptorSets(m_hDevice, 1, &descriptorWrite, 0, nullptr);
    }

    return impl::vulkan_rhi(result);
}

void Slim::RHI::CVulkanDescriptorBuffer::Unbind(uint32_t slot) noexcept
{
    // TODO
}

RHI::CODE RHI::CVulkanDescriptorBuffer::Init(const DescriptorBufferDesc& desc) noexcept
{
    const auto count = desc.slotCount;
    const auto slots = desc.slots;
    m_uSlotCount = count;
    pod::vector<VkDescriptorSetLayoutBinding> bindings;
    if (!bindings.resize(count))
        return CODE_OUTOFMEMORY;

    uint32_t counter[COUNT_OF_DESCRIPTOR_TYPE] = {};
    // SETUP BINDINGS
    for (uint32_t i = 0; i != count; ++i) {
        const auto& item = slots[i];
        auto& binding = bindings[i];
        if (item.type >= COUNT_OF_DESCRIPTOR_TYPE)
            return CODE_INVALIDARG;
        binding.binding = item.binding + desc.shift.shift[item.type].offset;
        counter[item.type]++;
        binding.descriptorType = impl::rhi_vulkan(item.type);
        binding.descriptorCount = 1;
        binding.stageFlags = impl::rhi_vulkan(item.visibility);
    }

    VkResult result = VK_SUCCESS;
    // CREATE VULKAN POOL
    if (result == VK_SUCCESS) {
        VkDescriptorPoolSize sizes[COUNT_OF_DESCRIPTOR_TYPE];
        uint32_t countOfSize = 0;

        // Populate sizes array with non-zero descriptor type counts
        for (uint32_t i = 0; i != COUNT_OF_DESCRIPTOR_TYPE; ++i) {
            if (counter[i] > 0) {
                sizes[countOfSize].type = impl::rhi_vulkan(static_cast<DESCRIPTOR_TYPE>(i));
                sizes[countOfSize].descriptorCount = counter[i];
                countOfSize++;
            }
        }

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = countOfSize;
        poolInfo.pPoolSizes = sizes;
        assert(m_hPool == VK_NULL_HANDLE);
        result = ::vkCreateDescriptorPool(m_hDevice, &poolInfo, &gc_sAllocationCallbackDRHI, &m_hPool);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateDescriptorPool(Fixed DescriptorBuffer) failed: %u", result);
        }
    }

    // CREATE VULKAN SET LAYOUT 
    if (result == VK_SUCCESS) {
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        assert(m_hSetLayout == VK_NULL_HANDLE);
        result = ::vkCreateDescriptorSetLayout(m_hDevice, &layoutInfo, &gc_sAllocationCallbackDRHI, &m_hSetLayout);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateDescriptorSetLayout(Fixed DescriptorBuffer) failed: %u", result);
        }
    }

    // CREATE VULKAN SET
    if (result == VK_SUCCESS) {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_hPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_hSetLayout;
        assert(m_hSet == VK_NULL_HANDLE);
        result = ::vkAllocateDescriptorSets(m_hDevice, &allocInfo, &m_hSet);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkAllocateDescriptorSets(Fixed DescriptorBuffer) failed: %u", result);
        }
    }

    return impl::vulkan_rhi(result);
}

// ----------------------------------------------------------------------------
//                                 CVulkanDevice
// ----------------------------------------------------------------------------


RHI::CODE RHI::CVulkanDevice::CreateDescriptorBuffer(const DescriptorBufferDesc* pDesc, IRHIDescriptorBuffer** ppBuffer) noexcept
{
    if (!pDesc || !ppBuffer)
        return CODE_POINTER;
    if (!pDesc->slotCount)
        return CODE_INVALIDARG;

    const auto obj = new (std::nothrow) CVulkanDescriptorBuffer{ this };
    if (!obj)
        return CODE_OUTOFMEMORY;

    const auto code = obj->Init(*pDesc);

    if (Failure(code)) {
        obj->Dispose();
        *ppBuffer = nullptr;
    }
    else {
        *ppBuffer = obj;
    }
    return code;
}

#if 0
RHI::CODE RHI::CVulkanDevice::CreateDescriptorBuffer(const DescriptorBufferDesc* pDesc, IRHIDescriptorBuffer** ppBuffer) noexcept
{
    if (!pDesc || !ppBuffer)
        return CODE_POINTER;

    VkResult result = VK_SUCCESS;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    // CREATE VULKAN POOL
    if (result == VK_SUCCESS) {
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16  }
        };
        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
        poolInfo.pPoolSizes = sizes;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        result = ::vkCreateDescriptorPool(m_hDevice, &poolInfo, &gc_sAllocationCallbackDRHI, &pool);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateDescriptorPool(DescriptorBuffer) failed: %u", result);
        }
    }
    // CREATE VULKAN SET LAYOUT 
    if (result == VK_SUCCESS) {
        VkDescriptorSetLayoutBinding bindings[1] = {};
        // Binding 0: First texture
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = 2;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = bindings;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;


        VkDescriptorBindingFlags bindingFlags[] = {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{};
        extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        extendedInfo.bindingCount = sizeof(bindingFlags) / sizeof(bindingFlags[0]);
        //extendedInfo.bindingCount = 1;
        extendedInfo.pBindingFlags = bindingFlags;
        layoutInfo.pNext = &extendedInfo; 

        result = ::vkCreateDescriptorSetLayout(m_hDevice, &layoutInfo, &gc_sAllocationCallbackDRHI, &setLayout);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateDescriptorSetLayout failed: %u", result);
        }
    }
    // CREATE VULKAN SET
    if (result == VK_SUCCESS) {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &setLayout;

        result = ::vkAllocateDescriptorSets(m_hDevice, &allocInfo, &descriptorSet);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkAllocateDescriptorSets failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        DescriptorBufferDescVulkan descVulkan{};
        static_cast<DescriptorBufferDesc&>(descVulkan) = *pDesc;
        descVulkan.pool = pool;
        descVulkan.setLayout = setLayout;
        descVulkan.set = descriptorSet;

        CVulkanDescriptorBuffer* pBuffer = new(std::nothrow) CVulkanDescriptorBuffer{ this, descVulkan };
        *ppBuffer = nullptr;
        if (pBuffer) {
            *ppBuffer = pBuffer;
            pool = VK_NULL_HANDLE; // obj own it
            setLayout = VK_NULL_HANDLE; // obj own it
            //descriptorSet = VK_NULL_HANDLE; // obj own it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // CLEAN UP
    // CVulkanDescriptorBuffer
    // if (descriptorSet != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
    //     ::vkFreeDescriptorSets(m_hDevice, pool, 1, &descriptorSet);
    //     descriptorSet = VK_NULL_HANDLE;
    // }
    if (setLayout != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorSetLayout(m_hDevice, setLayout, &gc_sAllocationCallbackDRHI);
        setLayout = VK_NULL_HANDLE;
    }
    if (pool != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorPool(m_hDevice, pool, &gc_sAllocationCallbackDRHI);
        pool = VK_NULL_HANDLE;
    }
    return impl::vulkan_rhi(result);
}
#endif

#endif

