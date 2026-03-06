#pragma once
#include <type_traits>
#include <iterator>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>
#include <new>

namespace Slim { namespace impl {
    
    // Modern POD check (replacement for deprecated std::is_pod)
    template<typename T>
    struct is_pod_like : std::bool_constant<
        std::is_trivially_copyable_v<T> && 
        std::is_standard_layout_v<T>
    > {};

    template<typename T>
    constexpr bool is_pod_like_v = is_pod_like<T>::value;

    
    // false by default
    template<typename, typename = void>
    struct is_iterator : std::false_type {};
    // true when the iterator provides a random access category
    template<typename Iter>
    struct is_iterator<Iter, typename std::enable_if<
        std::is_base_of_v<std::random_access_iterator_tag,
        typename std::iterator_traits<Iter>::iterator_category>
    >::type> : std::true_type {};



    // constexpr log2 for unsigned integral types
    template<typename T>
    constexpr T log2(T value) noexcept {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");
        return value <= 1 ? 0 : static_cast<T>(1 + log2<T>(value >> 1));
    }


    // Push back helper impl
    template<size_t> struct push_back_helper;
    struct vector_base_private;

    /// <summary>
    /// Vector base class with type erasure
    /// </summary>
    class vector_base {
        // Template friend
        template<size_t> friend struct push_back_helper;
        friend struct vector_base_private;
                
        // Extra bit layout
        enum {
            // Extra aligned size (max for 128 byte in 32bit...)
            EX_ALIGNED1 = 0, EX_ALIGNED2 = 4,
            // Extra reservation length(+1s)
            EX_RESERVED1 = 4, EX_RESERVED2 = 8,
            // Extra fixed buffer length (SOO)
            EX_SOO1 = 8, EX_SOO2 = 16,
        };

    public:
#ifdef DHRI_VECTOR_SIZE_USE_UINT32
        // Size type
        using size_type = uint32_t;
#else
        // Size type
        using size_type = size_t;
#endif

        // Size of element
        auto size_of_element() const noexcept { return m_uByteLen; }
        // Max size
        auto max_size() const noexcept -> size_type { return 1u << 30; }
        // Clear data
        void clear() noexcept { m_uVecLen = 0; }
        // Pop back
        void pop_back() noexcept { assert(m_uVecLen && "UB: none data"); --m_uVecLen; }
        // Size of vector
        auto size() const noexcept -> size_type { return m_uVecLen; }
        // Get capacity
        auto capacity() const noexcept -> size_type { return m_uVecCap; }
        // Is empty?
        bool empty() const noexcept { return !m_uVecLen; }
        // Reserve space (returns false on failure)
        bool reserve(size_type n) noexcept;

    public:
        // Extra helper
        template<size_t ali, size_t res, size_t soo>
        struct extra_t {
            static_assert((ali & (ali - 1)) == 0, "must aligned in power of 2");
            static_assert(ali != 0, "must not be 0");
            static_assert(ali <= (1 << (1 << (EX_RESERVED2 - EX_RESERVED1))), "must less than it");
            // TODO: alignment
            static_assert(ali <= alignof(uint64_t), "todo");
            static_assert(res < 16, "must less than 16");
            static_assert(soo < 256, "must less than 256");
            static_assert(res <= soo, "must less or eq than soo");
            enum : size_t {
                value = (soo << EX_SOO1) | (res << EX_RESERVED1) | (impl::log2<size_t>(ali) << EX_ALIGNED1) 
            };
        };

