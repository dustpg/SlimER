#pragma once
#include "er_base.h"


namespace Slim { namespace RHI {

    enum { MAX_SHADER_ENTRY_LENGTH = 31 };

    struct DescriptorBufferDesc;
    struct SwapChainDesc;
    struct BufferDesc;
    struct CommandPoolDesc;
    struct CommandQueueDesc;
    struct PipelineDesc;
    struct RenderPassDesc;
    struct PipelineLayoutDesc;
    struct AttachmentDesc;
    struct ImageDesc;
    struct FrameBufferDesc;

    struct IRHIDescriptorBuffer;
    struct IRHIPipeline;
    struct IRHIPipelineLayout;
    struct IRHIShader;
    struct IRHISwapChain;
    struct IRHIBuffer;
    struct IRHICommandPool;
    struct IRHICommandQueue;
    struct IRHIImage;
    struct IRHIAttachment;
#if 0
    struct IRHIRenderPass;
    struct IRHIFrameBuffer;
#endif

    struct DRHI_NO_VTABLE IRHIFence : IRHIBase {

        virtual uint64_t GetCompletedValue() noexcept = 0;

        virtual WAIT Wait(uint64_t value, uint32_t time) noexcept = 0;

    };


    struct DRHI_NO_VTABLE IRHIDevice : IRHIBase {

        virtual CODE Flush() noexcept = 0;

        virtual CODE CreateSwapChain(const SwapChainDesc* pDesc, IRHISwapChain** ppSwapChain) noexcept = 0;

        virtual CODE CreateCommandPool(const CommandPoolDesc* pDesc, IRHICommandPool** ppPool) noexcept = 0;

        virtual CODE CreateCommandQueue(const CommandQueueDesc* pDesc, IRHICommandQueue** ppQueue) noexcept = 0;

        virtual CODE CreateFence(uint64_t initialValue, IRHIFence** ppFence) noexcept = 0;

        virtual CODE CreateBuffer(const BufferDesc* pDesc, IRHIBuffer** ppBuffer) noexcept = 0;

        virtual CODE CreateShader(const void* data, size_t size, IRHIShader** ppShader, const char* entry = nullptr/*"main"*/) noexcept = 0;

        virtual CODE CreatePipeline(const PipelineDesc* pDesc, IRHIPipeline** ppPipeline) noexcept = 0;

        virtual CODE CreatePipelineLayout(const PipelineLayoutDesc* pDesc, IRHIPipelineLayout** ppPipelineLayout) noexcept = 0;

#if 0
        virtual CODE CreateRenderPass(const RenderPassDesc* pDesc, IRHIRenderPass** ppRenderPass) noexcept = 0;

        virtual CODE CreateFrameBuffer(const FrameBufferDesc* pDesc, IRHIFrameBuffer** ppFrameBuffer) noexcept = 0;
#endif

        virtual CODE CreateImage(const ImageDesc* pDesc, IRHIImage** ppImage) noexcept = 0;

        virtual CODE CreateAttachment(const AttachmentDesc* pDesc, IRHIImage * pImage, IRHIAttachment** ppView) noexcept = 0;

        virtual CODE CreateDescriptorBuffer(const DescriptorBufferDesc* pDesc, IRHIDescriptorBuffer** ppBuffer)noexcept = 0;
    };
}}