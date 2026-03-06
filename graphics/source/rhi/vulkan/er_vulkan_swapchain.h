#pragma once
#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_VULKAN
#include <rhi/er_swapchain.h>
#include <vulkan/vulkan.h>
#include "../common/er_helper_p.h"

namespace Slim { namespace RHI {

    class CVulkanDevice;
    class CVulkanCmdQueue;
    class CVulkanImage;

    struct SwapChainDescVulkan : SwapChainDesc {

        VkPresentModeKHR            presentMode;

        VkSurfaceKHR                surface;

        VkSwapchainKHR              swapchain;

    };

    class CVulkanSwapChain final : public impl::ref_count_class<CVulkanSwapChain, IRHISwapChain>  {
    public:

        CVulkanSwapChain(CVulkanDevice*, const SwapChainDescVulkan& desc) noexcept;

        ~CVulkanSwapChain() noexcept;

        VkResult Init() noexcept;

        void GetDesc(SwapChainDesc* pDesc) noexcept override;

        void GetBoundQueue(IRHICommandQueue** ppQueue) noexcept override;

        CODE AcquireFrame(FrameContext& ctx) noexcept override;

        CODE PresentFrame(uint32_t syncInterval) noexcept override;

        CODE Resize(uint32_t width, uint32_t height) noexcept override;

        CODE GetImage(uint32_t index, IRHIImage** image) noexcept override;

    public:

        void OwnerDispose(CVulkanImage*) noexcept;

        auto GetPresentIndex() const noexcept { return m_uPresentIndex; }

        auto&CurrentFlight() const noexcept { return m_szFlightFrames[m_uCurrentFlight]; };

        auto&CurrentSwapchain() const noexcept { return m_szSwapChainImages[m_uNextFrame]; };

    protected:

        struct FlightFrameData {
            VkSemaphore                 imageAvailable;
            VkFence                     inFlight;
        };

        struct SwapChainImageData {
            VkImage                     handle;
            VkSemaphore                 renderFinished;
            impl::weak<CVulkanImage>    image;
        };

    protected:

        CVulkanDevice*              m_pThisDevice = nullptr;

        CVulkanCmdQueue*            m_pGraphicsQueue = nullptr;


        VkQueue                     m_hGraphicsQueue = VK_NULL_HANDLE;

        VkQueue                     m_hPresentQueue = VK_NULL_HANDLE;

        SwapChainDescVulkan         m_sDesc{};

        uint32_t                    m_uCurrentFlight = 0;

        uint32_t                    m_uNextFrame = 0;

        uint32_t                    m_uPresentIndex = 0;

        FlightFrameData             m_szFlightFrames[MAX_FRAMES_IN_FLIGHT] = {};

        SwapChainImageData          m_szSwapChainImages[SWAPCHAIN_MAX_BUFFER_COUNT] = {};
    };
}}
#endif