    protected:
        // Ctor
        template<typename EX>
        vector_base(size_type bl, EX) noexcept
            : vector_base(bl, static_cast<uint16_t>(EX::value)) {}
        // Ctor
        vector_base(size_type bl) noexcept
            : vector_base(bl, extra_t<alignof(void*), 0, 0>{}) {}
    protected:
        // Dtor
        ~vector_base() noexcept { this->free_memory(); }
        // Copy ctor (may fail, check capacity() > 0 after construction)
        vector_base(const vector_base&) noexcept;
        // Move ctor
        vector_base(vector_base&&) noexcept;
        // Ctor
        vector_base(size_type bl, uint16_t ex) noexcept;
        // Resize (returns false on failure)
        bool resize(size_type n, const char* data) noexcept;
        // Shrink resize
        void shrink_resize(size_type n) noexcept;
        // Erase at pos range
        void erase(size_type start, size_type end) noexcept;
        // Insert range (returns false on failure)
        bool insert(size_type pos, const char* first, const char* last) noexcept;
        // Insert pos (returns false on failure)
        bool insert(size_type pos, size_type n, const char* data) noexcept;
        // Assign data (returns false on failure)
        bool assign(const char* first, const char* last) noexcept;
        // Assign data n times (returns false on failure)
        bool assign(size_type n, const char* data) noexcept;
        // Push back (returns false on failure)
        bool push_back(const char*) noexcept;
        // Push back pointer (returns false on failure)
        bool push_back_ptr(const char*) noexcept;
        // Swap another object
        void swap(vector_base& x) noexcept;
        // Begin pointer
        auto begin() noexcept -> void* { return m_pData; }
        // End pointer
        auto end() noexcept -> void* { return const_cast<void*>(static_cast<const vector_base&>(*this).end()); }
        // Const begin pointer
        auto begin() const noexcept -> const void* { return m_pData; }
        // Const end pointer
        auto end() const noexcept -> const void*;
        // Try operator= copy (returns false on failure)
        bool try_op_equal(const vector_base& x) noexcept;
        // Move operator= (swap implementation)
        void op_equal(vector_base&& x) noexcept { this->swap(x); }
        // Fit size
        void shrink_to_fit() noexcept;

    private:
        // Force reset
        inline void force_reset_set() noexcept;

    private:
        // Free memory
        static void free(char*) noexcept;
        // Alloc memory
        static auto malloc(size_type len) noexcept -> char*;
        // Realloc memory (avoids free+malloc)
        static auto realloc(char*, size_type len) noexcept -> char*;
        // Is valid heap data
        bool is_valid_heap(void* ptr) const noexcept { return ptr != (this + 1); }
        // Invalid heap data (SOO buffer)
        auto invalid_heap() const noexcept -> char* { return (char*)(this + 1); }
        // Free memory
        void free_memory() noexcept;
        // Alloc memory
        void alloc_memory(size_type len) noexcept;
        // Insert helper
        bool insert_helper(size_type pos, size_type n) noexcept;
        // Realloc memory helper (unified realloc logic)
        bool realloc_memory(size_type new_cap) noexcept;

    private:
        // Get aligned size
        auto get_extra_ali() const noexcept -> uint32_t {
            return 1 << ((m_uExtra >> EX_ALIGNED1) & ((1 << (EX_ALIGNED2 - EX_ALIGNED1)) - 1));
        }
        // Get extra buy
        auto get_extra_buy() const noexcept -> uint32_t {
            return (m_uExtra >> EX_RESERVED1) & ((1 << (EX_RESERVED2 - EX_RESERVED1)) - 1);
        }
        // Get extra SOO(small object optimization) capacity
        auto get_extra_soo() const noexcept -> uint32_t {
            return (m_uExtra >> EX_SOO1) & ((1 << (EX_SOO2 - EX_SOO1)) - 1);
        }

    private:
        // Vector length
        uint32_t            m_uVecLen = 0;
        // Vector capacity
        uint32_t            m_uVecCap = 0;
        // Data byte size
        const uint16_t      m_uByteLen;
        // Extra data (alignment, reserved, SOO capacity)
        const uint16_t      m_uExtra;
        // Data ptr
        char* m_pData = invalid_heap();
    };

