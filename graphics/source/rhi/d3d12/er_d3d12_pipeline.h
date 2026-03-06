#pragma once
#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_pipeline.h>
#include <rhi/er_device.h>
#include "../common/er_helper_p.h"
#include "../common/er_pod_vector.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace Slim { namespace RHI {

    class CD3D12Device;

    class CD3D12Shader final : public impl::ref_count_class<CD3D12Shader, IRHIShader> {
    public:

        CD3D12Shader(CD3D12Device* pDevice, const void* pData, size_t size) noexcept;

        ~CD3D12Shader() noexcept;

        auto GetBytecodeData() const noexcept { return m_pCodeData; }

        auto GetBytecodeSize() const noexcept { return m_uCodeLength; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        void*                       m_pCodeData = nullptr;

        size_t                      m_uCodeLength = 0;

    };

    //struct PipelineLayoutDescD3D12 : PipelineLayoutDesc {};

    class CD3D12PipelineLayout final : public impl::ref_count_class<CD3D12PipelineLayout, IRHIPipelineLayout> {
    public:

        CD3D12PipelineLayout(CD3D12Device* pDevice, const PipelineLayoutDesc& desc, ID3D12RootSignature* pRootSignature) noexcept;

        ~CD3D12PipelineLayout() noexcept;

        auto&RefRootSignature() const noexcept { return *m_pRootSignature; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto PushDescriptorBase() const noexcept { return m_uPushDescriptorBase; }

        auto DescriptorBufferBase() const noexcept { return m_uDescriptorBufferBase; }

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        ID3D12RootSignature*        m_pRootSignature = nullptr;

        uint32_t                    m_uPushDescriptorBase = 0;

        uint32_t                    m_uDescriptorBufferBase = 0;

    };

#if 0
    class CD3D12RenderPass final : public impl::ref_count_class<CD3D12RenderPass, IRHIRenderPass> {
    public:

        CD3D12RenderPass(CD3D12Device* pDevice, const RenderPassDesc& desc) noexcept;

        ~CD3D12RenderPass() noexcept;

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefDesc() const noexcept { return m_desc; }

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        RenderPassDesc              m_desc;

    };
#endif

    class CD3D12Pipeline final : public impl::ref_count_class<CD3D12Pipeline, IRHIPipeline> {
    public:

        CD3D12Pipeline(CD3D12Device* pDevice, ID3D12PipelineState* pPipelineState, ID3D12RootSignature*) noexcept;

        ~CD3D12Pipeline() noexcept;

        //long Init() noexcept;

        auto&RefRootSignature() noexcept { return *m_pBondRootSignature; }

        auto&RefPipelineState() noexcept { return *m_pPipelineState; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        ID3D12PipelineState*        m_pPipelineState = nullptr;

        ID3D12RootSignature*        m_pBondRootSignature = nullptr;

    };

}}
#endif

