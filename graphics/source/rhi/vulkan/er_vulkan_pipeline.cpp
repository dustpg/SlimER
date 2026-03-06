#include "er_backend_vulkan.h"
#ifndef SLIM_RHI_NO_VULKAN
#include <cassert>
#include <cstdio>
#include "er_vulkan_pipeline.h"
#include "er_vulkan_device.h"
#include "er_vulkan_descriptor.h"
#include "er_rhi_vulkan.h"

using namespace Slim;

// ----------------------------------------------------------------------------
//                              CVulkanShader
// ----------------------------------------------------------------------------

RHI::CVulkanShader::CVulkanShader(CVulkanDevice* pDevice, 
    VkShaderModule shaderModule) noexcept
    : m_pThisDevice(pDevice)
    //, m_hDevice(pDevice->GetHandle())
    , m_hShaderModule(shaderModule)
{
    assert(pDevice && shaderModule);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
    
}

RHI::CVulkanShader::~CVulkanShader() noexcept
{
    if (m_hShaderModule != VK_NULL_HANDLE) {
        ::vkDestroyShaderModule(m_hDevice, m_hShaderModule, &gc_sAllocationCallbackDRHI);
        m_hShaderModule = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

RHI::CODE RHI::CVulkanShader::SetEntry(const char* entry) noexcept
{
    const auto len = std::strlen(entry);
    if (len > MAX_SHADER_ENTRY_LENGTH)
        return CODE_INVALIDARG;
    std::memcpy(m_szEntry, entry, len + 1);
    return CODE_OK;
}


// ----------------------------------------------------------------------------
//                             CVulkanPipeline
// ----------------------------------------------------------------------------

RHI::CVulkanPipeline::CVulkanPipeline(CVulkanPipelineLayout* pLayout,
    VkPipeline pipeline) noexcept
    : m_pThisLayout(pLayout)
    , m_hPipeline(pipeline)
{
    assert(pLayout && pipeline);
    m_pThisLayout->AddRefCnt();
    
}

RHI::CVulkanPipeline::~CVulkanPipeline() noexcept
{
    if (m_hPipeline != VK_NULL_HANDLE) {
        const auto device = m_pThisLayout->RefDevice().GetHandle();
        ::vkDestroyPipeline(device, m_hPipeline, &gc_sAllocationCallbackDRHI);
        m_hPipeline = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisLayout);
}



// ----------------------------------------------------------------------------
//                           CVulkanPipelineLayout
// ----------------------------------------------------------------------------

RHI::CVulkanPipelineLayout::CVulkanPipelineLayout(CVulkanDevice* pDevice
    , const PipelineLayoutDescVulkan& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_hPushDescriptor(desc.pushDesc)
    , m_hPipelineLayout(desc.layout)
    , m_hStaticSamplerSet(desc.staticSamplerSet)
    , m_vStaticSamplers(std::move(*desc.staticSamplers))
{
    assert(pDevice && m_hPipelineLayout && desc.staticSamplers);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
    // write blidings
    const auto count = desc.pushDescriptorCount;
    const auto pushes = desc.pushDescriptors;
    for (uint32_t i = 0; i != count; ++i) {
        m_szBinding[i] = static_cast<uint16_t>(pushes[i].baseBinding);
    }
    // sets id
    impl::calculate_setid(desc, m_uStaticSamplerSetId, m_uDescriptorBufferSetId);
}

RHI::CVulkanPipelineLayout::~CVulkanPipelineLayout() noexcept
{
    for (auto item : m_vStaticSamplers) {
        ::vkDestroySampler(m_hDevice, item, &gc_sAllocationCallbackDRHI);
    }
    m_vStaticSamplers.clear();
    m_pThisDevice->DisposeStaticSamplerSet(m_hStaticSamplerSet);
    if (m_hPushDescriptor != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorSetLayout(m_hDevice, m_hPushDescriptor, &gc_sAllocationCallbackDRHI);
        m_hPushDescriptor = VK_NULL_HANDLE;
    }
    if (m_hPipelineLayout != VK_NULL_HANDLE) {
        ::vkDestroyPipelineLayout(m_hDevice, m_hPipelineLayout, &gc_sAllocationCallbackDRHI);
        m_hPipelineLayout = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}

// ----------------------------------------------------------------------------
//                            CVulkanRenderPass
// ----------------------------------------------------------------------------

#if 0
RHI::CVulkanRenderPass::CVulkanRenderPass(CVulkanDevice* pDevice, 
    VkRenderPass renderPass) noexcept
    : m_pThisDevice(pDevice)
    , m_hRenderPass(renderPass)
{
    assert(pDevice && renderPass);
    pDevice->AddRefCnt();
    m_hDevice = pDevice->GetHandle();
    
}

RHI::CVulkanRenderPass::~CVulkanRenderPass() noexcept
{
    if (m_hRenderPass != VK_NULL_HANDLE) {
        ::vkDestroyRenderPass(m_hDevice, m_hRenderPass, &gc_sAllocationCallbackDRHI);
        m_hRenderPass = VK_NULL_HANDLE;
    }
    RHI::SafeDispose(m_pThisDevice);
    m_hDevice = VK_NULL_HANDLE;
}
#endif

// ----------------------------------------------------------------------------
//                               CVulkanDevice
// ----------------------------------------------------------------------------


namespace Slim { namespace RHI {

    VkResult CreatePushDescriptorVulkan(VkDevice device
        , const PipelineLayoutDesc& desc
        , const VkAllocationCallbacks* pAllocationCallbacks
        , VkDescriptorSetLayout* pSetLayout) noexcept
    {
        if (desc.pushDescriptorCount > MAX_PUSH_DESCRIPTOR) {
            assert(!"pushDescriptorCount out of MAX_PUSH_DESCRIPTOR");
            return VK_ERROR_UNKNOWN;
        }
        VkDescriptorSetLayoutBinding bindings[MAX_PUSH_DESCRIPTOR];

        for (uint32_t i = 0; i < desc.pushDescriptorCount; ++i) {
            const auto& push = desc.pushDescriptors[i];
            auto& binding = bindings[i];
            binding = {};
            binding.binding = push.baseBinding;
            binding.descriptorType = impl::rhi_vulkan(push.type);
            binding.descriptorCount = 1;
            binding.stageFlags = impl::rhi_vulkan(push.visibility);
        }

        VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
        descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutCI.flags = 0
            // PUSH_DESCRIPTOR
            | VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
            ;
        descriptorLayoutCI.bindingCount = desc.pushDescriptorCount;
        descriptorLayoutCI.pBindings = bindings;

        return ::vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, pSetLayout);
    }


    VkResult CreateStaticSamplerVulkan(VkDevice device
        , const PipelineLayoutDesc& desc
        , pod::vector_soo< VkSampler, 4>& vector
        , const VkAllocationCallbacks* pAllocationCallbacks
        , VkDescriptorSetLayout* pSetLayout) noexcept
    {
        constexpr uint32_t BASE_STATIC_SAMPLER_COUNT = 32;
        if (desc.staticSamplerCount > BASE_STATIC_SAMPLER_COUNT) {
            // pod::tbuffer;
            DRHI_LOGGER_ERR("staticSamplerCount > BASE_STATIC_SAMPLER_COUNT");
            return VK_ERROR_UNKNOWN;
        }
        assert(desc.staticSamplerCount && desc.staticSamplers);

        VkDescriptorSetLayoutBinding bindings[BASE_STATIC_SAMPLER_COUNT];
        if (!vector.resize(desc.staticSamplerCount))
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        VkResult result = VK_SUCCESS;
        uint32_t samplerCountCurrent = desc.staticSamplerCount;
        const auto samplerCountInput = desc.staticSamplerCount;

        // Create VkSampler for each static sampler
        for (uint32_t i = 0; i < samplerCountInput; ++i) {
            const auto& staticSampler = desc.staticSamplers[i];
            const auto& samplerDesc = staticSampler.sampler;

            VkSamplerCreateInfo samplerCI{};
            impl::rhi_vulkan_cvt(&samplerDesc, &samplerCI);

            result = ::vkCreateSampler(device, &samplerCI, pAllocationCallbacks, &vector[i]);
            if (result != VK_SUCCESS) {
                samplerCountCurrent = i;
                DRHI_LOGGER_ERR("vkCreateSampler failed: %u", result);
                break;
            }
        }


        if (result == VK_SUCCESS) {
            // Create descriptor set layout bindings with immutable samplers
            for (uint32_t i = 0; i < samplerCountInput; ++i) {
                const auto& staticSampler = desc.staticSamplers[i];
                auto& binding = bindings[i];

                binding = {};
                binding.binding = staticSampler.binding;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                binding.descriptorCount = 1;
                binding.stageFlags = impl::rhi_vulkan(staticSampler.visibility);
                binding.pImmutableSamplers = &vector[i];  // Set immutable sampler
            }

            VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
            descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorLayoutCI.flags = 0;
            descriptorLayoutCI.bindingCount = desc.staticSamplerCount;
            descriptorLayoutCI.pBindings = bindings;

            result = ::vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, pAllocationCallbacks, pSetLayout);
            if (result != VK_SUCCESS) {
                DRHI_LOGGER_ERR("vkCreateDescriptorSetLayout failed: %u", result);
            }
        }
        vector.resize(samplerCountCurrent);
        return result;
    }
}}


RHI::CODE RHI::CVulkanDevice::CreatePipelineLayout(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept
{
    if (!pDesc || !ppPipelineLayout)
        return CODE_POINTER;

    const uint32_t descBufferCount = pDesc->descriptorBuffer ? 1 : 0;
    VkPushConstantRange oneRange;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkResult result = VK_SUCCESS;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet staticSamplerSet = VK_NULL_HANDLE;
    pod::vector_soo< VkSampler, 4> ssVector = {};

    VkDescriptorSetLayout pushDescriptor = VK_NULL_HANDLE;
    VkDescriptorSetLayout staticDescriptor = VK_NULL_HANDLE;
    // PUSH + STATIC + BUFFER
    VkDescriptorSetLayout descriptorSetLayout[3] = { };
    uint32_t setLayoutCount = 0;
    uint32_t setLayoutIndex = 0;
    // PUSH CONSTANT
    if (pDesc->pushConstant.size) {
        // TODO: CHECK LIMIT(128)?
        assert(pDesc->pushConstant.size % 4 == 0);

        oneRange = {};
        oneRange.stageFlags = impl::rhi_vulkan(pDesc->pushConstant.visibility);
        oneRange.offset = 0;  // Push constants start at offset 0 in RHI
        oneRange.size = pDesc->pushConstant.size;

        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &oneRange;
    }

    // CHECK PUSH DESCRIPTOR
    if (pDesc->pushDescriptorCount) {
        setLayoutCount++;
        setLayoutIndex++;
        result = RHI::CreatePushDescriptorVulkan(m_hDevice, *pDesc, &gc_sAllocationCallbackDRHI, &pushDescriptor);
        descriptorSetLayout[0] = pushDescriptor;
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("RHI::CreatePushDescriptorVulkan failed: %u", result);
        }
    }
    // CHECK STATIC SAMPLER
    if (result == VK_SUCCESS && pDesc->staticSamplerCount) {
        result = RHI::CreateStaticSamplerVulkan(m_hDevice, *pDesc, ssVector, &gc_sAllocationCallbackDRHI, &staticDescriptor);
        if (pDesc->staticSamplerOnBack) {
            descriptorSetLayout[setLayoutIndex + descBufferCount] = staticDescriptor;
        }
        else {
            descriptorSetLayout[setLayoutIndex] = staticDescriptor;
            setLayoutIndex++;
        }
        setLayoutCount++;
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("RHI::CreatePushDescriptorVulkan failed: %u", result);
        }
    }
    // STATIC SAMPLER SET
    if (result == VK_SUCCESS && pDesc->staticSamplerCount) {
        VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_hStaticSamplerPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &staticDescriptor;

        result = ::vkAllocateDescriptorSets(m_hDevice, &allocInfo, &staticSamplerSet);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkAllocateDescriptorSets(STATIC SET) failed: %u", result);
        }
    }
    // COPY BUFFER SAMPLER SET
    {
        const auto buffer = static_cast<CVulkanDescriptorBuffer*>(pDesc->descriptorBuffer);
        // SIMPLE RTTI
        assert(&buffer->RefDevice() == this);
        descriptorSetLayout[setLayoutIndex] = buffer->GetLayoutHandle();
    }
    // CREATE LAYOUT
    if (result == VK_SUCCESS) {
        setLayoutCount += descBufferCount;
        pipelineLayoutInfo.setLayoutCount = setLayoutCount;
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayout;
        result = ::vkCreatePipelineLayout(m_hDevice, &pipelineLayoutInfo, &gc_sAllocationCallbackDRHI, &pipelineLayout);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreatePipelineLayout failed: %u", result);
        }
    }

    // CREATE RHI OBJECT
    if (result == VK_SUCCESS) {
        PipelineLayoutDescVulkan vdesc = {};
        static_cast<PipelineLayoutDesc&>(vdesc) = *pDesc;
        vdesc.pushDesc = pushDescriptor;
        vdesc.layout = pipelineLayout;
        vdesc.staticSamplerSet = staticSamplerSet;
        vdesc.staticSamplers = &ssVector;
        const auto obj = new(std::nothrow) CVulkanPipelineLayout{ this, vdesc };
        *ppPipelineLayout = obj;
        if (obj) {
            pipelineLayout = VK_NULL_HANDLE; // rhi object own it
            pushDescriptor = VK_NULL_HANDLE; // rhi object own it
            staticSamplerSet = VK_NULL_HANDLE; // rhi object own it
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            DRHI_LOGGER_ERR("Failed to allocate CVulkanPipelineLayout");
        }
    }
    // CLEAN UP
    {
        for (auto item : ssVector) {
            ::vkDestroySampler(m_hDevice, item, &gc_sAllocationCallbackDRHI);
        }
        ssVector.clear();
    }
    if (staticSamplerSet != VK_NULL_HANDLE) {
        ::vkFreeDescriptorSets(
            m_hDevice,
            m_hStaticSamplerPool,
            1,
            &staticSamplerSet
        );
        staticSamplerSet = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        ::vkDestroyPipelineLayout(m_hDevice, pipelineLayout, &gc_sAllocationCallbackDRHI);
        pipelineLayout = VK_NULL_HANDLE;
    }
    
    if (staticDescriptor != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorSetLayout(m_hDevice, staticDescriptor, &gc_sAllocationCallbackDRHI);
        staticDescriptor = VK_NULL_HANDLE;
    }
    if (pushDescriptor != VK_NULL_HANDLE) {
        ::vkDestroyDescriptorSetLayout(m_hDevice, pushDescriptor, &gc_sAllocationCallbackDRHI);
        pushDescriptor = VK_NULL_HANDLE;
    }
    return impl::vulkan_rhi(result);
}


