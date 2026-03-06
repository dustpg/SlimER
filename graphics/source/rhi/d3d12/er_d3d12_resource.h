#pragma once

#include <rhi/er_instance.h>
#ifndef SLIM_RHI_NO_D3D12
#include <rhi/er_resource.h>
#include "../common/er_helper_p.h"

struct ID3D12DescriptorHeap;
struct ID3D12Resource;

namespace Slim { namespace RHI {

    class CD3D12Device;
    class CD3D12SwapChain;

    struct BufferDescD3D12 : BufferDesc {

        ID3D12Resource*             resource;

        uint32_t                    resState;

    };

    class CD3D12Buffer final : public impl::ref_count_class<CD3D12Buffer, IRHIBuffer> {
    public:

        CD3D12Buffer(CD3D12Device* pDevice, const BufferDescD3D12& desc) noexcept;

        ~CD3D12Buffer() noexcept;

        auto&RefResource() noexcept { return *m_pResource; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefDesc() const noexcept { return m_sDesc; }

        auto GetGpuAddress() const noexcept { return m_u64GpuAddress; }

        auto GetSize() const noexcept { return m_sDesc.size; }

        auto GetResState() const noexcept { return m_eResState; }

    public:

        CODE Map(void** ppData, const BufferRange* rangeHint = nullptr) noexcept override;

        void Unmap() noexcept override;

        void SetDebugName(const char* name) noexcept override;

    protected:

        uint64_t                    m_u64GpuAddress = 0;

        CD3D12Device*               m_pThisDevice = nullptr;

        ID3D12Resource*             m_pResource = nullptr;

        BufferDesc                  m_sDesc{};

        uint32_t                    m_eResState = 0;

    };

    struct ImageDescD3D12 : ImageDesc {

        ID3D12Resource*             resource;
        // image from swap chain [strong ref] [optional]
        CD3D12SwapChain*            swapchain;

        //uint32_t                    dxgifmt;

    };

    class CD3D12Image final : public impl::ref_count_class<CD3D12Image, IRHIImage> {
    public:

        CD3D12Image(CD3D12Device* pDevice, const ImageDescD3D12& desc) noexcept;

        ~CD3D12Image() noexcept;

        auto&RefResource() noexcept { return *m_pResource; }

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefDesc() const noexcept { return m_sDesc; }

        //auto GetDxgiFormat() const noexcept { return m_eDxgiFormat; }

    public:

        void DisposeDescriptor(size_t ptr) noexcept;

    protected:

        void SetDebugName(const char* name) noexcept override;

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        ImageDesc                   m_sDesc{};

        ID3D12Resource*             m_pResource = nullptr;

        CD3D12SwapChain*            m_pOwner = nullptr;

        //uint32_t                    m_eDxgiFormat = 0;

    };

    struct AttachmentDescD3D12 : AttachmentDesc {

        CD3D12Image*                image;

        ID3D12DescriptorHeap*       heap;

        size_t                      ptr;

    };

    class CD3D12Attachment final : public impl::ref_count_class<CD3D12Attachment, IRHIAttachment> {
    public:

        CD3D12Attachment(CD3D12Device* pDevice, const AttachmentDescD3D12& desc) noexcept;

        ~CD3D12Attachment() noexcept;

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefImage() noexcept { return *m_pImage; }

        auto&RefDesc() const noexcept { return m_sDesc; }

        auto GetCpuAddress() const noexcept { return m_hHeapPtr; }

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        CD3D12Image*                m_pImage = nullptr;
        // FALL BACK
        ID3D12DescriptorHeap*       m_pStandaloneHeap = nullptr;

        size_t                      m_hHeapPtr = 0;

        AttachmentDesc              m_sDesc{};

    };

#if 0
    class CD3D12RenderPass;

    class CD3D12FrameBuffer final : public impl::ref_count_class<CD3D12FrameBuffer, IRHIFrameBuffer> {
    public:

        CD3D12FrameBuffer(CD3D12Device* pDevice, const FrameBufferDesc& desc) noexcept;

        ~CD3D12FrameBuffer() noexcept;

        auto&RefDevice() noexcept { return *m_pThisDevice; }

        auto&RefDesc() const noexcept { return m_sDesc; }

    protected:

        CD3D12Device*               m_pThisDevice = nullptr;

        FrameBufferDesc             m_sDesc{};

    };
#endif

}}
#endif