    // Base iterator
    template<typename V, typename Ptr> 
    class base_iterator {
        // Self type
        using self = base_iterator;
    public:
        // C++ iterator traits
        using iterator_category = std::random_access_iterator_tag;
        // C++ iterator traits
        using difference_type = std::ptrdiff_t;
        // C++ iterator traits
        using pointer = Ptr;
        // C++ iterator traits
        using value_type = typename std::remove_pointer_t<pointer>;
        // C++ iterator traits
        using reference = typename std::add_lvalue_reference_t<value_type>;

#ifndef NDEBUG
    private:
        // Data debug
        void data_dbg() const noexcept { assert(m_vector && m_data == m_vector->data() && "invalid iterator"); }
        // Range debug
        void range_dbg() const noexcept { assert(m_vector && (*this) >= m_vector->begin() && (*this) <= m_vector->end() && "out of range"); }
        // Range debug
        void range_dbg_ex() const noexcept { assert(m_vector && (*this) >= m_vector->begin() && (*this) < m_vector->end() && "out of range"); }
        // Debug check
        void check_dbg() const noexcept { data_dbg(); range_dbg(); }
        // Debug check
        void check_dbg_ex() const noexcept { data_dbg(); range_dbg_ex(); }
    public:
        // Def ctor
        base_iterator() noexcept : m_vector((V*)(nullptr)), m_ptr(nullptr), m_data(nullptr) {};
        // Nor ctor
        base_iterator(V& v, Ptr ptr) noexcept : m_vector(&v), m_ptr(ptr), m_data(v.data()) {};
        // Copy ctor
        base_iterator(const self& x) noexcept = default;
        // ++operator
        auto operator++() noexcept -> self& { ++m_ptr; check_dbg(); return *this; }
        // --operator
        auto operator--() noexcept -> self& { --m_ptr; check_dbg(); return *this; }
        // operator*
        auto operator*() const noexcept -> reference { check_dbg_ex(); return *m_ptr; }
        // operator->
        auto operator->() const noexcept -> pointer { check_dbg_ex(); return m_ptr; }
        // operator +=
        auto operator+=(difference_type i) noexcept -> self& { m_ptr += i; check_dbg(); return *this; }
        // operator -=
        auto operator-=(difference_type i) noexcept -> self& { m_ptr -= i; check_dbg(); return *this; }
        // operator[]
        auto operator[](difference_type i) noexcept -> reference { auto itr = *this; itr += i; return *itr.m_ptr; }
#else
    public:
        // Def ctor
        base_iterator() noexcept : m_ptr(nullptr) {};
        // Nor ctor
        base_iterator(V& v, Ptr ptr) noexcept : m_ptr(ptr) {};
        // Copy ctor
        base_iterator(const self& x) noexcept = default;
        // ++operator
        auto operator++() noexcept -> self& { ++m_ptr; return *this; }
        // --operator
        auto operator--() noexcept -> self& { --m_ptr; return *this; }
        // operator*
        auto operator*() const noexcept -> reference { return *m_ptr; }
        // operator->
        auto operator->() const noexcept -> pointer { return m_ptr; }
        // operator +=
        auto operator+=(difference_type i) noexcept -> self& { m_ptr += i; return *this; }
        // operator -=
        auto operator-=(difference_type i) noexcept -> self& { m_ptr -= i; return *this; }
        // operator[]
        auto operator[](difference_type i) noexcept -> reference { return m_ptr[i]; }
#endif
        // operator=
        auto operator=(const self& x) noexcept -> self& { std::memcpy(this, &x, sizeof x); return *this; }
        // operator<
        bool operator<(const self& x) const noexcept { return m_ptr < x.m_ptr; }
        // operator>
        bool operator>(const self& x) const noexcept { return m_ptr > x.m_ptr; }
        // operator==
        bool operator==(const self& x) const noexcept { return m_ptr == x.m_ptr; }
        // operator!=
        bool operator!=(const self& x) const noexcept { return m_ptr != x.m_ptr; }
        // operator<=
        bool operator<=(const self& x) const noexcept { return m_ptr <= x.m_ptr; }
        // operator>=
        bool operator>=(const self& x) const noexcept { return m_ptr >= x.m_ptr; }
        // operator-
        auto operator-(const self& x) const noexcept -> difference_type { return m_ptr - x.m_ptr; }
        // operator +
        auto operator+(difference_type i) const noexcept -> self { self itr{ *this }; itr += i; return itr; }
        // operator -=
        auto operator-(difference_type i) const noexcept -> self { self itr{ *this }; itr -= i; return itr; }
        // operator++
        auto operator++(int) noexcept -> self { self itr{ *this }; ++(*this); return itr; }
        // operator--
        auto operator--(int) noexcept -> self { self itr{ *this }; --(*this); return itr; }
    private:
#ifndef NDEBUG
        // Vector data
        V* m_vector;
        // Data ptr
        const void* m_data;
#endif
        // Pointer
        pointer m_ptr;
    };

