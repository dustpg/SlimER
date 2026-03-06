#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "er_pod_vector.h"

#ifndef DRHI_NOINLINE
#ifdef _MSC_VER
#define DRHI_NOINLINE __declspec(noinline)
#else
#define DRHI_NOINLINE
#endif
#endif

using namespace Slim::impl;


// base_class
#include "er_helper_p.h"


DRHI_NOINLINE
/// <summary>
/// Copy constructor. Initializes a new instance by copying data from another vector_base.
/// </summary>
/// <param name="x">The source vector_base to copy from.</param>
vector_base::vector_base(const vector_base& x) noexcept :
    m_uByteLen(x.m_uByteLen),
    m_uVecCap(x.get_extra_soo()),
    m_uVecLen(x.m_uVecLen),
    m_uExtra(x.m_uExtra) {
    // 没有必要?
    if (!m_uVecLen) {
        m_uVecCap = 0;
        return;
    }
    // 申请对应大小内存
    this->alloc_memory(m_uVecLen);
    if (!m_uVecCap) {
        // 内存分配失败, 恢复到初始状态(异常安全)
        m_uVecCap = this->get_extra_soo();
        m_uVecLen = 0;
        return;
    }
    const auto cpylen = m_uVecLen + get_extra_buy();
    std::memcpy(m_pData, x.m_pData, cpylen * m_uByteLen);
#ifndef NDEBUG
    if (this->get_extra_buy()) {
        // Reserved space for future use
    }
#endif
}

/// <summary>
/// Forces the vector to reset to initial state, setting data pointer to invalid heap and clearing length.
/// </summary>
inline void vector_base::force_reset_set() noexcept {
    m_pData = this->invalid_heap();
    m_uVecCap = this->get_extra_soo();
    m_uVecLen = 0;
}


DRHI_NOINLINE
/// <summary>
/// Move constructor. Initializes a new instance by moving data from another vector_base.
/// </summary>
/// <param name="x">The source vector_base to move from.</param>
vector_base::vector_base(vector_base&& x) noexcept :
    m_pData(x.m_pData),
    m_uByteLen(x.m_uByteLen),
    m_uVecCap(x.m_uVecCap),
    m_uVecLen(x.m_uVecLen),
    m_uExtra(x.m_uExtra) {
    assert(&x != this && "can not move same object");
    // 对面有效堆数据
    if (x.is_valid_heap(x.m_pData)) {
        x.force_reset_set();
    }
    // 自己也必须是缓存数据 (SOO)
    else {
        m_pData = this->invalid_heap();
        const auto cpylen = m_uVecLen + get_extra_buy();
        std::memcpy(m_pData, x.m_pData, cpylen * m_uByteLen);
    }
    // 长度置为0
    x.m_uVecLen = 0;
}

DRHI_NOINLINE
/// <summary>
/// Constructor. Initializes a new instance with specified byte length per element and extra data.
/// </summary>
/// <param name="bl">The byte length for a single object.</param>
/// <param name="ex">The extra data to store.</param>
vector_base::vector_base(
    size_type bl, uint16_t ex) noexcept :
    m_uByteLen(static_cast<uint16_t>(bl)),
    m_uExtra(ex) {
    m_uVecCap = this->get_extra_soo();
    assert(this->get_extra_ali() != 0);
    assert(this->get_extra_ali() <= alignof(void*) && "alignment must not exceed pointer alignment");
}

/// <summary>
/// Allocates memory of the specified length using std::malloc.
/// </summary>
/// <param name="len">The number of bytes to allocate.</param>
/// <returns>Pointer to allocated memory, or nullptr on failure.</returns>
inline auto vector_base::malloc(size_type len) noexcept -> char* {
    assert(len && "cannot malloc 0 length");
    return reinterpret_cast<char*>(base_class::imalloc(len));
    //return reinterpret_cast<char*>(std::malloc(len));
}

/// <summary>
/// Frees memory previously allocated by malloc.
/// </summary>
/// <param name="ptr">Pointer to the memory to free.</param>
inline void vector_base::free(char* ptr) noexcept {
    return base_class::ifree(ptr);
    //return std::free(ptr);
}

