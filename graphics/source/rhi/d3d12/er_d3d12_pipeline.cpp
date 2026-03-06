#include "er_backend_d3d12.h"
#ifndef SLIM_RHI_NO_D3D12
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "er_d3d12_pipeline.h"
#include "er_d3d12_device.h"
#include "er_d3d12_descriptor.h"
#include "er_rhi_d3d12.h"
#include <d3d12.h>

using namespace Slim;

RHI::CD3D12Shader::CD3D12Shader(CD3D12Device* pDevice, const void* pData, size_t size) noexcept
    : m_pThisDevice(pDevice)
    , m_pCodeData(imalloc(size))
{
    assert(pDevice && pData && size);
    pDevice->AddRefCnt();
    
    if (m_pCodeData) {
        m_uCodeLength = size;
        std::memcpy(m_pCodeData, pData, size);
    }
}

RHI::CD3D12Shader::~CD3D12Shader() noexcept
{
    m_uCodeLength = 0;
    if (m_pCodeData) {
        this->ifree(m_pCodeData);
        m_pCodeData = nullptr;
    }
    RHI::SafeDispose(m_pThisDevice);
}



// ----------------------------------------------------------------------------
//                           CD3D12PipelineLayout
// ----------------------------------------------------------------------------

RHI::CD3D12PipelineLayout::CD3D12PipelineLayout(CD3D12Device* pDevice
    , const PipelineLayoutDesc& desc
    , ID3D12RootSignature* pRootSignature) noexcept
    : m_pThisDevice(pDevice)
    , m_pRootSignature(pRootSignature)
{
    assert(pDevice && pRootSignature);
    pDevice->AddRefCnt();
    pRootSignature->AddRef();
    // space0: [PUSH CONST] [PUSH DESC]
    m_uPushDescriptorBase = desc.pushConstant.size ? 1 : 0;
    // root: [PUSH] [DESC] [BUFFER]
    if (desc.pushConstant.size) m_uDescriptorBufferBase++;
    if (desc.pushDescriptorCount) m_uDescriptorBufferBase++;
    
}

RHI::CD3D12PipelineLayout::~CD3D12PipelineLayout() noexcept
{
    RHI::SafeRelease(m_pRootSignature);
    RHI::SafeDispose(m_pThisDevice);
}

// ----------------------------------------------------------------------------
//                               CD3D12Device
// ----------------------------------------------------------------------------


namespace Slim { namespace impl {

    auto cached_find_bind(const RHI::VertexBindingDesc* cached
        , const RHI::VertexBindingDesc* begin
        , const RHI::VertexBindingDesc* end
        , uint32_t binding
        ) noexcept -> const RHI::VertexBindingDesc* {

        if (cached && cached->binding == binding)
            return cached;

        const auto itr = std::find_if(begin, end, [binding](const RHI::VertexBindingDesc& vb) noexcept {
            return vb.binding == binding;
        });

        if (itr != end)
            return itr;

        return nullptr;
    }

}}