    // Push back helper
    template<size_t> struct push_back_helper {
        // Call push back
        template<typename T>
        static inline bool call(vector_base& obj, const T& data) noexcept {
            const auto ptr = reinterpret_cast<const char*>(&data);
            return obj.push_back(ptr);
        }
    };

    // Push back helper specialization for pointer size
    template<> struct push_back_helper<sizeof(void*)> {
        // Call push back
        template<typename T>
        static inline bool call(vector_base& obj, const T& data) noexcept {
            union { const char* ptr; T t; } union_data;
            union_data.t = data;
            return obj.push_back_ptr(union_data.ptr);
        }
    };
}}

namespace Slim { namespace pod {

    // Type-erased POD vector
    template<typename T> 
    class vector : protected impl::vector_base {
        // Type helper
        static inline auto tr(T* ptr) noexcept -> char* { return reinterpret_cast<char*>(ptr); }
        // Type helper
        static inline auto tr(const T* ptr) noexcept -> const char* { return reinterpret_cast<const char*>(ptr); }

    public:
        // Size type
        using size_type = vector_base::size_type;
        // Size of element
        auto size_of_element() const noexcept { return vector_base::size_of_element(); }
        // Max size
        auto max_size() const noexcept -> size_type { return vector_base::max_size(); }
        // Clear data
        void clear() noexcept { return vector_base::clear(); }
        // Pop back
        void pop_back() noexcept { return vector_base::pop_back(); }
        // Size of vector
        auto size() const noexcept -> size_type { return vector_base::size(); }
        // Get capacity
        auto capacity() const noexcept -> size_type { return vector_base::capacity(); }
        // Is empty?
        bool empty() const noexcept { return vector_base::empty(); }
        // Reserve space (returns false on failure)
        bool reserve(size_type n) noexcept { return vector_base::reserve(n); }
        // Swap
        void swap(vector<T>& x) noexcept { this->vector_base::swap(x); }

    protected:
        // Buffer ctor
        template<typename EX>
        vector(size_type /*ali*/, EX ex) noexcept : vector_base(sizeof(T), ex) {}

    public:
        // Iterator
        using iterator = impl::base_iterator<vector, T*>;
        // Const iterator
        using const_iterator = const impl::base_iterator<const vector, const T*>;
            
        // Check for POD-like (modern replacement for is_pod)
        static_assert(impl::is_pod_like_v<T>, "type T must be POD-like (trivially copyable and standard layout)");
        static_assert(alignof(T) <= alignof(uint64_t), "todo: > 64bit alignment");

        // Ctor
        vector() noexcept : vector_base(sizeof(T)) {}
        // Ctor with initializer_list
        vector(std::initializer_list<T> list) noexcept : vector_base(sizeof(T)) { assign(list); }
        // Copy ctor
        vector(const vector& v) noexcept : vector_base(v) {}
        // Move ctor
        vector(vector&& v) noexcept : vector_base(std::move(v)) {}
        // Range ctor with random access iterator
        template<typename RAI>
        vector(RAI first, RAI last) noexcept : vector_base(sizeof(T)) { assign(first, last); }
        // Fill 0 ctor
        vector(size_type n) noexcept : vector(n, T{}) {}
        // Fill x ctor
        vector(size_type n, const T& x) noexcept : vector_base(sizeof(T)) { assign(n, x); }
        // Copy op_equal (returns false on failure)
        auto&operator=(const vector& x) noexcept { vector_base::try_op_equal(x); return *this; }
        // Move op_equal
        auto operator=(vector&& x) noexcept -> vector& { vector_base::op_equal(std::move(x)); return *this; }
        // Try copy operator= (returns false on failure)
        bool try_op_equal(const vector& x) noexcept { return vector_base::try_op_equal(x); }
        // Get at
        auto at(size_type pos) noexcept -> T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // Get at const
        auto at(size_type pos) const noexcept -> const T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // operator[] 
        auto operator[](size_type pos) noexcept -> T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // operator[] const
        auto operator[](size_type pos) const noexcept -> const T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // Force base object
        auto force_base_object() noexcept -> impl::vector_base* { return this; }

