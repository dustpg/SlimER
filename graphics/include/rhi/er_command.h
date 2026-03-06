#pragma once
#include <initializer_list>
#include "er_base.h"
#include "er_pipeline.h"
#include "er_descriptor.h"

namespace Slim { namespace RHI {
    enum PASS_LOAD_OP : uint32_t;
    enum PASS_STORE_OP : uint32_t;
    //enum DESCRIPTOR_TYPE : uint32_t;

    enum QUEUE_TYPE : uint32_t {
        QUEUE_TYPE_GRAPHICS,
        QUEUE_TYPE_COMPUTE,
        QUEUE_TYPE_COPY,
        QUEUE_TYPE_VIDEO_DECODE,
        QUEUE_TYPE_VIDEO_ENCODE,
    };

    enum INDEX_TYPE : uint32_t {
        INDEX_TYPE_UINT16=0,
        INDEX_TYPE_UINT32,
    };

    struct CommandPoolDesc {

        QUEUE_TYPE          queueType;
    };

    struct IRHIBuffer;
    struct IRHIImage;
    struct IRHIAttachment;
    struct IRHIFence;
    struct IRHICommandList;
    struct IRHISampler;
    struct IRHIPipeline;
    struct IRHIPipelineLayout;
    struct IRHIDescriptorBuffer;
#if 0
    struct IRHIRenderPass;
    struct IRHIFrameBuffer;
#endif

    struct FencePair { IRHIFence* fence; uint64_t value; };

    struct SubmitSynch {
        // TODO: REMOVE MAX_SIGNAL_FENCE_COUNT?
        enum : uint32_t { MAX_SIGNAL_FENCE_COUNT = 16 };
        uint32_t            waitCount;
        uint32_t            signalCount;

        FencePair           waitPairs[MAX_SIGNAL_FENCE_COUNT];
        FencePair           signalPairs[MAX_SIGNAL_FENCE_COUNT];

    };
    struct FencePairWait { IRHIFence* fence; uint64_t value; };
    struct FencePairSignal { IRHIFence* fence; uint64_t value; };

    struct SubmitSynchAuto : SubmitSynch {
        SubmitSynchAuto(FencePairSignal s) noexcept {
            waitCount = 0; signalCount = 1;
            signalPairs[0] = reinterpret_cast<const FencePair&>(s);
        }
    };


    struct DRHI_NO_VTABLE IRHICommandQueue : IRHIBase {

        virtual CODE Submit(uint32_t count, IRHICommandList* const*, const SubmitSynch* synch = nullptr) noexcept = 0;
        
        virtual CODE WaitIdle() noexcept = 0;
    };


    struct BeginRenderPassColorDesc {
        IRHIAttachment*     attachment;
        PASS_LOAD_OP        load;
        PASS_STORE_OP       store;
        float               clear[4];
    };

    struct BeginRenderDepthStencilDesc {
        IRHIAttachment*     attachment;
        PASS_LOAD_OP        depthLoad;
        PASS_STORE_OP       depthStore;
        float               depthClear;
        PASS_LOAD_OP        stencilLoad;
        PASS_STORE_OP       stencilStore;
        uint8_t             stencilClear;
        bool                stencilTestEnable;
    };

    struct BeginRenderPassDesc {
        //IRHIRenderPass*             renderpass;
        //IRHIFrameBuffer*            framebuffer;

        uint32_t                    width;
        uint32_t                    height;

        uint32_t                    colorCount;
        BeginRenderPassColorDesc    colors[8];
        BeginRenderDepthStencilDesc depthStencil;
    };


    enum IMAGE_LAYOUT : uint32_t {
        IMAGE_LAYOUT_UNDEFINED = 0,
        IMAGE_LAYOUT_PRESENT_DST,     // vulkan: UNDEFINED d3d12: PRESENT
        IMAGE_LAYOUT_PRESENT_SRC,     // vulkan: PRESENT_SRC d3d12: PRESENT
        IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        IMAGE_LAYOUT_DEPTH_ATTACHMENT_READ, // DEPTH+STENCIL
        IMAGE_LAYOUT_DEPTH_ATTACHMENT_WRITE,// DEPTH+STENCIL
        IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        IMAGE_LAYOUT_GENERAL,
    };

    struct ResourceBarrierDesc {
        IRHIImage*          resource;
        IMAGE_LAYOUT        before;
        IMAGE_LAYOUT        after;
    };


    struct VertexBufferView {
        uint64_t            offset;
        IRHIBuffer*         buffer;
        uint32_t            stride;
    };

    struct CopyBufferDesc {
        IRHIBuffer*         destination;
        IRHIBuffer*         source;
        uint64_t            destinationOffset;
        uint64_t            sourceOffset;
        uint64_t            size;

    };

