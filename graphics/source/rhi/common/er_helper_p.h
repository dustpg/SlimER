#pragma once
#include <atomic>
#include <cstdlib>
#include <new>
#include <rhi/er_base.h>

namespace Slim {

    namespace impl {

        template<typename T>
        struct weak { T* ptr; };

        template<typename T>
        struct relaxed_counter {
            explicit relaxed_counter(T v) noexcept : count_(v) {}
            relaxed_counter(const relaxed_counter&) noexcept = delete;
            relaxed_counter& operator=(const relaxed_counter&) noexcept = delete;
            
            // Increment operators
            T operator++() noexcept { return count_.fetch_add(1, std::memory_order_relaxed) + 1; }
            T operator++(int) noexcept { return count_.fetch_add(1, std::memory_order_relaxed); }
            
            // Decrement operators
            T operator--() noexcept { return count_.fetch_sub(1, std::memory_order_acq_rel) - 1; }
            T operator--(int) noexcept { return count_.fetch_sub(1, std::memory_order_acq_rel ); }
            
        private:
            std::atomic<T>      count_;
        };
    
        template<typename T = impl::relaxed_counter<int32_t>>
        struct ref_count {

            long add_ref() noexcept { return ++ref_count_; };

            template<typename T>
            long dispose(T* p) noexcept { 
                const auto cnt = --ref_count_;
                if (!cnt) delete p;
                return cnt; 
            }

            T           ref_count_{ 1 };

        };

        struct base_class {
            //uint32_t    base_class_magic = 0x44524849;

            static void*imalloc(size_t len) noexcept { return std::malloc(len); }
            static void ifree(void* ptr) noexcept { return std::free(ptr); }
            static void*irealloc(void* ptr, size_t len) noexcept { return std::realloc(ptr, len); }

            // Override new/delete to use imalloc/ifree
            static void* operator new(size_t size, const std::nothrow_t&) noexcept { return imalloc(size); }
            static void operator delete(void* ptr) noexcept { ifree(ptr); }
        };

        struct base_class_replacement {
            //uint32_t    base_class_magic = 0x44524849;
            static void* imalloc(size_t len) noexcept { return std::malloc(len); }
            static void ifree(void* ptr) noexcept { return std::free(ptr); }
            static void* operator new(size_t size) = delete;
            static void operator delete(void* ptr) noexcept { ifree(ptr); }
            static void* operator new(size_t size, void* ptr) noexcept { return ptr; }

        };

        // I->std::has_virtual_destructor_v || std::is_final
        template<typename T, typename I>
        class ref_count_class : public I, public base_class {
        public:

            long Dispose() noexcept override final { return counter_.dispose(static_cast<T*>(this)); }

            long AddRefCnt() noexcept override final { return counter_.add_ref(); }

            RHI::CODE InnerQuery(const void* data, size_t len, void** output) noexcept override { return RHI::CODE_NOTIMPL; };

        protected:

            ref_count<>         counter_;

        };


        // I->std::has_virtual_destructor_v || std::is_final
        template<typename T, typename I>
        class ref_count_class_replacement : public I, public base_class_replacement {
        public:

            long Dispose() noexcept override final { return counter_.dispose(static_cast<T*>(this)); }

            long AddRefCnt() noexcept override final { return counter_.add_ref(); }

            RHI::CODE InnerQuery(const void* data, size_t len, void** output) noexcept override { return RHI::CODE_NOINTERFACE; };

        protected:

            ref_count<>         counter_;

        };


    }

}