/// <summary>
/// Reallocates memory to a new size, avoiding free+malloc overhead.
/// </summary>
/// <param name="ptr">Pointer to existing memory, or nullptr for new allocation.</param>
/// <param name="len">The new number of bytes to allocate.</param>
/// <returns>Pointer to reallocated memory, or nullptr on failure.</returns>
inline auto vector_base::realloc(char* ptr, size_type len) noexcept -> char* {
    assert((ptr || len) && "cannot realloc nullptr to 0");
    return reinterpret_cast<char*>(base_class::irealloc(ptr, len));
    //return reinterpret_cast<char*>(std::realloc(ptr, len));
}

/// <summary>
/// Returns a pointer to the end of the vector (one past the last element).
/// </summary>
/// <returns>Pointer to the end of the vector data.</returns>
auto vector_base::end() const noexcept -> const void* {
    return m_pData + m_uByteLen * m_uVecLen;
}

DRHI_NOINLINE
/// <summary>
/// Erases elements in the range [start, end) from the vector.
/// </summary>
/// <param name="start">The starting index of the range to erase.</param>
/// <param name="end">The ending index of the range to erase (exclusive).</param>
void vector_base::erase(size_type start, size_type end) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    assert(start <= size() && "erase:[start] out of range");
    end = std::min(end, size());
    // 移动内存
    if (end != m_uVecLen) {
        // 剩余的长度
        const auto remain = (m_uVecLen - end) * m_uByteLen;
        const auto wptr = m_pData + start * m_uByteLen;
        const auto rptr = m_pData + end * m_uByteLen;
        std::memmove(wptr, rptr, remain);
    }
    // 直接减少长度即可
    m_uVecLen -= static_cast<uint32_t>(end - start);
}

DRHI_NOINLINE
/// <summary>
/// Frees the heap memory if it was allocated, otherwise does nothing for SOO (Small Object Optimization) case.
/// </summary>
void vector_base::free_memory() noexcept {
    if (this->is_valid_heap(m_pData)) this->free(m_pData);
#ifndef NDEBUG
    m_pData = nullptr; m_pData = this->invalid_heap();
#endif
}

DRHI_NOINLINE
/// <summary>
/// Allocates memory for the specified number of elements. Does not modify m_uVecLen.
/// </summary>
/// <param name="len">The number of elements to allocate memory for.</param>
/// <remarks>This function does not modify m_uVecLen.</remarks>
void vector_base::alloc_memory(size_type len) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
#ifndef NDEBUG
    assert(!is_valid_heap(m_pData) && "free memory first");
#endif
    // 额外内存
    const auto cap = len + this->get_extra_buy();
    const auto bytelen = m_uByteLen * cap;
    // 申请内存
    if (const auto ptr = this->malloc(bytelen)) {
        m_pData = ptr;
        m_uVecCap = static_cast<uint32_t>(cap);
    }
    // 内存不足: 保持调用前的状态(异常安全)
    // 不修改任何成员变量
}

DRHI_NOINLINE
/// <summary>
/// Reallocates memory to a new capacity using unified logic. Handles both heap and SOO cases.
/// </summary>
/// <param name="new_cap">The new capacity to allocate.</param>
/// <returns>true on success, false on failure (maintains original state on failure).</returns>
bool vector_base::realloc_memory(size_type new_cap) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    const auto bytelen = new_cap * m_uByteLen;
    // 有效堆: 使用realloc扩容 (避免先free再malloc)
    const auto ptr = this->is_valid_heap(m_pData) ? m_pData : nullptr;
    // 使用realloc扩容
    if (const auto data = this->realloc(ptr, bytelen)) {
        // 无效堆(SOO): 复制旧数据
        m_pData = data;
        if (!ptr) {
            // SOO情况：从invalid_heap复制数据到新分配的内存
            std::memcpy(m_pData, this->invalid_heap(), m_uVecLen * m_uByteLen);
        }
        m_uVecCap = static_cast<uint32_t>(new_cap);
        return true;
    }
    // 内存不足: realloc失败时原指针仍然有效(对于堆内存), 或保持原状态(对于SOO)
    // 不修改任何成员变量, 保持原状态(异常安全)
    return false;
}

