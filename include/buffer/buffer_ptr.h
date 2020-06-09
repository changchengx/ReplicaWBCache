/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_PTR_H
#define BUFFER_PTR_H

#include <type_traits>
#include "buffer_error.h"
#include "buffer_fwd.h"
#include "../unique_leakable_ptr.h"
#include "../page.h"
#include "../spec_assert/spec_assert.h"

namespace spec {

namespace buffer {

class list;
class raw;

extern spec::unique_leakable_ptr<raw> copy(const char* c, uint64_t len);
extern spec::unique_leakable_ptr<raw> create(uint64_t len);

class ptr_hook {
public:
    mutable ptr_hook* next;

    ptr_hook(): ptr_hook(nullptr) {
    }
    ptr_hook(ptr_hook* const next): next(next) {
    }
};

class ptr {
    friend class list;
protected:
    raw *m_raw;
    uint64_t m_off, m_len;

private:
    void release();

    template <bool is_const>
    class iterator_impl {
        friend ptr;
    private:
        const ptr* this_ptr;  // parent ptr
        const char* start;    // start position pointer in this_ptr->c_str()
        const char* cur_pos;  // current position pointer in this_ptr->c_str()
        const char* end_ptr;  // end position pointer in this_ptr->end_c_str()
        const bool deep_copy; // if true, do not allow shallow ptr copies

        iterator_impl(typename std::conditional_t<is_const, const ptr*, ptr*> p,
                      size_t offset, bool deep_copy)
            : this_ptr(p), start(p->c_str() + offset),
              cur_pos(start),
              end_ptr(p->end_c_str()),
              deep_copy(deep_copy) {
        }

    public:
        using pointer = typename std::conditional_t<is_const, const char*, char*>;
        pointer get_pos_add(size_t len) {
            auto rst = cur_pos;
            *this += len;
            return rst;
        }

        ptr get_ptr(size_t len) {
            if (deep_copy) {
                return buffer::copy(get_pos_add(len), len);
            } else {
                size_t off = cur_pos - this_ptr->c_str();
                *this += len;
                return ptr(*this_ptr, off, len);
            }
        }

        iterator_impl& operator+=(size_t len) {
            cur_pos += len;
            if (cur_pos > end_ptr) {
                throw end_of_buffer();
            }
            return *this;
        }

        const char *get_pos() {
            return cur_pos;
        }

        const char *get_end() {
            return end_ptr;
        }

        size_t get_offset() {
            return cur_pos - start;
        }

        bool end() const {
            return cur_pos == end_ptr;
        }
    };
public:
    using const_iterator = iterator_impl<true>;
    using iterator = iterator_impl<false>;

    ptr(): m_raw(nullptr), m_off(0), m_len(0) {
    }
    ptr(unique_leakable_ptr<raw> pbraw);
    ptr(uint64_t len);
    ptr(const char* d, uint64_t len);
    ptr(const ptr& bptr);
    ptr(ptr&& bptr) noexcept;
    ptr(const ptr& bptr, uint64_t offset, uint64_t len);
    ptr(const ptr& bptr, unique_leakable_ptr<raw> pbraw);
    ptr& operator= (const ptr& bptr);
    ptr& operator= (ptr&& bptr) noexcept;
    ~ptr() {
        release();
    }

    bool have_raw() const {
        return m_raw ? true : false;
    }
    bool is_partial() const {
        return have_raw() && (start() > 0 || end() < raw_length());
    }

    unique_leakable_ptr<raw> clone();
    void swap(ptr& other) noexcept;

    iterator begin(size_t offset = 0) {
        return iterator(this, offset, false);
    }
    const_iterator begin(size_t offset = 0) const {
        return const_iterator(this, offset, false);
    }
    const_iterator cbegin() const {
        return begin();
    }
    const_iterator begin_deep(size_t offset = 0) const {
        return const_iterator(this, offset, true);
    }