    public:
        // Assign data (returns false on failure)
        bool assign(size_type n, const T& value) noexcept { return vector_base::assign(n, tr(&value)); }
        // Assign range with random access iterator (returns false on failure)
        template<typename RAI> 
        typename std::enable_if_t<impl::is_iterator<RAI>::value, bool>
        assign(RAI first, RAI last) noexcept {
#ifndef NDEBUG
            if (const auto n = last - first) {
                const auto ptr = &first[0];
                return vector_base::assign(tr(ptr), tr(ptr + n));
            }
            return true;
#else
            return vector_base::assign(tr(&first[0]), tr(&last[0]));
#endif
        }
        // Resize (returns false on failure)
        bool resize(size_type n) noexcept { return vector_base::resize(n, nullptr); }
        // Resize with filled-value (returns false on failure)
        bool resize(size_type n, const T& x) noexcept { return vector_base::resize(n, tr(&x)); }
        // Shrink resize
        void shrink_resize(size_type n) noexcept { vector_base::shrink_resize(n); }
        // Fit
        void shrink_to_fit() noexcept { vector_base::shrink_to_fit(); }
        // Get data ptr
        auto data() noexcept -> T* { return reinterpret_cast<T*>(vector_base::begin()); }
        // Get data ptr
        auto data() const noexcept -> const T* { return reinterpret_cast<const T*>(vector_base::begin()); }
        // Assign data (returns false on failure)
        bool assign(std::initializer_list<T> list) noexcept { return assign(list.begin(), list.end()); }
        // Push back (returns false on failure)
        bool push_back(const T& x) noexcept { return impl::push_back_helper<sizeof(T)>::call(*this, x); }
        // Begin iterator
        auto begin() noexcept -> iterator { return{ *this, reinterpret_cast<T*>(vector_base::begin()) }; }
        // End iterator
        auto end() noexcept -> iterator { return{ *this, reinterpret_cast<T*>(vector_base::end()) }; }
        // Front
        auto front() noexcept -> T& { return *reinterpret_cast<T*>(vector_base::begin()); }
        // Back
        auto back() noexcept -> T& { assert(!empty()); return reinterpret_cast<T*>(vector_base::end())[-1]; }
        // Begin iterator
        auto begin() const noexcept -> const_iterator { return{ *this, reinterpret_cast<const T*>(vector_base::begin()) }; }
        // End iterator
        auto end() const noexcept -> const_iterator { return{ *this, reinterpret_cast<const T*>(vector_base::end()) }; }
        // Const begin iterator
        auto cbegin() const noexcept -> const_iterator { return begin(); }
        // Const end iterator
        auto cend() const noexcept -> const_iterator { return end(); }
        // Const front
        auto front() const noexcept -> const T& { return *reinterpret_cast<const T*>(vector_base::begin()); }
        // Const back
        auto back() const noexcept -> const T& { assert(!empty()); return reinterpret_cast<const T*>(vector_base::end())[-1]; }