RHI::CODE RHI::CD3D12Device::CreatePipeline(const PipelineDesc* pDesc, IRHIPipeline** ppPipeline) noexcept
{
    if (!pDesc || !ppPipeline)
        return CODE_POINTER;

    if (!pDesc->layout)
        return CODE_INVALIDARG;

    // TODO: state check
    //if (!pDesc->vs || !pDesc->ps)
    //    return CODE_INVALIDARG;

    assert(m_pDevice);
    HRESULT hr = S_OK;
    ID3D12PipelineState* pPipelineState = nullptr;

    // Get RootSignature from PipelineLayout
    const auto pPipelineLayout = static_cast<CD3D12PipelineLayout*>(pDesc->layout);
    assert(this == &pPipelineLayout->RefDevice());
    ID3D12RootSignature& refRootSignature = pPipelineLayout->RefRootSignature();

    // Get BasicRenderPass description
    const auto& basicPass = pDesc->basicPass;


    pod::vector<uint8_t> buffer;
    // BUFFER = D3D12_INPUT_ELEMENT_DESC[align=void*] x N
    uint32_t bufferSize = 0;
    // CALCULATE BUFFER SIZE
    if (pDesc->vertexAttributeCount > 0) {
        bufferSize = pDesc->vertexAttributeCount * sizeof(D3D12_INPUT_ELEMENT_DESC);
    }

    if (!buffer.resize(bufferSize))
        return CODE_OUTOFMEMORY;

    auto bufptr = buffer.data();

    D3D12_INPUT_ELEMENT_DESC* pInputElements = nullptr;
    UINT inputElementCount = 0;

    // CONVERT VERTEX INPUT LAYOUT
    if (pDesc->vertexAttributeCount > 0) {
        assert(pDesc->vertexAttribute);

        uint32_t semanticCounter[COUNT_OF_VERTEX_ATTRIBUTE_SEMANTIC] = {};

        pInputElements = reinterpret_cast<D3D12_INPUT_ELEMENT_DESC*>(bufptr);
        inputElementCount = pDesc->vertexAttributeCount;
        bufptr += inputElementCount * sizeof(D3D12_INPUT_ELEMENT_DESC);
        const VertexBindingDesc* cached = nullptr;
        
        for (uint32_t i = 0; i < inputElementCount; ++i) {
            const auto& rhiAttr = pDesc->vertexAttribute[i];
            auto& d3d12Element = pInputElements[i];

            // Find the corresponding vertex binding to get input rate
            D3D12_INPUT_CLASSIFICATION inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            UINT instanceDataStepRate = 0;


            cached = impl::cached_find_bind(
                cached, pDesc->vertexBinding,
                pDesc->vertexBinding + pDesc->vertexBindingCount,
                rhiAttr.binding
            );
            if (cached) {
                if (cached->inputRate == RHI::VERTEX_INPUT_RATE_INSTANCE) {
                    inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                    instanceDataStepRate = 1;
                }
            }

            d3d12Element.SemanticName = impl::rhi_d3d12_semantic_name(rhiAttr.semantic);
            d3d12Element.SemanticIndex = semanticCounter[rhiAttr.semantic];
            ++semanticCounter[rhiAttr.semantic];
            d3d12Element.Format = impl::rhi_d3d12(rhiAttr.format);
            d3d12Element.InputSlot = rhiAttr.binding;
            d3d12Element.AlignedByteOffset = rhiAttr.offset;
            d3d12Element.InputSlotClass = inputSlotClass;
            d3d12Element.InstanceDataStepRate = instanceDataStepRate;
        }
    }

    // Setup shader stages
    D3D12_SHADER_BYTECODE vsBytecode = {};
    D3D12_SHADER_BYTECODE psBytecode = {};
    D3D12_SHADER_BYTECODE dsBytecode = {};
    D3D12_SHADER_BYTECODE hsBytecode = {};
    D3D12_SHADER_BYTECODE gsBytecode = {};
    // TODO: Validate shaders (at least VS and PS should be present)

    if (pDesc->vs) {
        const auto pVSShader = static_cast<CD3D12Shader*>(pDesc->vs);
        assert(this == &pVSShader->RefDevice());
        vsBytecode.pShaderBytecode = pVSShader->GetBytecodeData();
        vsBytecode.BytecodeLength = pVSShader->GetBytecodeSize();
    }

    if (pDesc->ps) {
        const auto pPSShader = static_cast<CD3D12Shader*>(pDesc->ps);
        assert(this == &pPSShader->RefDevice());
        psBytecode.pShaderBytecode = pPSShader->GetBytecodeData();
        psBytecode.BytecodeLength = pPSShader->GetBytecodeSize();
    }


    if (pDesc->ds) {
        const auto pDSShader = static_cast<CD3D12Shader*>(pDesc->ds);
        assert(this == &pDSShader->RefDevice());
        dsBytecode.pShaderBytecode = pDSShader->GetBytecodeData();
        dsBytecode.BytecodeLength = pDSShader->GetBytecodeSize();
    }

    if (pDesc->hs) {
        const auto pHShader = static_cast<CD3D12Shader*>(pDesc->hs);
        assert(this == &pHShader->RefDevice());
        hsBytecode.pShaderBytecode = pHShader->GetBytecodeData();
        hsBytecode.BytecodeLength = pHShader->GetBytecodeSize();
    }

    if (pDesc->gs) {
        const auto pGSShader = static_cast<CD3D12Shader*>(pDesc->gs);
        assert(this == &pGSShader->RefDevice());
        gsBytecode.pShaderBytecode = pGSShader->GetBytecodeData();
        gsBytecode.BytecodeLength = pGSShader->GetBytecodeSize();
    }

    // Setup rasterizer state
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = impl::rhi_d3d12(pDesc->rasterization.polygonMode);
    rasterizerDesc.CullMode = impl::rhi_d3d12(pDesc->rasterization.cullMode);
    rasterizerDesc.FrontCounterClockwise = impl::rhi_d3d12(pDesc->rasterization.frontFace);
    rasterizerDesc.DepthBias = static_cast<INT>(pDesc->rasterization.depthBiasConstantFactor);
    rasterizerDesc.DepthBiasClamp = pDesc->rasterization.depthBiasClamp;
    rasterizerDesc.SlopeScaledDepthBias = pDesc->rasterization.depthBiasSlopeFactor;
    rasterizerDesc.DepthClipEnable = !pDesc->rasterization.depthClampEnable;
    rasterizerDesc.MultisampleEnable = (pDesc->sampleCount.count > 1);
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Setup depth stencil state
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = pDesc->depthStencil.depthTestEnable ? TRUE : FALSE;
    depthStencilDesc.DepthWriteMask = impl::rhi_d3d12(pDesc->depthStencil.depthWriteEnable ? DEPTH_WRITE_MASK_ALL : DEPTH_WRITE_MASK_ZERO);
    depthStencilDesc.DepthFunc = impl::rhi_d3d12(pDesc->depthStencil.depthCompareOp);
    depthStencilDesc.StencilEnable = pDesc->depthStencil.stencilTestEnable ? TRUE : FALSE;
    depthStencilDesc.StencilReadMask = pDesc->depthStencil.stencilReadMask;
    depthStencilDesc.StencilWriteMask = pDesc->depthStencil.stencilWriteMask;
    depthStencilDesc.FrontFace.StencilFailOp = impl::rhi_d3d12(pDesc->depthStencil.front.failOp);
    depthStencilDesc.FrontFace.StencilDepthFailOp = impl::rhi_d3d12(pDesc->depthStencil.front.depthFailOp);
    depthStencilDesc.FrontFace.StencilPassOp = impl::rhi_d3d12(pDesc->depthStencil.front.passOp);
    depthStencilDesc.FrontFace.StencilFunc = impl::rhi_d3d12(pDesc->depthStencil.front.compareOp);
    depthStencilDesc.BackFace.StencilFailOp = impl::rhi_d3d12(pDesc->depthStencil.back.failOp);
    depthStencilDesc.BackFace.StencilDepthFailOp = impl::rhi_d3d12(pDesc->depthStencil.back.depthFailOp);
    depthStencilDesc.BackFace.StencilPassOp = impl::rhi_d3d12(pDesc->depthStencil.back.passOp);
    depthStencilDesc.BackFace.StencilFunc = impl::rhi_d3d12(pDesc->depthStencil.back.compareOp);

    // Setup blend state
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = pDesc->blend.alphaToCoverageEnable ? TRUE : FALSE;
    blendDesc.IndependentBlendEnable = pDesc->blend.independentBlendEnable ? TRUE : FALSE;
    
    const uint32_t renderTargetCount = basicPass.colorFormatCount;
    assert(renderTargetCount <= 8);
    for (uint32_t i = 0; i < renderTargetCount; ++i) {
        const auto& srcBlend = pDesc->blend.attachments[i];
        auto& dstBlend = blendDesc.RenderTarget[i];
        
        dstBlend.BlendEnable = srcBlend.blendEnable ? TRUE : FALSE;
        dstBlend.LogicOpEnable = pDesc->blend.logicOpEnable ? TRUE : FALSE;
        dstBlend.SrcBlend = impl::rhi_d3d12(srcBlend.srcColorBlendFactor);
        dstBlend.DestBlend = impl::rhi_d3d12(srcBlend.dstColorBlendFactor);
        dstBlend.BlendOp = impl::rhi_d3d12(srcBlend.colorBlendOp);
        dstBlend.SrcBlendAlpha = impl::rhi_d3d12(srcBlend.srcAlphaBlendFactor);
        dstBlend.DestBlendAlpha = impl::rhi_d3d12(srcBlend.dstAlphaBlendFactor);
        dstBlend.BlendOpAlpha = impl::rhi_d3d12(srcBlend.alphaBlendOp);
        dstBlend.LogicOp = impl::rhi_d3d12(pDesc->blend.logicOp);
        dstBlend.RenderTargetWriteMask = impl::rhi_d3d12(srcBlend.colorWriteMask);
    }

    // Setup render target formats
    DXGI_FORMAT rtvFormats[8] = {};
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
    
    for (uint32_t i = 0; i < renderTargetCount; ++i) {
        rtvFormats[i] = impl::rhi_d3d12(basicPass.colorFormats[i]);
    }
    
    if (basicPass.depthStencil != RHI::RHI_FORMAT_UNKNOWN) {
        dsvFormat = impl::rhi_d3d12(basicPass.depthStencil);
    }

    // Setup sample desc
    DXGI_SAMPLE_DESC sampleDesc = impl::rhi_d3d12(pDesc->sampleCount);

    // Create pipeline state description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = &refRootSignature;
    psoDesc.VS = vsBytecode;
    psoDesc.PS = psBytecode;
    psoDesc.DS = dsBytecode;
    psoDesc.HS = hsBytecode;
    psoDesc.GS = gsBytecode;
    psoDesc.StreamOutput = {};
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = pDesc->sampleMask;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.InputLayout.pInputElementDescs = pInputElements;
    psoDesc.InputLayout.NumElements = inputElementCount;
    psoDesc.PrimitiveTopologyType = impl::rhi_d3d12_type(pDesc->topology);
    psoDesc.NumRenderTargets = renderTargetCount;
    for (uint32_t i = 0; i < renderTargetCount; ++i) {
        psoDesc.RTVFormats[i] = rtvFormats[i];
    }
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc = sampleDesc;
    psoDesc.NodeMask = 0;
    psoDesc.CachedPSO = {};
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    // Create pipeline state
    if (SUCCEEDED(hr)) {
        hr = m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_ID3D12PipelineState, reinterpret_cast<void**>(&pPipelineState));
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateGraphicsPipelineState failed: %u", hr);
        }
    }

    // Create RHI object
    if (SUCCEEDED(hr)) {
        const auto pPipeline = new(std::nothrow) CD3D12Pipeline{ 
            this, pPipelineState, &refRootSignature
        };
        *ppPipeline = pPipeline;
        if (!pPipeline) {
            hr = E_OUTOFMEMORY;
            DRHI_LOGGER_ERR("Failed to allocate CD3D12Pipeline");
        }
    }

    // Clean up
    // Note: pInputElements is managed by pod::tbuffer and will be automatically freed
    RHI::SafeRelease(pPipelineState);

    return impl::d3d12_rhi(hr);
}