    bool is_aligned(uint64_t align) const {
        return ((uint64_t)c_str() & (align - 1)) == 0;
    }
    bool is_page_aligned() const {
        return is_aligned(SPEC_PAGE_SIZE);
    }
    bool is_n_align_sized(uint64_t align) const {
        return (length() % align) == 0;
    }
    bool is_n_page_sized() const {
        return is_n_align_sized(SPEC_PAGE_SIZE);
    }

    int get_mempool_type() const;
    void reassign_to_mempool(int64_t mempool_type_index);
    void try_assign_to_mempool(int64_t mempool_type_index);

    const char* c_str() const;
    char* c_str();
    const char* end_c_str() const;
    char* end_c_str();
    uint64_t length() const {
        return m_len;
    }
    uint64_t offset() const {
        return m_off;
    }
    uint64_t start() const {
        return m_off;
    }
    uint64_t end() const {
        return m_off + m_len;
    }
    uint64_t unused_tail_length() const;
    const char& operator[](uint64_t pos) const;
    char& operator[](uint64_t pos);

    const char* raw_c_str() const;
    uint64_t raw_length() const;
    uint64_t raw_nref() const;

    void copy_out(uint64_t offset, uint64_t len, char* dest) const;
    void copy_in(uint64_t offset, uint64_t len, const char* src, bool crc_reset = true);
    uint64_t wasted() const;

    int cmp(const ptr& other) const;
    bool is_zero() const;

    void set_offset(uint64_t offset) {
        spec_assert(raw_length() >= offset);
        m_off = offset;
    }
    void set_length(uint64_t len) {
        spec_assert(raw_length() >= len);
        m_len = len;
    }

    uint64_t append(char c);
    uint64_t append(const char*p, uint64_t len);
    uint64_t append(std::string_view s) {
        return append(s.data(), s.length());
    }
    void zero(bool crc_reset = true);
    void zero(uint64_t offset, uint64_t len, bool crc_reset = true);
    uint64_t append_zeros(uint64_t len);
};

class ptr_node: public ptr_hook, public ptr {
private:
    ptr_node(const ptr_node&) = default;

    template <typename... Args>
    ptr_node(Args&&... args): ptr(std::forward<Args>(args)...) {
    }

    ptr& operator= (const ptr& rhs) = delete;
    ptr& operator= (ptr&& rhs) noexcept = delete;
    ptr_node& operator= (const ptr_node& rhs) = delete;
    ptr_node& operator= (ptr_node&& rhs) noexcept = delete;
    void swap(ptr& other) noexcept = delete;
    void swap(ptr_node& other) noexcept = delete;

public:
    class cloner {
    public:
        ptr_node* operator()(const ptr_node& clone_this);
    };
    class disposer {
    public:
        void operator()(ptr_node* const delete_this) {
            if (!dispose_if_hypercombined(delete_this)) {
                delete delete_this;
            }
        }
    };

private:
    static bool dispose_if_hypercombined(ptr_node* delete_this);

    static std::unique_ptr<ptr_node, disposer>
    create_hypercombined(unique_leakable_ptr<raw> pbraw);

public:
    ~ptr_node() = default;

    static ptr_node* copy_hypercombined(const ptr_node& copy_this);

    static std::unique_ptr<ptr_node, disposer>
    create(unique_leakable_ptr<raw> pbraw) {
        return create_hypercombined(std::move(pbraw));
    }
    static std::unique_ptr<ptr_node, disposer>
    create(const uint64_t len) {
        return create_hypercombined(buffer::create(len));
    }

    template <typename... Args>
    static std::unique_ptr<ptr_node, disposer>
    create(Args&&... args) {
        return std::unique_ptr<ptr_node, disposer>(
                new ptr_node(std::forward<Args>(args)...));
    }
};

extern std::ostream& operator<<(std::ostream& out, const buffer_ptr& bptr);

} // namespace:buffer
} // namespace:spec
#endif // BUFFER_PTR_H
