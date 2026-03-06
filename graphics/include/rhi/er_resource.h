#pragma once
#include "er_base.h"
namespace Slim { namespace RHI {
    struct IRHIAttachment;
    //struct IRHIFrameBuffer;
    //struct IRHIRenderPass;

    enum IMAGE_LAYOUT : uint32_t;

    enum ATTACHMENT_TYPE : uint32_t {
        ATTACHMENT_TYPE_COLOR=0,        // RTV in D3D12, color attachment in Vulkan
        //ATTACHMENT_TYPE_DEPTH,          // DSV in D3D12, depth attachment in Vulkan
        ATTACHMENT_TYPE_DEPTHSTENCIL,   // DSV in D3D12, depth/stencil attachment in Vulkan
    };

    struct AttachmentDesc {
        ATTACHMENT_TYPE         type;
        RHI_FORMAT              format;
    };

#if 0
    struct FrameBufferDesc {
        IRHIRenderPass*         renderpass;

        IRHIAttachment*         depthStencil;
        IRHIAttachment*         colorViews[8];
        uint32_t                colorViewCount;

        uint32_t                width;
        uint32_t                height;

    };
#endif
    // like VkBufferUsage
    enum BUFFER_USAGE : uint32_t {
        BUFFER_USAGE_NONE = 0,
        BUFFER_USAGE_VERTEX_BUFFER  = 1 << 0,  // VB Read
        BUFFER_USAGE_INDEX_BUFFER   = 1 << 1,  // IB Read
        BUFFER_USAGE_CONSTANT_BUFFER= 1 << 2,  // CB/Push Read
        BUFFER_USAGE_INDIRECT_ARGS  = 1 << 3,  // DrawIndirect
        BUFFER_USAGE_STORAGE_BUFFER = 1 << 4,  // SSBO/UAV Read/Write
        BUFFER_USAGE_UNIFORM_TEXEL  = 1 << 5,  // Uniform Texel Buffer
        BUFFER_USAGE_STORAGE_TEXEL  = 1 << 6,  // Storage Texel UAV
        BUFFER_USAGE_TRANSFER_SRC   = 1 << 7,  // Copy Source
        BUFFER_USAGE_TRANSFER_DST   = 1 << 8,  // Copy Dest
    };
    // strong typedef for BUFFER_USAGE
    struct BUFFER_USAGES { uint32_t flags; };

    // like VkImageUsage
    enum IMAGE_USAGE : uint32_t {
        IMAGE_USAGE_NONE = 0,
        IMAGE_USAGE_TRANSFER_SRC    = 1 << 1,   // Copy Source
        IMAGE_USAGE_TRANSFER_DST    = 1 << 2,   // Copy Dest
        IMAGE_USAGE_COLOR_ATTACHMENT= 1 << 3,   // RTV
        IMAGE_USAGE_DEPTH_STENCIL   = 1 << 4,   // DEPTH/STENCIL
        IMAGE_USAGE_STORAGE         = 1 << 5,   // UAV

    };
    // strong typedef for BUFFER_USAGE
    struct IMAGE_USAGES { uint32_t flags; };

    // D3D12 Heap Type
    enum MEMORY_TYPE : uint32_t {
        MEMORY_TYPE_DEFAULT,
        MEMORY_TYPE_UPLOAD,     // COHERENT RAM [CPU-WRITE GPU-READ]
        MEMORY_TYPE_READBACK,
        //MEMORY_TYPE_CUSTOM, 
    };


    struct ImageDesc {
        uint32_t            width;
        uint32_t            height;
        RHI_FORMAT          format;
        IMAGE_USAGES        usage;
        //MEMORY_TYPE         type;         // MEMORY_TYPE_DEFAULT only
        //IMAGE_LAYOUT        initialLayout;
    };

    struct BufferDesc {
        uint64_t            size;
        BUFFER_USAGES       usage;
        MEMORY_TYPE         type;
        uint32_t            alignment;
        //uint32_t            stride;
    };

    struct BufferRange { size_t begin, end; };

    struct ImageRange { 
        uint32_t            x, y/*, z*/;
        uint32_t            width, height/*, depth*/;
    };

    struct ImageMappedData {
        void*               data;
        uint32_t            pitch;
    };

    struct DRHI_NO_VTABLE IRHISampler : IRHIBase {

    };

    // TODO: Use handles instead of interfaces for Buffer?
    struct DRHI_NO_VTABLE IRHIBuffer : IRHIDeviceResource {
        // *ppData: return pointer to start, not offset(like D3D12) 
        virtual CODE Map(void** ppData, const BufferRange* rangeHint = nullptr) noexcept = 0;

        virtual void Unmap() noexcept = 0;

    };

    // TODO: Use handles instead of interfaces for Image?
    struct DRHI_NO_VTABLE IRHIImage : IRHIDeviceResource {

    };

    struct DRHI_NO_VTABLE IRHIAttachment : IRHIBase {

    };

#if 0
    struct DRHI_NO_VTABLE IRHIFrameBuffer : IRHIBase {

    };
#endif
}}