DRHI_NOINLINE
/// <summary>
/// Swaps the contents of this vector with another vector.
/// </summary>
/// <param name="x">The other vector_base to swap with.</param>
void vector_base::swap(vector_base& x) noexcept {
    assert(&x != this && "cannot swap this");
    assert(m_uByteLen == x.m_uByteLen);
    assert(m_uExtra == x.m_uExtra);
    std::swap(m_uVecLen, x.m_uVecLen);
    std::swap(m_uVecCap, x.m_uVecCap);
    std::swap(m_pData, x.m_pData);
    char sooBuffer[1024];
    const auto sooSize = this->get_extra_soo() * uint32_t(m_uByteLen);
    assert(sooSize <= sizeof(sooBuffer));
    // x-soo --> sooBuffer
    std::memcpy(sooBuffer, x.invalid_heap(), sooSize);
    // this-soo -->  x-soo
    std::memcpy(x.invalid_heap(), this->invalid_heap(), sooSize);
    // sooBuffer -->  this-soo
    std::memcpy(this->invalid_heap(), sooBuffer, sooSize);
    if (m_pData == x.invalid_heap()) m_pData = this->invalid_heap();
    if (x.m_pData == invalid_heap()) x.m_pData = x.invalid_heap();

}


DRHI_NOINLINE
/// <summary>
/// Attempts to copy assign from another vector. Returns false on failure without modifying state.
/// </summary>
/// <param name="x">The source vector_base to copy from.</param>
/// <returns>true on success, false on failure (maintains original state on failure).</returns>
bool vector_base::try_op_equal(const vector_base& x) noexcept {
    // 容量允许时
    if (capacity() >= x.size() + get_extra_buy()) {
        m_uVecLen = x.m_uVecLen;
        const auto cpylen = m_uVecLen + get_extra_buy();
        std::memmove(m_pData, x.m_pData, cpylen * m_uByteLen);
        return true;
    }
    // 需要扩容: 优先使用realloc保证内存不足时异常安全
    else {
        const auto new_cap = x.size() + this->get_extra_buy();
        // 使用统一的realloc逻辑
        if (this->realloc_memory(new_cap)) {
            m_uVecLen = x.m_uVecLen;
            const auto cpylen = m_uVecLen + get_extra_buy();
            std::memmove(m_pData, x.m_pData, cpylen * m_uByteLen);
            return true;
        }
        // 内存不足: realloc失败时原指针仍然有效(对于堆内存), 或保持原状态(对于SOO)
        // 不修改任何成员变量, 保持原状态(异常安全)
        return false;
    }
}

DRHI_NOINLINE
/// <summary>
/// Assigns data from a range [first, last) to the vector, replacing existing content.
/// </summary>
/// <param name="first">Pointer to the first element of the range.</param>
/// <param name="last">Pointer to one past the last element of the range.</param>
/// <returns>true on success, false on failure (restores original state on failure).</returns>
bool vector_base::assign(const char* first, const char* last) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    const auto old_len = m_uVecLen;
    const auto old_cap = m_uVecCap;
    const auto old_data = m_pData;
    
    this->clear();
    if (!first) return true;

    assert(last >= first && "bad data");
    const auto len = (last - first) / m_uByteLen;
    m_uVecLen = static_cast<uint32_t>(len);
    assert(m_uVecLen * m_uByteLen == std::uintptr_t(last - first) && "bad align");

    // 超过容器当前上限
    if (m_uVecLen > m_uVecCap) {
        // 拥有额外数据的必须自己保证
        assert(this->get_extra_buy() == 0 && "you must call reserve before, if ex-buy");
        // 优先使用realloc保证内存不足时异常安全
        const auto new_cap = m_uVecLen + this->get_extra_buy();
        // 使用统一的realloc逻辑
        if (!this->realloc_memory(new_cap)) {
            // 内存不足: 恢复原状态
            m_uVecLen = old_len;
            m_uVecCap = old_cap;
            m_pData = old_data;
            return false;
        }
    }
    // 数据有效
    std::memmove(m_pData, first, last - first);
    return true;
}

