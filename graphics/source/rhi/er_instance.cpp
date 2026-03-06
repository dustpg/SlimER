#include <rhi/er_instance.h>
#include "common/er_helper_p.h"
#include "d3d12/er_backend_d3d12.h"
#include "vulkan/er_backend_vulkan.h"

using namespace Slim;

namespace Slim { namespace impl {

    void init() noexcept {

    }

}}

RHI::CODE CreateSlimRHIInstance(const Slim::RHI::InstanceDesc* pDesc, Slim::RHI::IRHIInstance** ppIns) noexcept
{
    if (!pDesc || !ppIns)
        return RHI::CODE_POINTER;

    impl::init();

    switch (pDesc->api)
    {
    case RHI::API::VULKAN:
#ifndef SLIM_RHI_NO_VULKAN
        return RHI::CreateInstanceVulkan(pDesc, ppIns);
#endif
        break;
    case RHI::API::D3D12:
#ifndef SLIM_RHI_NO_D3D12
        return RHI::CreateInstanceD3D12(pDesc, ppIns);
#endif
        break;
    default:
        return RHI::CODE_INVALIDARG;
    }
    return RHI::CODE_NOTIMPL;
}