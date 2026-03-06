#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_swapchain.h>
#include "../common/er_helper_p.h"

struct IDXGISwapChain3;
struct ID3D12Device;
//struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12Resource;

namespace Slim { namespace RHI {

    class CD3D12Device;
    class CD3D12CommandQueue;
    class CD3D12Image;

    class CD3D12SwapChain final : public impl::ref_count_class<CD3D12SwapChain, IRHISwapChain> {
    public:

        CD3D12SwapChain(CD3D12Device* pDevice, const SwapChainDesc& desc, IDXGISwapChain3* pSwapChain, CD3D12CommandQueue* pQueue) noexcept;

        ~CD3D12SwapChain() noexcept;

        long Init() noexcept;

        void GetDesc(SwapChainDesc* pDesc) noexcept override;

        void GetBoundQueue(IRHICommandQueue** ppQueue) noexcept override;

        CODE AcquireFrame(FrameContext& ctx) noexcept override;

        CODE PresentFrame(uint32_t syncInterval) noexcept override;

        CODE Resize(uint32_t width, uint32_t height) noexcept override;

        CODE GetImage(uint32_t index, IRHIImage** image) noexcept override;

    public:

        void OwnerDispose(CD3D12Image*) noexcept;
      
    protected:

        struct FlightFrameData {
            uint64_t                    value;
        };

        struct SwapChainImageData {
            ID3D12Resource*             resource;
            impl::weak<CD3D12Image>     image;
        };

    protected:

        CD3D12Device*                   m_pDevice = nullptr;

        IDXGISwapChain3*                m_pSwapChain = nullptr;

        CD3D12CommandQueue*             m_pPresentQueue = nullptr;

        ID3D12Fence*                    m_pFrameCounter = nullptr;

        void*                           m_hFrameEvent = nullptr;

        SwapChainDesc                   m_sDesc{};

        uint32_t                        m_uCurrentFlightIndex = 0;

        uint32_t                        m_uCurrentFrameIndex = 0;

        FlightFrameData                 m_szFlightFrames[MAX_FRAMES_IN_FLIGHT] {};

        SwapChainImageData              m_szSwapChainImages[SWAPCHAIN_MAX_BUFFER_COUNT]{};

        uint64_t                        m_uFrameCounter = 0;

    };
}}
#endif