RHI::CODE RHI::CD3D12Device::CreatePipelineLayout(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept
{
    if (!pDesc || !ppPipelineLayout)
        return CODE_POINTER;
    // TODO: VERSION 1.0?
    //return this->create_pipeline_layout_1_0(pDesc, ppPipelineLayout);
    return this->create_pipeline_layout_1_1(pDesc, ppPipelineLayout);
}

RHI::CODE RHI::CD3D12Device::create_pipeline_layout_1_0(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept
{
#ifndef NDEBUG
    // !!TEST CODE!!
    ID3DBlob* serializedDesc = nullptr;;
    ID3D12RootSignature* rootSignature = nullptr;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    desc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


    HRESULT hr = ::D3D12SerializeVersionedRootSignature(&desc, &serializedDesc, nullptr);
    assert(SUCCEEDED(hr));
    hr = m_pDevice->CreateRootSignature(0,
        serializedDesc->GetBufferPointer(),
        serializedDesc->GetBufferSize(),
        IID_ID3D12RootSignature,
        (void**)&rootSignature
    );
    assert(SUCCEEDED(hr));

    const auto pPipelineLayout = new(std::nothrow) CD3D12PipelineLayout{ this, *pDesc, rootSignature };
    *ppPipelineLayout = pPipelineLayout;
    assert(pPipelineLayout);
    rootSignature->Release();
    serializedDesc->Release();
    return CODE_OK;
#else
    return CODE_NOTIMPL;
#endif
}


RHI::CODE RHI::CD3D12Device::create_pipeline_layout_1_1(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept
{
    assert(m_pDevice);
    HRESULT hr = S_OK;
    ID3D12RootSignature* pRootSignature = nullptr;
    ID3DBlob* pSignatureBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    // Validate push descriptor count
    if (pDesc->pushDescriptorCount > MAX_PUSH_DESCRIPTOR) {
        DRHI_LOGGER_ERR("pushDescriptorCount (%u) exceeds MAX_PUSH_DESCRIPTOR (%u)", 
            pDesc->pushDescriptorCount, MAX_PUSH_DESCRIPTOR);
        return CODE_INVALIDARG;
    }
    if (pDesc->pushDescriptorCount > 0 && !pDesc->pushDescriptors) {
        DRHI_LOGGER_ERR("pushDescriptorCount > 0 but pushDescriptors is nullptr");
        return CODE_INVALIDARG;
    }

    // Validate static sampler count
    if (pDesc->staticSamplerCount > 0 && !pDesc->staticSamplers) {
        DRHI_LOGGER_ERR("staticSamplerCount > 0 but staticSamplers is nullptr");
        return CODE_INVALIDARG;
    }

    // Calculate descriptor set / space index for static sampler & descriptor buffer
    uint32_t staticSamplerSpace = 0;
    uint32_t descriptorBufferSpace = 0;
    impl::calculate_setid(*pDesc, staticSamplerSpace, descriptorBufferSpace);

    // Count total root parameters: push constant (0 or 1) + push descriptors + buffer table
    uint32_t totalParameterCount = 0;
    if (pDesc->pushConstant.size > 0) {
        totalParameterCount += 1;
    }
    totalParameterCount += pDesc->pushDescriptorCount;

    if (pDesc->descriptorBuffer > 0) {
        totalParameterCount += 1;
    }


    // +1 for push constant, +2 for buffer
    D3D12_ROOT_PARAMETER1 rootParameters[MAX_PUSH_DESCRIPTOR + 1 + 2];
    uint32_t parameterIndex = 0;
    //D3D12_DESCRIPTOR_RANGE1 descriptorRanges[2];
    //uint32_t rangeIndex = 0;

    // Root Constants (Push Constant)
    if (pDesc->pushConstant.size > 0) {
        assert(pDesc->pushConstant.size % 4 == 0);
        const uint32_t num32BitValues = (pDesc->pushConstant.size + 3u) / 4u;
        auto& rootParam = rootParameters[parameterIndex];
        rootParam = {};
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParam.ShaderVisibility = impl::rhi_d3d12(pDesc->pushConstant.visibility);
        rootParam.Constants.ShaderRegister = 0; // b0
        rootParam.Constants.RegisterSpace = 0;  // space0
        rootParam.Constants.Num32BitValues = num32BitValues;
        ++parameterIndex;
    }

    // Root Descriptors (Push Descriptors)
    if (pDesc->pushDescriptors && pDesc->pushDescriptorCount > 0) {
        for (uint32_t i = 0; i < pDesc->pushDescriptorCount; ++i) {
            const auto& pushDesc = pDesc->pushDescriptors[i];
            auto& rootParam = rootParameters[parameterIndex];
            rootParam = {};
#if 0
            // Validate descriptor type (samplers cannot be root descriptors)
            if (pushDesc.type == DESCRIPTOR_TYPE_SAMPLER) {
                DRHI_LOGGER_ERR("DESCRIPTOR_TYPE_SAMPLER cannot be used as root descriptor");
                return CODE_INVALIDARG;
            }
#endif

            rootParam.ParameterType = impl::rhi_d3d12(pushDesc.type);
            rootParam.ShaderVisibility = impl::rhi_d3d12(pushDesc.visibility);
            
            // Set descriptor register and space
            rootParam.Descriptor.ShaderRegister = pushDesc.baseBinding;
            rootParam.Descriptor.RegisterSpace = 0; // space0
            rootParam.Descriptor.Flags = impl::rhi_d3d12_rd_flags(pushDesc.volatility);

            ++parameterIndex;
        }
    }

    // Max 32 static samplers (reasonable limit)
    constexpr uint32_t MAX_STATIC_SAMPLERS = 32;
    D3D12_STATIC_SAMPLER_DESC staticSamplers[MAX_STATIC_SAMPLERS];

    // Static Samplers
    if (pDesc->staticSamplerCount > 0) {
        if (pDesc->staticSamplerCount > MAX_STATIC_SAMPLERS) {
            DRHI_LOGGER_ERR("staticSamplerCount (%u) exceeds maximum limit", pDesc->staticSamplerCount);
            return CODE_INVALIDARG;
        }

        for (uint32_t i = 0; i < pDesc->staticSamplerCount; ++i) {
            const auto& rhiStaticSampler = pDesc->staticSamplers[i];
            auto& d3d12StaticSampler = staticSamplers[i];
            d3d12StaticSampler = {};

            // Set register and space
            d3d12StaticSampler.ShaderRegister = rhiStaticSampler.binding;
            d3d12StaticSampler.RegisterSpace = staticSamplerSpace;
            d3d12StaticSampler.ShaderVisibility = impl::rhi_d3d12(rhiStaticSampler.visibility);

            // Convert sampler descriptor using helper function
            D3D12_SAMPLER_DESC samplerDesc = {};
            impl::rhi_d3d12_cvt(&rhiStaticSampler.sampler, &samplerDesc);
            
            // Copy common fields from D3D12_SAMPLER_DESC to D3D12_STATIC_SAMPLER_DESC
            d3d12StaticSampler.Filter = samplerDesc.Filter;
            d3d12StaticSampler.AddressU = samplerDesc.AddressU;
            d3d12StaticSampler.AddressV = samplerDesc.AddressV;
            d3d12StaticSampler.AddressW = samplerDesc.AddressW;
            d3d12StaticSampler.MipLODBias = samplerDesc.MipLODBias;
            d3d12StaticSampler.MaxAnisotropy = samplerDesc.MaxAnisotropy;
            d3d12StaticSampler.ComparisonFunc = samplerDesc.ComparisonFunc;
            d3d12StaticSampler.MinLOD = samplerDesc.MinLOD;
            d3d12StaticSampler.MaxLOD = samplerDesc.MaxLOD;
            
            // Convert BorderColor using helper function
            d3d12StaticSampler.BorderColor = impl::rhi_d3d12(rhiStaticSampler.sampler.borderColor);
        }

        rootSignatureDesc.Desc_1_1.NumStaticSamplers = pDesc->staticSamplerCount;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = staticSamplers;
    }
    else {
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
    }

    // ONLY ONE DESCRIPTOR BUFFER
    CDescriptorBufferRanges rangesNormal;
    CDescriptorBufferRanges rangesSamper;
    if (pDesc->descriptorBuffer) {
        const auto pDescBuffer = static_cast<CD3D12DescriptorBuffer*>(pDesc->descriptorBuffer);
        if (const auto heap = pDescBuffer->NormalHeap()) {
            auto& rootParam = rootParameters[parameterIndex];
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            // TODO: MOVE SlotBitsD3D12 VISIBILITY TO rootParameter VISIBILITY
            rootParam.ShaderVisibility = pDescBuffer->GetNormalVisibility();
            const auto code = pDescBuffer->SetupRanges(descriptorBufferSpace, rangesNormal, false);
            if (FAILED(code)) {
                hr = code;
                DRHI_LOGGER_ERR("pDescBuffer->SetupRanges(false) failed");
            }
            rootParam.DescriptorTable.NumDescriptorRanges = rangesNormal.size();
            rootParam.DescriptorTable.pDescriptorRanges = rangesNormal.data();
            ++parameterIndex;
        }
        if (const auto heap = pDescBuffer->SamplerHeap()) {
            auto& rootParam = rootParameters[parameterIndex];
            rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            // TODO: MOVE SlotBitsD3D12 VISIBILITY TO rootParameter VISIBILITY
            rootParam.ShaderVisibility = pDescBuffer->GetNormalVisibility();
            const auto code = pDescBuffer->SetupRanges(descriptorBufferSpace, rangesSamper, true);
            if (FAILED(code)) {
                hr = code;
                DRHI_LOGGER_ERR("pDescBuffer->SetupRanges(false) failed");
            }
            rootParam.DescriptorTable.NumDescriptorRanges = rangesSamper.size();
            rootParam.DescriptorTable.pDescriptorRanges = rangesSamper.data();
            ++parameterIndex;
        }
    }

    // Set root signature parameters
    rootSignatureDesc.Desc_1_1.NumParameters = parameterIndex;
    rootSignatureDesc.Desc_1_1.pParameters = rootParameters;

    // FLAG
    if (pDesc->inputAssembler) {
        flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    }

    rootSignatureDesc.Desc_1_1.Flags = flags;

    // SERIALIZE
    if (SUCCEEDED(hr)) {
        hr = ::D3D12SerializeVersionedRootSignature(
            &rootSignatureDesc,
            &pSignatureBlob,
            &pErrorBlob
        );

        if (FAILED(hr)) {
            if (pErrorBlob) {
                DRHI_LOGGER_ERR("D3D12SerializeVersionedRootSignature failed: %s", (char*)pErrorBlob->GetBufferPointer());
            }
            else {
                DRHI_LOGGER_ERR("D3D12SerializeVersionedRootSignature failed: %u", hr);
            }
        }
    }

    // CREATE ROOT SIGNATURE
    if (SUCCEEDED(hr)) {
        hr = m_pDevice->CreateRootSignature(
            0,
            pSignatureBlob->GetBufferPointer(),
            pSignatureBlob->GetBufferSize(),
            IID_ID3D12RootSignature, reinterpret_cast<void**>(&pRootSignature)
        );
        if (FAILED(hr)) {
            DRHI_LOGGER_ERR("CreateRootSignature failed: %u", hr);
        }
    }

    // CREATE RHI OBJECT
    if (SUCCEEDED(hr)) {
        const auto pPipelineLayout = new(std::nothrow) CD3D12PipelineLayout{ this,*pDesc, pRootSignature };
        *ppPipelineLayout = pPipelineLayout;
        if (!pPipelineLayout) {
            hr = E_OUTOFMEMORY;
            DRHI_LOGGER_ERR("Failed to allocate CD3D12PipelineLayout");
        } 
    }

    // CLEAN UP
    RHI::SafeRelease(pRootSignature);
    RHI::SafeRelease(pSignatureBlob);
    RHI::SafeRelease(pErrorBlob);

    return impl::d3d12_rhi(hr);
}

// ----------------------------------------------------------------------------
//                           CD3D12RenderPass
// ----------------------------------------------------------------------------

#if 0
RHI::CD3D12RenderPass::CD3D12RenderPass(CD3D12Device* pDevice, const RenderPassDesc& desc) noexcept
    : m_pThisDevice(pDevice)
    , m_desc(desc)
{
    assert(pDevice);
    pDevice->AddRefCnt();
}

RHI::CD3D12RenderPass::~CD3D12RenderPass() noexcept
{
    RHI::SafeDispose(m_pThisDevice);
}
#endif

// ----------------------------------------------------------------------------
//                           CD3D12Pipeline
// ----------------------------------------------------------------------------

RHI::CD3D12Pipeline::CD3D12Pipeline(CD3D12Device* pDevice
    , ID3D12PipelineState* pPipelineState
    , ID3D12RootSignature* rs) noexcept
    : m_pThisDevice(pDevice)
    , m_pPipelineState(pPipelineState)
    , m_pBondRootSignature(rs)
{
    assert(pDevice && pPipelineState && rs);
    pDevice->AddRefCnt();
    pPipelineState->AddRef();
    rs->AddRef();
}

RHI::CD3D12Pipeline::~CD3D12Pipeline() noexcept
{
    RHI::SafeRelease(m_pBondRootSignature);
    RHI::SafeRelease(m_pPipelineState);
    RHI::SafeDispose(m_pThisDevice);
}

// ----------------------------------------------------------------------------
//                            CD3D12Device
// ----------------------------------------------------------------------------

#if 0
RHI::CODE RHI::CD3D12Device::CreateRenderPass(const RenderPassDesc* pDesc, IRHIRenderPass** ppRenderPass) noexcept
{
    if (!pDesc || !ppRenderPass)
        return CODE_POINTER;

    constexpr uint32_t COLOR_PASS_COUNT = sizeof(pDesc->colorAttachment) / sizeof(pDesc->colorAttachment[0]);

    const auto colorPassCount = pDesc->colorAttachmentCount;
    if (colorPassCount > COLOR_PASS_COUNT)
        return CODE_INVALIDARG;

    // D3D12 doesn't have a RenderPass object, we just store the description
    // The information will be used when BeginRenderPass is called
    const auto pRenderPass = new (std::nothrow) CD3D12RenderPass{ this, *pDesc };
    if (!pRenderPass) {
        DRHI_LOGGER_ERR("Failed to allocate CD3D12RenderPass");
        return CODE_OUTOFMEMORY;
    }

    *ppRenderPass = pRenderPass;
    return CODE_OK;
}
#endif

#endif