DRHI_NOINLINE
/// <summary>
/// Assigns n copies of the specified data element to the vector, replacing existing content.
/// </summary>
/// <param name="n">The number of elements to assign.</param>
/// <param name="data">Pointer to the data element to copy n times.</param>
/// <returns>true on success, false on failure (restores original state on failure).</returns>
bool vector_base::assign(size_type n, const char* data) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    const auto old_len = m_uVecLen;
    const auto old_cap = m_uVecCap;
    const auto old_data = m_pData;
    
    this->clear(); if (!n) return true;
    assert(data && "bad data");
    m_uVecLen = static_cast<uint32_t>(n);
    // 超过容器当前上限
    if (n > m_uVecCap) {
        // 拥有额外数据的必须自己保证
        assert(this->get_extra_buy() == 0 && "you must call reserve before, if ex-buy");
        // 优先使用realloc保证内存不足时异常安全
        const auto new_cap = n + this->get_extra_buy();
        // 使用统一的realloc逻辑
        if (!this->realloc_memory(new_cap)) {
            // 内存不足: 恢复原状态
            m_uVecLen = old_len;
            m_uVecCap = old_cap;
            m_pData = old_data;
            return false;
        }
    }
    // 数据有效
    auto address = m_pData;
    const auto step = m_uByteLen;
    for (size_type i = 0; i != n; ++i) {
        std::memcpy(address, data, step);
        address += step;
    }
    return true;
}

DRHI_NOINLINE
/// <summary>
/// Appends a single element to the end of the vector.
/// </summary>
/// <param name="data">Pointer to the data element to append.</param>
/// <returns>true on success, false on failure (maintains original state on failure).</returns>
bool vector_base::push_back(const char* data) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    // 重新申请空间
    if (m_uVecLen == m_uVecCap) {
        // 分配策略: 在reserve中实现
        if (!this->reserve(m_uVecLen + 1)) {
            // 内存不足, 保持原状态
            return false;
        }
    }
    assert(m_uVecLen <= m_uVecCap && "bad case");
    // 写入数据
    const auto write_ptr = m_pData + m_uByteLen * m_uVecLen;
    std::memcpy(write_ptr, data, m_uByteLen);
    // 增加了
    ++m_uVecLen;
    assert(size() <= max_size());
    return true;
}

DRHI_NOINLINE
/// <summary>
/// Appends a pointer value to the end of the vector. Specialized version for pointer-sized elements.
/// </summary>
/// <param name="ptr">The pointer value to append.</param>
/// <returns>true on success, false on failure (maintains original state on failure).</returns>
bool vector_base::push_back_ptr(const char* ptr) noexcept {
    assert(m_uByteLen == sizeof(void*) && "m_uByteLen cannot be ptr");
    // 重新申请空间
    if (m_uVecLen == m_uVecCap) {
        // 分配策略: 在reserve中实现
        if (!this->reserve(m_uVecLen + 1)) {
            // 内存不足, 保持原状态
            return false;
        }
    }
    // 写入数据
    const auto write_ptr = m_pData + m_uByteLen * m_uVecLen;
    reinterpret_cast<const char**>(write_ptr)[0] = ptr;
    // 增加了
    ++m_uVecLen;
    assert(size() <= max_size());
    return true;
}

/// <summary>
/// Reduces the size of the vector to n elements. Does not reallocate memory.
/// </summary>
/// <param name="n">The new size (must be less than or equal to current size).</param>
void vector_base::shrink_resize(size_type n) noexcept {
    assert(n <= size() && "must be less or same");
    m_uVecLen = static_cast<uint32_t>(n);
    assert(size() <= max_size());
}

DRHI_NOINLINE
/// <summary>
/// Resizes the specified n.
/// </summary>
/// <param name="n">The n.</param>
/// <param name="data">The data.</param>
/// <returns>true on success, false on failure</returns>
bool vector_base::resize(size_type n, const char* data) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    if (n > size()) {
        if (!this->reserve(n)) {
            // 内存分配失败, 保持原状态
            return false;
        }
        const auto start = m_pData + m_uByteLen * m_uVecLen;
        if (data) {
            auto write_ptr = start;
            const auto count = n - size();
            for (size_type i = 0; i != count; ++i) {
                std::memcpy(write_ptr, data, m_uByteLen);
                write_ptr += m_uByteLen;
            }
        }
        // 由于是POD-like, 所以默认值是0
        else std::memset(start, 0, (n - m_uVecLen) * m_uByteLen);
    }
    m_uVecLen = static_cast<uint32_t>(n);
    assert(size() <= max_size());
    return true;
}

