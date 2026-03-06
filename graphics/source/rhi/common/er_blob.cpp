#include "er_blob.h"
#include <rhi/er_shader_compiler.h>
#include <cassert>
#include "er_helper_p.h"

using namespace Slim;


namespace Slim { namespace RHI {


    class CRHIBlob : public impl::ref_count_class_replacement<CRHIBlob, IRHIBlob> {
        void* ptr() noexcept { return &m_lfPlaceHolder; }
    public:

        ~CRHIBlob() noexcept { }

        void* GetData() noexcept override { return ptr();}

        size_t GetSize() noexcept override { return m_uLength; }

        static CRHIBlob* Create(void* ptr, size_t size) noexcept {
            if (const auto ptr = CRHIBlob::imalloc(sizeof(CRHIBlob) + size)) {
                return new(ptr) CRHIBlob{ ptr, size };
            }
            return nullptr;
        }
    protected:

        CRHIBlob(void* data, size_t l) noexcept : m_uLength(l)  {
            std::memcpy(ptr(), data, l);
        }

        size_t          m_uLength = 0;

        double          m_lfPlaceHolder = 0;
        // VLA DATA
        //char          data[0];

    };

    IRHIBlob* CreateBlob(void* ptr, size_t size) noexcept
    {
        assert(ptr && size);
        return CRHIBlob::Create(ptr, size);
    }

}}

