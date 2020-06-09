/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include "compiler/likely.h"
#include "buffer/buffer_debug.h"
#include "buffer/buffer_raw.h"
#include "buffer/buffer_create.h"

using namespace spec;

namespace spec::buffer {
void ptr::release() {
    /* This is also called for hypercominbed ptr_node.
     * After freeing the underlying raw space, *this
     * can become inaccessible as well.
     */
    auto* const cached_raw = std::exchange(m_raw, nullptr);
    if (cached_raw) {
        bdout << "ptr " << this << " release " << cached_raw << bendl;
        const bool last_reference =
            1 == cached_raw->nref.load(std::memory_order_acquire);
        if (likely(last_reference) || --cached_raw->nref == 0) {
            delete cached_raw;
        }
        m_raw = nullptr;
    }
}

ptr::ptr(unique_leakable_ptr<raw> pbraw)
    : m_raw(pbraw.release()),
      m_off(0),
      m_len(m_raw->get_len()) {
    m_raw->nref.store(1, std::memory_order_release);
    bdout << "ptr " << this << " get " << m_raw << bendl;
}
ptr::ptr(uint64_t len)
    : m_off(0), m_len(len) {
    m_raw = buffer::create(m_len).release();
    m_raw->nref.store(1, std::memory_order_release);
    bdout << "ptr " << this << " get " << m_raw << bendl;
}
ptr::ptr(const char* buf, uint64_t len)
    : m_off(0), m_len(len) {
    m_raw = buffer::copy(buf, m_len).release();
    m_raw->nref.store(1, std::memory_order_release);
    bdout << "ptr " << this << " get " << m_raw << bendl;
}
ptr::ptr(const ptr& bptr)
    : m_raw(bptr.m_raw), m_off(bptr.m_off), m_len(bptr.m_len) {
    if (m_raw) {
        m_raw->nref++;
        bdout << "ptr " << this << " get " << m_raw << bendl;
    }
}
ptr::ptr(ptr&& bptr) noexcept
    : m_raw(bptr.m_raw), m_off(bptr.m_off), m_len(bptr.m_len) {
    bptr.m_raw = nullptr;
    bptr.m_off = 0;
    bptr.m_len = 0;
}
ptr::ptr(const ptr& bptr, uint64_t offset, uint64_t len)
    : m_raw(bptr.m_raw), m_off(bptr.m_off + offset), m_len(len) {
    spec_assert(offset + len <= bptr.m_len);
    spec_assert(m_raw);
    m_raw->nref++;
    bdout << "ptr " << this << " get " << m_raw << bendl;
}
ptr::ptr(const ptr& bptr, unique_leakable_ptr<raw> pbraw)
    : m_raw(pbraw.release()),
      m_off(bptr.m_off),
      m_len(bptr.m_len) {
    m_raw->nref.store(1, std::memory_order_release);
    bdout << "ptr " << this << " get " << m_raw << bendl;
}
ptr& ptr::operator= (const ptr& bptr) {
    if(bptr.m_raw) {
        bptr.m_raw->nref++;
        bdout << "ptr " << this << " get " << m_raw << bendl;
    }
    raw* bpraw = bptr.m_raw;
    release();
    if (bpraw) {
        m_raw = bpraw;
        m_off = bptr.m_off;
        m_len = bptr.m_len;
    } else {
        m_off = 0;
        m_len = 0;
    }
    return *this;
}
ptr& ptr::operator= (ptr&& bptr) noexcept {
    release();
    if(bptr.m_raw) {
        m_raw = bptr.m_raw;
        m_off = bptr.m_off;
        m_len = bptr.m_len;
        bptr.m_raw = nullptr;
        bptr.m_off = 0;
        bptr.m_len = 0;
    } else {
        m_off = 0;
        m_len = 0;
    }
    return *this;
}

unique_leakable_ptr<raw> ptr::clone() {
    return m_raw->clone();
}
void ptr::swap(ptr& other) noexcept {
    auto* r = m_raw;
    auto o = m_off;
    auto l = m_len;

    m_raw = other.m_raw;
    m_off = other.m_off;
    m_len = other.m_len;

    other.m_raw = r;
    other.m_off = o;
    other.m_len = l;
}

int ptr::get_mempool_type() const {
    if (m_raw) {
        return m_raw->mempool_type_id;
    }
    return mempool::mempool_buffer_anon;
}
void ptr::reassign_to_mempool(int64_t mempool_type_index) {
    if (m_raw) {
        m_raw->reassign_to_mempool(mempool_type_index);
    }
}
void ptr::try_assign_to_mempool(int64_t mempool_type_index) {
    if (m_raw) {
        m_raw->try_assign_to_mempool(mempool_type_index);
    }
}

const char* ptr::c_str() const {
    spec_assert(m_raw);
    return m_raw->get_data() + m_off;
}
char* ptr::c_str() {
    spec_assert(m_raw);
    return m_raw->get_data() + m_off;
}
const char* ptr::end_c_str() const {
    spec_assert(m_raw);
    return m_raw->get_data() + m_off + m_len;
}
char* ptr::end_c_str() {
    spec_assert(m_raw);
    return m_raw->get_data() + m_off + m_len;
}

uint64_t ptr::unused_tail_length() const {
    if (m_raw) {
        return m_raw->get_len() - (m_off + m_len);
    } else {
        return 0;
    }
}
const char& ptr::operator[](uint64_t pos) const {
    spec_assert(m_raw);
    spec_assert(pos < m_len);
    return m_raw->get_data()[m_off + pos];
}
char& ptr::operator[](uint64_t pos) {
    spec_assert(m_raw);
    spec_assert(pos < m_len);
    return m_raw->get_data()[m_off + pos];
}

const char* ptr::raw_c_str() const {
    spec_assert(m_raw);
    return m_raw->get_data();
}
uint64_t ptr::raw_length() const {
    spec_assert(m_raw);
    return m_raw->get_len();
}
uint64_t ptr::raw_nref() const {
    spec_assert(m_raw);
    return m_raw->nref;
}

void ptr::copy_out(uint64_t offset, uint64_t len, char* dest) const {
    spec_assert(m_raw);
    if (offset + len > m_len) {
        throw end_of_buffer();
    }
    char* src = m_raw->get_data() + m_off + offset;
    maybe_inline_memcpy(dest, src, len, 8);
}
void ptr::copy_in(uint64_t offset, uint64_t len, const char* src, bool crc_reset){
    spec_assert(m_raw);
    spec_assert(offset <= m_len);
    spec_assert(offset + len <= m_len);
    char* dest = m_raw->get_data() + m_off + offset;
    if (crc_reset) {
        m_raw->invalidate_crc();
    }
    maybe_inline_memcpy(dest, src, len, 64);
}
uint64_t ptr::wasted() const {
    return m_raw->get_len() - m_len;
}

int ptr::cmp(const ptr& other) const {
    auto len = m_len < other.m_len ? m_len : other.m_len;
    if (len) {
        int rst = memcmp(c_str(), other.c_str(), len);
        if (rst) {
            return rst;
        }
    }
    if (m_len < other.m_len) {
        return -1;
    } else if(m_len > other.m_len) {
        return 1;
    } else {
        return 0;
    }
}
bool ptr::is_zero() const {
    return mem_is_zero(c_str(), m_len);
}

uint64_t ptr::append(char c) {
    spec_assert(m_raw);
    spec_assert(1 <= unused_tail_length());
    char* ptr = m_raw->get_data() + m_off + m_len;
    *ptr = c;
    m_len++;
    return m_off + m_len;
}
uint64_t ptr::append(const char*src, uint64_t len) {
    spec_assert(m_raw);
    spec_assert(len <= unused_tail_length());
    char* dst = m_raw->get_data() + m_off + m_len;
    maybe_inline_memcpy(dst, src, len, 32);
    m_len += len;
    return m_off + m_len;
}

uint64_t ptr::append_zeros(uint64_t len) {
    spec_assert(m_raw);
    spec_assert(len <= unused_tail_length());
    char* dst = m_raw->get_data() + m_off + m_len;
    memset(dst, 0, len);
    m_len += len;
    return m_off + m_len;
}
void ptr::zero(bool crc_reset) {
    if (crc_reset) {
        m_raw->invalidate_crc();
    }
    memset(c_str(), 0, m_len);
}
void ptr::zero(uint64_t offset, uint64_t len, bool crc_reset) {
    spec_assert(offset + len <= m_len);
    if (crc_reset) {
        m_raw->invalidate_crc();
    }
    memset(c_str() + offset, 0, len);
}

bool ptr_node::dispose_if_hypercombined(ptr_node* const delete_this) {
    const bool is_hypercombined = static_cast<void*>(delete_this) == \
                      static_cast<void*>(&delete_this->m_raw->bptr_storage);
    if (is_hypercombined) {
        spec_assert_always("hypercombing is currently disabled" == nullptr);
        delete_this->~ptr_node();
    }
    return is_hypercombined;
}

std::unique_ptr<ptr_node, ptr_node::disposer>
ptr_node::create_hypercombined(unique_leakable_ptr<raw> pbraw) {
    // #TODO: use placement new to create ptr_node on buffer::raw::bptr_storage.
    return std::unique_ptr<ptr_node, ptr_node::disposer>(
            new ptr_node(std::move(pbraw)));
}

ptr_node* ptr_node::cloner::operator()(const ptr_node& clone_this) {
    return new ptr_node(clone_this);
}

std::ostream& operator<<(std::ostream& out, const raw& braw) {
    return out << "buffer::raw(" << (void*)(braw.get_data()) << " len "
               << braw.get_len() << " nref " << braw.nref.load() << ")";
}

std::ostream& operator<<(std::ostream& out, const ptr& bptr) {
    if (bptr.have_raw()) {
        out << "buffer::ptr(" << bptr.offset() << "~" << bptr.length() << " "
            << (void*)bptr.c_str() << " in raw " << (void*)bptr.raw_c_str()
            << " len " << bptr.raw_length()
            << " nref " << bptr.raw_nref() << ")";
    } else {
        out << "buffer::ptr(" << bptr.offset() << "~" << bptr.length()
            << " no raw)";
    }
    return out;
}

} //namespace spec::buffer