    public:
        // Insert value (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(size_type pos, const T& val) noexcept {
            const bool success = vector_base::insert(pos, tr(&val), tr(&val + 1));
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
        }
        // Insert range with random access iterator (returns pair<iterator, bool>)
        template<typename RAI> 
        typename std::enable_if_t<impl::is_iterator<RAI>::value, std::pair<iterator, bool>>
        insert(size_type pos, RAI first, RAI last) noexcept {
#ifndef NDEBUG
            if (const auto n = last - first) {
                const auto ptr = &first[0];
                const bool success = vector_base::insert(pos, tr(ptr), tr(ptr + n));
                return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
            }
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, true };
#else
            const bool success = vector_base::insert(pos, tr(&first[0]), tr(&last[0]));
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
#endif
        }
        // Insert value n times (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(size_type pos, size_type n, const T& val) noexcept {
            const bool success = vector_base::insert(pos, n, tr(&val));
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
        }
        // Insert value (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(iterator itr, const T& val) noexcept { return insert(itr - begin(), val); }
        // Insert value n times (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(iterator itr, size_type n, const T& val) noexcept { return insert(itr - begin(), n, val); }
        // Insert range with random access iterator (returns pair<iterator, bool>)
        template<typename RAI> 
        typename std::enable_if_t<impl::is_iterator<RAI>::value, std::pair<iterator, bool>>
        insert(iterator itr, RAI first, RAI last) noexcept { return insert(itr - begin(), first, last); }
        // Insert range (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(iterator itr, std::initializer_list<T> list) noexcept { return insert(itr - begin(), list.begin(), list.end()); }

    public:
        // Erase at pos 
        iterator erase(size_type pos) noexcept {
            vector_base::erase(pos, pos + 1);
            return{ *this, reinterpret_cast<T*>(vector_base::begin()) + pos };
        }
        // Erase range
        iterator erase(size_type first, size_type last) noexcept {
            vector_base::erase(first, last);
            return{ *this, reinterpret_cast<T*>(vector_base::begin()) + first };
        }
        // Erase at pos itr
        iterator erase(iterator itr) noexcept { return erase(itr - begin()); }
        // Erase at range itr
        iterator erase(iterator first, iterator last) noexcept { return erase(first - begin(), last - begin()); }
        // Erase at pos itr
        iterator erase(const_iterator itr) noexcept { return erase(itr - cbegin()); }
        // Erase at range itr
        iterator erase(const_iterator first, const_iterator last) noexcept { return erase(first - cbegin(), last - cbegin()); }
    };

    // Type-erased POD vector with Small Object Optimization (SOO)
    template<typename T, size_t SOO_LEN> 
    class alignas(alignof(T) > alignof(void*) ? alignof(T) : alignof(void*)) vector_soo : protected impl::vector_base{
        // Type helper
        static inline auto tr(T* ptr) noexcept -> char* { return reinterpret_cast<char*>(ptr); }
        // Type helper
        static inline auto tr(const T* ptr) noexcept -> const char* { return reinterpret_cast<const char*>(ptr); }

        T  soo[SOO_LEN];

    public:
        // Size type
        using size_type = vector_base::size_type;
        // Size of element
        auto size_of_element() const noexcept { return vector_base::size_of_element(); }
        // Max size
        auto max_size() const noexcept -> size_type { return vector_base::max_size(); }
        // Clear data
        void clear() noexcept { return vector_base::clear(); }
        // Pop back
        void pop_back() noexcept { return vector_base::pop_back(); }
        // Size of vector
        auto size() const noexcept -> size_type { return vector_base::size(); }
        // Get capacity
        auto capacity() const noexcept -> size_type { return vector_base::capacity(); }
        // Is empty?
        bool empty() const noexcept { return vector_base::empty(); }
        // Reserve space (returns false on failure)
        bool reserve(size_type n) noexcept { return vector_base::reserve(n); }
        // Swap
        void swap(vector_soo<T, SOO_LEN>& x) noexcept { this->vector_base::swap(x); }

    public:
        // Iterator
        using iterator = impl::base_iterator<vector_soo, T*>;
        // Const iterator
        using const_iterator = const impl::base_iterator<const vector_soo, const T*>;
            
        // Check for POD-like (modern replacement for is_pod)
        static_assert(impl::is_pod_like_v<T>, "type T must be POD-like (trivially copyable and standard layout)");
        static_assert(alignof(T) <= alignof(uint64_t), "todo: > 64bit alignment");
        static_assert(SOO_LEN < 256, "SOO_LEN must be less than 256");
        static_assert(SOO_LEN * sizeof(T) <= 1024, "SOO_LEN*T must be less than 1024");

        // Ctor
        vector_soo() noexcept : vector_base(sizeof(T), vector_base::extra_t<alignof(T), 0, SOO_LEN>{}) {}
        // Ctor with initializer_list
        vector_soo(std::initializer_list<T> list) noexcept : vector_base(sizeof(T), vector_base::extra_t<alignof(T), 0, SOO_LEN>{}) { assign(list); }
        // Copy ctor
        vector_soo(const vector_soo& v) noexcept : vector_base(v) {}
        // Move ctor
        vector_soo(vector_soo&& v) noexcept : vector_base(std::move(v)) {}
        // Range ctor with random access iterator
        template<typename RAI>
        vector_soo(RAI first, RAI last) noexcept : vector_base(sizeof(T), vector_base::extra_t<alignof(T), 0, SOO_LEN>{}) { assign(first, last); }
        // Fill 0 ctor
        vector_soo(size_type n) noexcept : vector_soo(n, T{}) {}
        // Fill x ctor
        vector_soo(size_type n, const T& x) noexcept : vector_base(sizeof(T), vector_base::extra_t<alignof(T), 0, SOO_LEN>{}) { assign(n, x); }
        // Copy operator=
        auto operator=(const vector_soo& x) noexcept -> vector_soo& { vector_base::try_op_equal(x); return *this; }
        // Move operator=
        auto operator=(vector_soo&& x) noexcept -> vector_soo& { vector_base::op_equal(std::move(x)); return *this; }
        // Try copy operator= (returns false on failure)
        bool try_op_equal(const vector_soo& x) noexcept { return vector_base::try_op_equal(x); }
        // Get at
        auto at(size_type pos) noexcept -> T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // Get at const
        auto at(size_type pos) const noexcept -> const T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // operator[] 
        auto operator[](size_type pos) noexcept -> T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // operator[] const
        auto operator[](size_type pos) const noexcept -> const T& { assert(pos < size() && "OOR"); return data()[pos]; }
        // Force base object
        auto force_base_object() noexcept -> impl::vector_base* { return this; }

    public:
        // Assign data (returns false on failure)
        bool assign(size_type n, const T& value) noexcept { return vector_base::assign(n, tr(&value)); }
        // Assign range with random access iterator (returns false on failure)
        template<typename RAI> 
        typename std::enable_if_t<impl::is_iterator<RAI>::value, bool>
        assign(RAI first, RAI last) noexcept {
#ifndef NDEBUG
            if (const auto n = last - first) {
                const auto ptr = &first[0];
                return vector_base::assign(tr(ptr), tr(ptr + n));
            }
            return true;
#else
            return vector_base::assign(tr(&first[0]), tr(&last[0]));
#endif
        }
        // Resize (returns false on failure)
        bool resize(size_type n) noexcept { return vector_base::resize(n, nullptr); }
        // Resize with filled-value (returns false on failure)
        bool resize(size_type n, const T& x) noexcept { return vector_base::resize(n, tr(&x)); }
        // Shrink resize
        void shrink_resize(size_type n) noexcept { vector_base::shrink_resize(n); }
        // Fit
        void shrink_to_fit() noexcept { vector_base::shrink_to_fit(); }
        // Get data ptr
        auto data() noexcept -> T* { return reinterpret_cast<T*>(vector_base::begin()); }
        // Get data ptr
        auto data() const noexcept -> const T* { return reinterpret_cast<const T*>(vector_base::begin()); }
        // Assign data (returns false on failure)
        bool assign(std::initializer_list<T> list) noexcept { return assign(list.begin(), list.end()); }
        // Push back (returns false on failure)
        bool push_back(const T& x) noexcept { return impl::push_back_helper<sizeof(T)>::call(*this, x); }
        // Begin iterator
        auto begin() noexcept -> iterator { return{ *this, reinterpret_cast<T*>(vector_base::begin()) }; }
        // End iterator
        auto end() noexcept -> iterator { return{ *this, reinterpret_cast<T*>(vector_base::end()) }; }
        // Front
        auto front() noexcept -> T& { return *reinterpret_cast<T*>(vector_base::begin()); }
        // Back
        auto back() noexcept -> T& { assert(!empty()); return reinterpret_cast<T*>(vector_base::end())[-1]; }
        // Begin iterator
        auto begin() const noexcept -> const_iterator { return{ *this, reinterpret_cast<const T*>(vector_base::begin()) }; }
        // End iterator
        auto end() const noexcept -> const_iterator { return{ *this, reinterpret_cast<const T*>(vector_base::end()) }; }
        // Const begin iterator
        auto cbegin() const noexcept -> const_iterator { return begin(); }
        // Const end iterator
        auto cend() const noexcept -> const_iterator { return end(); }
        // Const front
        auto front() const noexcept -> const T& { return *reinterpret_cast<const T*>(vector_base::begin()); }
        // Const back
        auto back() const noexcept -> const T& { assert(!empty()); return reinterpret_cast<const T*>(vector_base::end())[-1]; }

    public:
        // Insert value (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(size_type pos, const T& val) noexcept {
            const bool success = vector_base::insert(pos, tr(&val), tr(&val + 1));
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
        }
        // Insert range with random access iterator (returns pair<iterator, bool>)
        template<typename RAI> 
        typename std::enable_if_t<impl::is_iterator<RAI>::value, std::pair<iterator, bool>>
        insert(size_type pos, RAI first, RAI last) noexcept {
#ifndef NDEBUG
            if (const auto n = last - first) {
                const auto ptr = &first[0];
                const bool success = vector_base::insert(pos, tr(ptr), tr(ptr + n));
                return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
            }
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, true };
#else
            const bool success = vector_base::insert(pos, tr(&first[0]), tr(&last[0]));
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
#endif
        }
        // Insert value n times (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(size_type pos, size_type n, const T& val) noexcept {
            const bool success = vector_base::insert(pos, n, tr(&val));
            return{ { *this, reinterpret_cast<T*>(vector_base::begin()) + pos }, success };
        }
        // Insert value (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(iterator itr, const T& val) noexcept { return insert(itr - begin(), val); }
        // Insert value n times (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(iterator itr, size_type n, const T& val) noexcept { return insert(itr - begin(), n, val); }
        // Insert range with random access iterator (returns pair<iterator, bool>)
        template<typename RAI> 
        typename std::enable_if_t<impl::is_iterator<RAI>::value, std::pair<iterator, bool>>
        insert(iterator itr, RAI first, RAI last) noexcept { return insert(itr - begin(), first, last); }
        // Insert range (returns pair<iterator, bool>)
        std::pair<iterator, bool> insert(iterator itr, std::initializer_list<T> list) noexcept { return insert(itr - begin(), list.begin(), list.end()); }

    public:
        // Erase at pos 
        iterator erase(size_type pos) noexcept {
            vector_base::erase(pos, pos + 1);
            return{ *this, reinterpret_cast<T*>(vector_base::begin()) + pos };
        }
        // Erase range
        iterator erase(size_type first, size_type last) noexcept {
            vector_base::erase(first, last);
            return{ *this, reinterpret_cast<T*>(vector_base::begin()) + first };
        }
        // Erase at pos itr
        iterator erase(iterator itr) noexcept { return erase(itr - begin()); }
        // Erase at range itr
        iterator erase(iterator first, iterator last) noexcept { return erase(first - begin(), last - begin()); }
        // Erase at pos itr
        iterator erase(const_iterator itr) noexcept { return erase(itr - cbegin()); }
        // Erase at range itr
        iterator erase(const_iterator first, const_iterator last) noexcept { return erase(first - cbegin(), last - cbegin()); }
    };



}}