    struct CopyImageDesc {
        IRHIImage*          destination;
        IRHIImage*          source;

        uint32_t            sourceX;
        uint32_t            sourceY;

        uint32_t            destinationX;
        uint32_t            destinationY;

        uint32_t            width;
        uint32_t            height;
    };

    struct CopyBufferImageDesc {
        IRHIImage*          destination;
        IRHIBuffer*         source;

        uint64_t            sourceOffset;
        uint32_t            sourcePitch;

        uint32_t            destinationX;
        uint32_t            destinationY;
        uint32_t            destinationWidth;
        uint32_t            destinationHeight;

    };
    

    struct Viewport {
        float topLeftX;
        float topLeftY;
        float width;
        float height;
        float minDepth;
        float maxDepth;
    };

    struct Rect {
        int32_t left, top;
        int32_t right, bottom;
    };

    struct PushDescriptorSetDesc {
        union {
            IRHIBuffer*     buffer;     // type == DESCRIPTOR_TYPE_CONST_BUFFER
            IRHIAttachment* image;      // type == DESCRIPTOR_TYPE_SHADER_RESOURCE
        };
        DESCRIPTOR_TYPE     type;
    };

    struct PushDescriptorConstBuffer : PushDescriptorSetDesc {
        PushDescriptorConstBuffer(IRHIBuffer* b) noexcept { type = DESCRIPTOR_TYPE_CONST_BUFFER; buffer = b; }
    };
    struct PushDescriptorShaderResource : PushDescriptorSetDesc {
        PushDescriptorShaderResource(IRHIAttachment* v) noexcept { type = DESCRIPTOR_TYPE_SHADER_RESOURCE; image = v; }
    };

    // Functions in IRHICommandList do not check their parameters 
    struct DRHI_NO_VTABLE IRHICommandList : IRHIBase {

        virtual CODE Begin() noexcept = 0;

        virtual CODE End() noexcept = 0;

        virtual void BeginRenderPass(const BeginRenderPassDesc* pDesc) noexcept = 0;

        virtual void EndRenderPass() noexcept = 0;

        virtual void BindPipeline(IRHIPipeline* pPipeline) noexcept = 0;

        virtual void BindDescriptorBuffer(IRHIPipelineLayout* pLayout, IRHIDescriptorBuffer*) noexcept = 0;

        virtual void BindVertexBuffers(uint32_t firstBinding, uint32_t count, const VertexBufferView* pViews) noexcept = 0;

        virtual void BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, INDEX_TYPE type) noexcept = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) noexcept = 0;

        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) noexcept = 0;

        virtual void ResourceBarriers(uint32_t count, const ResourceBarrierDesc* pList) noexcept = 0;

        virtual void CopyBuffer(const CopyBufferDesc* pDesc) noexcept = 0;

        virtual void CopyImage(const CopyImageDesc* pDesc) noexcept = 0;

        virtual void CopyBufferToImage(const CopyBufferImageDesc* pDesc) noexcept = 0;

        virtual void SetViewports(uint32_t count, const Viewport* pViewports) noexcept = 0;

        virtual void SetScissors(uint32_t count, const Rect* pRects) noexcept = 0;
        // *Sequentially* bind the push descriptors in the pipeline layout 
        // using the specified buffers
        virtual void PushDescriptorSets(IRHIPipelineLayout* pLayout, uint32_t count, const PushDescriptorSetDesc* pDesc) noexcept = 0;

        // helpers
        void ResourceBarrier(const ResourceBarrierDesc& desc) noexcept { ResourceBarriers(1, &desc); }
        void ResourceBarriers(std::initializer_list<ResourceBarrierDesc> barriers) noexcept { ResourceBarriers(static_cast<uint32_t>(barriers.size()), barriers.begin()); }
        void BeginRenderPass(const BeginRenderPassDesc& desc) noexcept { BeginRenderPass(&desc); }
        void CopyBuffer(const CopyBufferDesc& desc) noexcept { CopyBuffer(&desc); }
        void BindVertexBuffer(const VertexBufferView& view, uint32_t firstBinding = 0) noexcept { BindVertexBuffers(firstBinding, 1, &view); }
        void SetViewport(const Viewport& viewport) noexcept { SetViewports(1, &viewport); }
        void SetScissor(const Rect& rect) noexcept { SetScissors(1, &rect); }
        void CopyImage(const CopyImageDesc& desc) noexcept { CopyImage(&desc); }
        void CopyBufferToImage(const CopyBufferImageDesc& desc) noexcept { CopyBufferToImage(&desc); }
    };

    struct DRHI_NO_VTABLE IRHICommandPool : IRHIBase {

        virtual CODE Reset() noexcept = 0;

        virtual CODE CreateCommandList(IRHICommandList** ppCmdList) noexcept = 0;

    };
}}