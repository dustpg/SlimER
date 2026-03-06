#pragma once
#include <rhi/er_instance.h>

#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_resource.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"
#ifndef SLIM_RHI_NO_VMA
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)
#endif

namespace Slim { namespace RHI {

    class CVulkanDevice;
    class CVulkanSwapChain;
    class CVulkanRenderPass;

    struct BufferDescVulkan : BufferDesc {

        VkBuffer                buffer;
#ifndef SLIM_RHI_NO_VMA
        VmaAllocation           allocation = VK_NULL_HANDLE;
        VmaAllocator            allocator = VK_NULL_HANDLE;
#endif

    };

    class CVulkanBuffer final : public impl::ref_count_class<CVulkanBuffer, IRHIBuffer> {
    public:

        CVulkanBuffer(CVulkanDevice* pDevice, const BufferDescVulkan& desc) noexcept;

        ~CVulkanBuffer() noexcept;

        auto GetHandle() const noexcept { return m_hBuffer; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefDesc() const noexcept { return m_sDesc; }

        CODE Map(void** ppData, const BufferRange* rangeHint) noexcept override;

        void Unmap() noexcept override;

    protected:

        void SetDebugName(const char* name) noexcept override;

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        BufferDesc                  m_sDesc{};

        VkBuffer                    m_hBuffer = VK_NULL_HANDLE;
#ifndef SLIM_RHI_NO_VMA
        VmaAllocation               m_hAllocation = VK_NULL_HANDLE;
        
        VmaAllocator                m_hVmaAllocator = VK_NULL_HANDLE;
        
        //void*                       m_pMappedData = nullptr;
#endif

    };

    struct ImageDescVulkan : ImageDesc {

        VkImage                 image;
        // image from swap chain [strong ref] [optional]
        CVulkanSwapChain*       swapchain;
#ifndef SLIM_RHI_NO_VMA
        VmaAllocation           allocation = VK_NULL_HANDLE;
        VmaAllocator            allocator = VK_NULL_HANDLE;
#endif

    };

    class CVulkanImage final : public impl::ref_count_class<CVulkanImage, IRHIImage> {
    public:

        CVulkanImage(CVulkanDevice* pDevice, const ImageDescVulkan& desc) noexcept;

        ~CVulkanImage() noexcept;

        auto GetHandle() const noexcept { return m_hImage; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefDesc() const noexcept { return m_sDesc; }

        auto GetAllAspectMask() const noexcept { return m_maskAllAspect; }

    protected:

        void SetDebugName(const char* name) noexcept override;

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        ImageDesc                   m_sDesc{};

        VkImage                     m_hImage = VK_NULL_HANDLE;

        CVulkanSwapChain*           m_pOwner = nullptr;
#ifndef SLIM_RHI_NO_VMA
        VmaAllocation               m_hAllocation = VK_NULL_HANDLE;
        
        VmaAllocator                m_hVmaAllocator = VK_NULL_HANDLE;
#endif
        // All aspectMask
        uint32_t                    m_maskAllAspect = 0;

    };

    struct AttachmentDescVulkan : AttachmentDesc {

        CVulkanImage*               image;
    
        VkImageView                 view;

    };

    class CVulkanAttachment final : public impl::ref_count_class<CVulkanAttachment, IRHIAttachment> {
    public:

        CVulkanAttachment(CVulkanDevice* pDevice, const AttachmentDescVulkan& desc) noexcept;

        ~CVulkanAttachment() noexcept;

        auto GetHandle() const noexcept { return m_hImageView; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefImage() noexcept { return *m_pImage; }

        auto&RefDesc() const noexcept { return m_sDesc; }

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        VkDevice                    m_hDevice = VK_NULL_HANDLE;

        CVulkanImage*               m_pImage = nullptr;

        VkImageView                 m_hImageView = VK_NULL_HANDLE;

        AttachmentDesc              m_sDesc{};

    };

#if 0
    struct FrameBufferDescVulkan : FrameBufferDesc {

        VkFramebuffer               framebuffer;

    };

    class CVulkanFrameBuffer final : public impl::ref_count_class<CVulkanFrameBuffer, IRHIFrameBuffer> {
    public:

        CVulkanFrameBuffer(CVulkanDevice* pDevice, const FrameBufferDescVulkan& desc) noexcept;

        ~CVulkanFrameBuffer() noexcept;

        auto GetHandle() const noexcept { return m_hFrameBuffer; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        //auto&RefRenderPass() noexcept { return *m_pRenderPass; }

        //auto GetWidth() const noexcept { return m_width; }

        //auto GetHeight() const noexcept { return m_height; }

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        //VkDevice                    m_hDevice = VK_NULL_HANDLE;

        //CVulkanRenderPass*          m_pRenderPass = nullptr;

        //FrameBufferDesc             m_sDesc{};

        VkFramebuffer               m_hFrameBuffer = VK_NULL_HANDLE;

        //uint32_t                    m_attachmentCount = 0;

        //CVulkanImageView*           m_pAttachments[8 + 1] = {};  // 8 color + 1 depth/stencil

        //uint32_t                    m_width = 0;

        //uint32_t                    m_height = 0;

        //uint32_t                    m_layers = 1;

    };
#endif

}}
#endif