RHI::CODE RHI::CVulkanDevice::CreatePipeline(const PipelineDesc* pDesc, IRHIPipeline** ppPipeline) noexcept
{
    if (!pDesc || !ppPipeline)
        return CODE_POINTER;

    pod::vector<uint8_t> buffer;
    uint32_t bufferSize = 0;

    // CALCULATE BUFFER SIZE

    VkVertexInputBindingDescription* pVkVertexInputBindingDescriptions = nullptr;
    VkVertexInputAttributeDescription* pVkVertexInputAttributeDescriptions = nullptr;
    {
        const uint32_t cVkVertexInputBindingDescription = pDesc->vertexBindingCount;
        const uint32_t cVkVertexInputAttributeDescription = pDesc->vertexAttributeCount;
        
        // Calculate total buffer size needed
        bufferSize = cVkVertexInputBindingDescription * sizeof(VkVertexInputBindingDescription) +
                     cVkVertexInputAttributeDescription * sizeof(VkVertexInputAttributeDescription);
    }

    if (bufferSize) {
        if (!buffer.resize(bufferSize)) {
            return CODE_OUTOFMEMORY;
        }
        // Set up pointers to the arrays within the buffer
        uint8_t* pCurrent = buffer.data();
        if (pDesc->vertexBindingCount > 0) {
            pVkVertexInputBindingDescriptions = reinterpret_cast<VkVertexInputBindingDescription*>(pCurrent);
            pCurrent += pDesc->vertexBindingCount * sizeof(VkVertexInputBindingDescription);
        }
        if (pDesc->vertexAttributeCount > 0) {
            pVkVertexInputAttributeDescriptions = reinterpret_cast<VkVertexInputAttributeDescription*>(pCurrent);
        }
        
        // Convert and fill VkVertexInputBindingDescription array
        if (pDesc->vertexBinding && pDesc->vertexBindingCount > 0) {
            const auto vertexBindingCount = pDesc->vertexBindingCount;
            for (uint32_t i = 0; i < vertexBindingCount; ++i) {
                const auto& rhiBinding = pDesc->vertexBinding[i];
                auto& vkBinding = pVkVertexInputBindingDescriptions[i];
                vkBinding.binding = rhiBinding.binding;
                vkBinding.stride = rhiBinding.stride;
                vkBinding.inputRate = impl::rhi_vulkan(rhiBinding.inputRate);
            }
        }


        // Convert and fill VkVertexInputAttributeDescription array
        if (pDesc->vertexAttribute && pDesc->vertexAttributeCount > 0) {
            const auto vertexAttributeCount = pDesc->vertexAttributeCount;
            for (uint32_t i = 0; i < vertexAttributeCount; ++i) {
                const auto& rhiAttr = pDesc->vertexAttribute[i];
                auto& vkAttr = pVkVertexInputAttributeDescriptions[i];

                vkAttr.location = rhiAttr.location;
                vkAttr.binding = rhiAttr.binding;
                vkAttr.format = impl::rhi_vulkan(rhiAttr.format);
                vkAttr.offset = rhiAttr.offset;

            }
        }
    }

    VkResult result = VK_SUCCESS;
    VkPipeline hPipeline = VK_NULL_HANDLE; // CREATE FROM HERE
    VkPipelineLayout hPipelineLayout = VK_NULL_HANDLE;  // WEAK-REF
    //VkRenderPass hRenderPass = VK_NULL_HANDLE;   // WEAK-REF

    VkPipelineShaderStageCreateInfo szShaderStages[5];
    VkPipelineShaderStageCreateInfo* pVsShaderStage = nullptr;
    VkPipelineShaderStageCreateInfo* pPsShaderStage = nullptr;
    VkPipelineShaderStageCreateInfo* pDsShaderStage = nullptr;
    VkPipelineShaderStageCreateInfo* pHsShaderStage = nullptr;
    VkPipelineShaderStageCreateInfo* pGsShaderStage = nullptr;

    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineMultisampleStateCreateInfo multisampling { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo viewportState { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rasterization{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendStateCreateInfo colorBlend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    constexpr uint32_t COLOR_BLEND_ATTACHMENT_COUNT = sizeof(BlendDesc::attachments) / sizeof(BlendDesc::attachments[0]);
    VkPipelineColorBlendAttachmentState colorBlendAttachments[COLOR_BLEND_ATTACHMENT_COUNT];
    uint32_t shaderIndex = 0;

    // Setup vertex input state
    {
        vertexInputInfo.vertexBindingDescriptionCount = pDesc->vertexBindingCount;
        vertexInputInfo.pVertexBindingDescriptions = pVkVertexInputBindingDescriptions;
        vertexInputInfo.vertexAttributeDescriptionCount = pDesc->vertexAttributeCount;
        vertexInputInfo.pVertexAttributeDescriptions = pVkVertexInputAttributeDescriptions;
    }

    // VkPipelineInputAssemblyStateCreateInfo
    {
        inputAssembly.topology = impl::rhi_vulkan(pDesc->topology);
        inputAssembly.primitiveRestartEnable = (pDesc->primitiveRestart != RHI::PRIMITIVE_RESTART_VALUE_DISABLED) ? VK_TRUE : VK_FALSE;
    }

    // VkPipelineMultisampleStateCreateInfo
    {
        assert(pDesc->sampleCount.count);
        multisampling.rasterizationSamples = impl::rhi_vulkan(pDesc->sampleCount);
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 0.0f;
        multisampling.pSampleMask = (pDesc->sampleMask != 0) ? &pDesc->sampleMask : nullptr;
        multisampling.alphaToCoverageEnable = pDesc->blend.alphaToCoverageEnable ? VK_TRUE : VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;
    }

    // VkPipelineDynamicStateCreateInfo
    {
        dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
        dynamicState.pDynamicStates = dynamicStates;
    }

    // VkPipelineViewportStateCreateInfo
    {
        // When using dynamic viewport/scissor, pViewports and pScissors must be nullptr
        // but viewportCount and scissorCount must still be set
        viewportState.viewportCount = pDesc->maxViewportCount;
        viewportState.pViewports = nullptr; // Dynamic state
        viewportState.scissorCount = pDesc->maxScissorCount;
        viewportState.pScissors = nullptr; // Dynamic state
    }

    // VkPipelineRasterizationStateCreateInfo
    {
        const auto& rhiRaster = pDesc->rasterization;
        rasterization.depthClampEnable = rhiRaster.depthClampEnable ? VK_TRUE : VK_FALSE;
        rasterization.rasterizerDiscardEnable = rhiRaster.rasterizerDiscardEnable ? VK_TRUE : VK_FALSE;
        rasterization.polygonMode = impl::rhi_vulkan(rhiRaster.polygonMode);
        rasterization.lineWidth = 1.0f; // Fixed line width as per Vulkan spec requirement for non-wide lines
        rasterization.cullMode = impl::rhi_vulkan(rhiRaster.cullMode);
        rasterization.frontFace = impl::rhi_vulkan(rhiRaster.frontFace);
        rasterization.depthBiasEnable = rhiRaster.depthBiasEnable ? VK_TRUE : VK_FALSE;
        rasterization.depthBiasConstantFactor = rhiRaster.depthBiasConstantFactor;
        rasterization.depthBiasClamp = rhiRaster.depthBiasClamp;
        rasterization.depthBiasSlopeFactor = rhiRaster.depthBiasSlopeFactor;
    }

    // VkPipelineDepthStencilStateCreateInfo
    {
        const auto& rhiDepthStencil = pDesc->depthStencil;
        
        // Depth testing
        depthStencil.depthTestEnable = rhiDepthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = rhiDepthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = impl::rhi_vulkan(rhiDepthStencil.depthCompareOp);
        depthStencil.depthBoundsTestEnable = rhiDepthStencil.depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.minDepthBounds = rhiDepthStencil.minDepthBounds;
        depthStencil.maxDepthBounds = rhiDepthStencil.maxDepthBounds;
        
        // Stencil testing
        depthStencil.stencilTestEnable = rhiDepthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;
        
        // Front face stencil operations
        const auto& rhiFront = rhiDepthStencil.front;
        depthStencil.front.failOp = impl::rhi_vulkan(rhiFront.failOp);
        depthStencil.front.passOp = impl::rhi_vulkan(rhiFront.passOp);
        depthStencil.front.depthFailOp = impl::rhi_vulkan(rhiFront.depthFailOp);
        depthStencil.front.compareOp = impl::rhi_vulkan(rhiFront.compareOp);
        depthStencil.front.compareMask = rhiDepthStencil.stencilReadMask;  // Use top-level mask (D3D12 compatible)
        depthStencil.front.writeMask = rhiDepthStencil.stencilWriteMask;    // Use top-level mask (D3D12 compatible)
        depthStencil.front.reference = rhiFront.reference;
        
        // Back face stencil operations
        const auto& rhiBack = rhiDepthStencil.back;
        depthStencil.back.failOp = impl::rhi_vulkan(rhiBack.failOp);
        depthStencil.back.passOp = impl::rhi_vulkan(rhiBack.passOp);
        depthStencil.back.depthFailOp = impl::rhi_vulkan(rhiBack.depthFailOp);
        depthStencil.back.compareOp = impl::rhi_vulkan(rhiBack.compareOp);
        depthStencil.back.compareMask = rhiDepthStencil.stencilReadMask;    // Use top-level mask (D3D12 compatible)
        depthStencil.back.writeMask = rhiDepthStencil.stencilWriteMask;   // Use top-level mask (D3D12 compatible)
        depthStencil.back.reference = rhiBack.reference;
    }

    // VkPipelineColorBlendStateCreateInfo
    {
        const auto& rhiBlend = pDesc->blend;
        
        // Fill per-attachment blend states
        const uint32_t attachmentCount = rhiBlend.attachmentCount;
        for (uint32_t i = 0; i < attachmentCount && i < COLOR_BLEND_ATTACHMENT_COUNT; ++i) {
            const auto& rhiAttachment = rhiBlend.attachments[i];
            auto& vkAttachment = colorBlendAttachments[i];
            
            vkAttachment.blendEnable = rhiAttachment.blendEnable ? VK_TRUE : VK_FALSE;
            vkAttachment.srcColorBlendFactor = impl::rhi_vulkan(rhiAttachment.srcColorBlendFactor);
            vkAttachment.dstColorBlendFactor = impl::rhi_vulkan(rhiAttachment.dstColorBlendFactor);
            vkAttachment.colorBlendOp = impl::rhi_vulkan(rhiAttachment.colorBlendOp);
            vkAttachment.srcAlphaBlendFactor = impl::rhi_vulkan(rhiAttachment.srcAlphaBlendFactor);
            vkAttachment.dstAlphaBlendFactor = impl::rhi_vulkan(rhiAttachment.dstAlphaBlendFactor);
            vkAttachment.alphaBlendOp = impl::rhi_vulkan(rhiAttachment.alphaBlendOp);
            vkAttachment.colorWriteMask = impl::rhi_vulkan(rhiAttachment.colorWriteMask);
        }
        
        // Fill global blend state
        colorBlend.logicOpEnable = rhiBlend.logicOpEnable ? VK_TRUE : VK_FALSE;
        colorBlend.logicOp = impl::rhi_vulkan(rhiBlend.logicOp);
        colorBlend.attachmentCount = attachmentCount;
        colorBlend.pAttachments = (attachmentCount > 0) ? colorBlendAttachments : nullptr;
        colorBlend.blendConstants[0] = rhiBlend.blendConstants[0];
        colorBlend.blendConstants[1] = rhiBlend.blendConstants[1];
        colorBlend.blendConstants[2] = rhiBlend.blendConstants[2];
        colorBlend.blendConstants[3] = rhiBlend.blendConstants[3];
    }

    // VALIDATE SHADERS
    if (result == VK_SUCCESS) {

        CVulkanShader* pVulkanVS = nullptr;
        CVulkanShader* pVulkanPS = nullptr;
        CVulkanShader* pVulkanDS = nullptr;
        CVulkanShader* pVulkanHS = nullptr;
        CVulkanShader* pVulkanGS = nullptr;

        // TODO: Validate shaders (at least VS and PS should be present)
        // VK_ERROR_INITIALIZATION_FAILED

        if (pDesc->vs) {
            pVulkanVS = static_cast<CVulkanShader*>(pDesc->vs);
            // SIMPLE RTTI
            assert(&pVulkanVS->RefDevice() == this);
            pVsShaderStage = szShaderStages + shaderIndex; ++shaderIndex;
            *pVsShaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            pVsShaderStage->stage = VK_SHADER_STAGE_VERTEX_BIT;
            pVsShaderStage->module = pVulkanVS->GetHandle();
            pVsShaderStage->pName = pVulkanVS->GetEntry();
        }
        if (pDesc->ps) {
            pVulkanPS = static_cast<CVulkanShader*>(pDesc->ps);
            // SIMPLE RTTI
            assert(&pVulkanPS->RefDevice() == this);
            pPsShaderStage = szShaderStages + shaderIndex; ++shaderIndex;
            *pPsShaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            pPsShaderStage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            pPsShaderStage->module = pVulkanPS->GetHandle();
            pPsShaderStage->pName = pVulkanPS->GetEntry();
        }
        if (pDesc->ds) {
            pVulkanDS = static_cast<CVulkanShader*>(pDesc->ds);
            // SIMPLE RTTI
            assert(&pVulkanDS->RefDevice() == this);
            pDsShaderStage = szShaderStages + shaderIndex; ++shaderIndex;
            *pDsShaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            pDsShaderStage->stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            pDsShaderStage->module = pVulkanDS->GetHandle();
            pDsShaderStage->pName = pVulkanDS->GetEntry();
        }
        if (pDesc->hs) {
            pVulkanHS = static_cast<CVulkanShader*>(pDesc->hs);
            // SIMPLE RTTI
            assert(&pVulkanHS->RefDevice() == this);
            pHsShaderStage = szShaderStages + shaderIndex; ++shaderIndex;
            *pHsShaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            pHsShaderStage->stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            pHsShaderStage->module = pVulkanHS->GetHandle();
            pHsShaderStage->pName = pVulkanHS->GetEntry();
        }
        if (pDesc->gs) {
            pVulkanGS = static_cast<CVulkanShader*>(pDesc->gs);
            // SIMPLE RTTI
            assert(&pVulkanGS->RefDevice() == this);
            pGsShaderStage = szShaderStages + shaderIndex; ++shaderIndex;
            *pGsShaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            pGsShaderStage->stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            pGsShaderStage->module = pVulkanGS->GetHandle();
            pGsShaderStage->pName = pVulkanGS->GetEntry();
        }
    }

    // CHECK PIPELINE LAYOUT
    if (result == VK_SUCCESS) {
        const auto layout = static_cast<CVulkanPipelineLayout*>(pDesc->layout);
        // SIMPLE RTTI
        assert(this == &layout->RefDevice());
        hPipelineLayout = layout->GetHandle();
    }
#if 0
    // CHECK RENDER PASS
    if (result == VK_SUCCESS) {
        const auto pass = static_cast<CVulkanRenderPass*>(pDesc->renderPass);
        // SIMPLE RTTI
        assert(this == &pass->RefDevice());
        hRenderPass = pass->GetHandle();
    }
#endif
    // CREATE GRAPHICS PIPELINE
    if (result == VK_SUCCESS) {

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.stageCount = shaderIndex;
        pipelineInfo.pStages = szShaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;

        // 3.3.1. Object Lifetime
        // A VkRenderPass or VkPipelineLayout object passed as a parameter to create another object 
        // is not further accessed by that object after the duration of the command it is passed into.
        pipelineInfo.layout = hPipelineLayout;

        // vulkan1.3 dyanamic rendering 
        // TODO: Fallback if not supported?
#if 0
        pipelineInfo.renderPass = hRenderPass;
#else
        VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        const auto& basicPass = pDesc->basicPass;
        constexpr uint32_t MAC_COLOR_ACOUNT = sizeof(basicPass.colorFormats) / sizeof(basicPass.colorFormats[0]);
        VkFormat colorFormats[MAC_COLOR_ACOUNT];
        
        // Convert color attachment formats
        const uint32_t colorFormatCount = pDesc->basicPass.colorFormatCount;
        assert(colorFormatCount <= MAC_COLOR_ACOUNT);
        for (uint32_t i = 0; i < colorFormatCount; ++i) {
            colorFormats[i] = impl::rhi_vulkan(pDesc->basicPass.colorFormats[i]);
        }
        // Set color attachment formats
        renderingInfo.colorAttachmentCount = colorFormatCount;
        renderingInfo.pColorAttachmentFormats = (colorFormatCount > 0) ? colorFormats : nullptr;
        
        // Convert depth/stencil attachment format
        const auto depthStencilFormat = pDesc->basicPass.depthStencil;
        if (depthStencilFormat != RHI::RHI_FORMAT_UNKNOWN) {
            const VkFormat vkDepthStencilFormat = impl::rhi_vulkan(depthStencilFormat);
            // For combined depth/stencil formats, stencil format is the same
            renderingInfo.depthAttachmentFormat = vkDepthStencilFormat;
            if (pDesc->depthStencil.stencilTestEnable)
                renderingInfo.stencilAttachmentFormat = vkDepthStencilFormat;
        } 
        
        pipelineInfo.pNext = &renderingInfo;
#endif

        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        result = ::vkCreateGraphicsPipelines(m_hDevice, VK_NULL_HANDLE, 1, &pipelineInfo, &gc_sAllocationCallbackDRHI, &hPipeline);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateGraphicsPipelines failed: %u", result);
        }
    }

    // CREATE RHI PIPELINE OBJECT
    if (result == VK_SUCCESS) {
        const auto layout = static_cast<CVulkanPipelineLayout*>(pDesc->layout);
        const auto obj = new(std::nothrow) CVulkanPipeline{ layout, hPipeline };
        *ppPipeline = obj;
        if (obj) {
            hPipeline = VK_NULL_HANDLE;
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            DRHI_LOGGER_ERR("Failed to allocate CVulkanPipeline");
        }
    }

    // CLEAN UP
    if (hPipeline != VK_NULL_HANDLE) {
        ::vkDestroyPipeline(m_hDevice, hPipeline, &gc_sAllocationCallbackDRHI);
        hPipeline = VK_NULL_HANDLE;
    }
    return impl::vulkan_rhi(result);
}

#if 0
RHI::CODE RHI::CVulkanDevice::CreateRenderPass(const RenderPassDesc* pDesc, IRHIRenderPass** ppRenderPass) noexcept
{
    if (!pDesc || !ppRenderPass)
        return CODE_POINTER;

    constexpr uint32_t COLOR_PASS_COUNT = sizeof(pDesc->colorAttachment) / sizeof(pDesc->colorAttachment[0]);

    const auto colorPassCount = pDesc->colorAttachmentCount;
    if (colorPassCount > COLOR_PASS_COUNT)
        return CODE_INVALIDARG;

    VkAttachmentDescription attachments[COLOR_PASS_COUNT + 1];
    uint32_t attachmentCount = 0;
    VkAttachmentReference colorReferences[COLOR_PASS_COUNT];
    VkAttachmentReference depthStencilRef{};
    VkAttachmentReference* pDepthStencilReference = nullptr;
    VkSubpassDescription subpass{};
    VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    VkResult result = VK_SUCCESS;
    VkRenderPass hRenderPass = VK_NULL_HANDLE;
    VkSubpassDependency dependency{};

    // FILL COLOR ATTACHMENTS
    for (uint32_t i = 0; i < colorPassCount; ++i) {
        const auto& colorPass = pDesc->colorAttachment[i];
        auto& attachment = attachments[attachmentCount];
        
        attachment.flags = 0;
        attachment.format = impl::rhi_vulkan(colorPass.format);
        attachment.samples = impl::rhi_vulkan(pDesc->sampleCount);
        attachment.loadOp = impl::rhi_vulkan(colorPass.load);
        attachment.storeOp = impl::rhi_vulkan(colorPass.store);
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // TODO: ImageLayout -> RenderPassDesc ?
        //attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


        colorReferences[i].attachment = attachmentCount;
        colorReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++attachmentCount;
    }

    // FILL DEPTH STENCIL ATTACHMENT
    const bool hasDepthStencil = pDesc->depthStencil.format != RHI::RHI_FORMAT_UNKNOWN;
    if (hasDepthStencil) {
        auto& attachment = attachments[attachmentCount];
        
        attachment.flags = 0;
        attachment.format = impl::rhi_vulkan(pDesc->depthStencil.format);
        attachment.samples = impl::rhi_vulkan(pDesc->sampleCount);
        attachment.loadOp = impl::rhi_vulkan(pDesc->depthStencil.depthLoad);
        attachment.storeOp = impl::rhi_vulkan(pDesc->depthStencil.depthStore);
        attachment.stencilLoadOp = impl::rhi_vulkan(pDesc->depthStencil.stencilLoad);
        attachment.stencilStoreOp = impl::rhi_vulkan(pDesc->depthStencil.stencilStore);
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthStencilRef.attachment = attachmentCount;
        depthStencilRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        pDepthStencilReference = &depthStencilRef;
        ++attachmentCount;
    }

    // FILL SUBPASS DESCRIPTION
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0; // no ex-subpass
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = colorPassCount;
    subpass.pColorAttachments = colorPassCount ? colorReferences : nullptr;
    subpass.pResolveAttachments = nullptr; // TODO: MSAA Resolve
    subpass.pDepthStencilAttachment = pDepthStencilReference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    // FILL RENDER PASS CREATE INFO
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.pAttachments = attachmentCount > 0 ? attachments : nullptr;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    //renderPassInfo.dependencyCount = 0;
    //renderPassInfo.pDependencies = nullptr;

    // ADD EXTERNAL SUBPASS DEPENDENCY (default for single subpass)
    // This ensures proper synchronization with operations outside the render pass
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT /* | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT*/;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT /* | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT*/;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    // CREATE VULKAN RENDER PASS OBJECT
    if (result == VK_SUCCESS) {
        result = ::vkCreateRenderPass(m_hDevice, &renderPassInfo, &gc_sAllocationCallbackDRHI, &hRenderPass);
        if (result != VK_SUCCESS) {
            DRHI_LOGGER_ERR("vkCreateRenderPass failed: %u", result);
        }
    }

    // CREATE RHI RENDER PASS OBJECT
    if (result == VK_SUCCESS) {
        const auto pRenderPass = new (std::nothrow) CVulkanRenderPass{ this, hRenderPass };
        *ppRenderPass = pRenderPass;
        if (pRenderPass) {
            hRenderPass = VK_NULL_HANDLE;
        }
        else {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            DRHI_LOGGER_ERR("Failed to allocate CVulkanRenderPass");
        }
    }

    // CLEAN UP
    if (hRenderPass != VK_NULL_HANDLE) {
        ::vkDestroyRenderPass(m_hDevice, hRenderPass, &gc_sAllocationCallbackDRHI);
        hRenderPass = VK_NULL_HANDLE;
    }

    return impl::vulkan_rhi(result);
}
#endif


#endif