DRHI_NOINLINE
/// <summary>
/// Reserves capacity for at least n elements. Uses realloc to avoid free+malloc overhead.
/// </summary>
/// <param name="n">The minimum number of elements to reserve capacity for.</param>
/// <returns>true on success, false on failure (maintains original state on failure).</returns>
bool vector_base::reserve(size_type n) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    assert(n < this->max_size() && "too huge");
    // 获取目标容量
    const auto cap = static_cast<uint32_t>(n + this->get_extra_buy());
    // 扩容: 不足就重新申请
    if (m_uVecCap < cap) {
        auto new_cap = cap;
        // 额外分配策略是加一半
        const auto target = m_uVecCap + m_uVecCap / 2;
        // 所以没有一半就加上一半
        if (new_cap < target) new_cap = target;
        // 最少是4
        if (new_cap < 4) new_cap = 4;
        // 使用统一的realloc逻辑
        if (this->realloc_memory(new_cap)) {
            return true;
        }
        // 内存不足: realloc失败时原指针仍然有效(对于堆内存), 或保持原状态(对于SOO)
        // 不修改任何成员变量, 保持原状态
        return false;
    }
    return true;
}

DRHI_NOINLINE
/// <summary>
/// Helper function for insert operations. Prepares space for n elements at position pos.
/// </summary>
/// <param name="pos">The position where elements will be inserted.</param>
/// <param name="n">The number of elements to make space for.</param>
/// <returns>true on success, false on failure.</returns>
bool vector_base::insert_helper(size_type pos, size_type n) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    // 越界操作
    assert(pos <= size() && "out of range");
    // 计算长度
    const size_type remain = m_uVecCap - m_uVecLen;
    // 长度不足则重新申请
    if (remain < n) {
        if (!this->reserve(size() + n)) {
            return false;
        }
    }
    // 数据插入中间?
    if (pos != size()) {
        const size_type S = m_uByteLen;
        const auto src_pos = m_pData + pos * S;
        const auto des_pos = src_pos + n * S;
        // 移动内存
        std::memmove(des_pos, src_pos, (size() - pos) * S);
    }
    // 增加容量
    m_uVecLen += static_cast<uint32_t>(n);
    assert(size() <= max_size());
    return true;
}

DRHI_NOINLINE
/// <summary>
/// Inserts a range of elements [first, last) at the specified position.
/// </summary>
/// <param name="pos">The position where elements will be inserted.</param>
/// <param name="first">Pointer to the first element of the range to insert.</param>
/// <param name="last">Pointer to one past the last element of the range to insert.</param>
/// <returns>true on success, false on failure.</returns>
bool vector_base::insert(
    size_type pos, const char* first, const char* last) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    // 空数据
    if (!first) return true;
    // 尾指针不允许空
    assert(last && "bad pointer");
    // 准备数据
    if (insert_helper(pos, static_cast<size_type>((last - first) / size_t(m_uByteLen)))) {
        std::memcpy(m_pData + pos * m_uByteLen, first, (last - first));
        return true;
    }
    return false;
}

DRHI_NOINLINE
/// <summary>
/// Inserts n copies of the specified data element at the specified position.
/// </summary>
/// <param name="pos">The position where elements will be inserted.</param>
/// <param name="n">The number of elements to insert.</param>
/// <param name="data">Pointer to the data element to copy n times.</param>
/// <returns>true on success, false on failure.</returns>
bool vector_base::insert(
    size_type pos, size_type n, const char* data) noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    // 这种情况不允许空对象
    assert(data && "bad data ptr");
    // 准备数据
    if (insert_helper(pos, n)) {
        const auto S = m_uByteLen;
        auto write_pos = m_pData + pos * S;
        for (size_type i = 0; i != n; ++i) {
            std::memcpy(write_pos, data, S);
            write_pos += S;
        }
        return true;
    }
    return false;
}

DRHI_NOINLINE
/// <summary>
/// Reduces the capacity to fit the current size, freeing unused memory.
/// </summary>
void vector_base::shrink_to_fit() noexcept {
    assert(m_uByteLen && "m_uByteLen cannot be 0");
    // 无效(SOO)
    if (!this->is_valid_heap(m_pData)) return;
    // 为空直接释放
    if (m_uVecLen == 0) {
        this->free(m_pData);
        this->force_reset_set();
        return;
    }
    // 因为使用realloc继续缩水
    const auto cap = m_uVecLen + this->get_extra_buy();
    // 所以只要有空间就可以缩
    if (cap < m_uVecCap) {
        // 使用统一的realloc逻辑(缩容时基本不会失败)
        this->realloc_memory(cap);
    }
}
