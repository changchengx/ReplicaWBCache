/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

#include <iomanip>
#include <algorithm>

#include "compiler/likely.h"
#include "buffer/buffer_list.h"
#include "buffer/buffer_raw_combined.h"
#include "buffer/buffer_create.h"
#include "crc32/crc32c.h"
#include "compact_map.h"
#include "mempool/mempool.h"
#include "intarith.h"
#include "compat.h"
#include "safe_io.h"
#include "encode/armor.h"


using namespace spec;

namespace spec::buffer {

static spec::atomic<uint64_t> buffer_cached_crc{0};
static spec::atomic<uint64_t> buffer_cached_crc_adjusted{0};
static spec::atomic<uint64_t> buffer_missed_crc{0};

static bool buffer_track_crc = false;

void track_cached_crc(bool btrack) {
    buffer_track_crc = btrack;
}

uint64_t get_cached_crc() {
    return buffer_cached_crc;
}

uint64_t get_cached_crc_adjusted() {
    return buffer_cached_crc_adjusted;
}

uint64_t get_missed_crc() {
    return buffer_missed_crc;
}

template<bool is_const>
list::iterator_impl<is_const>::iterator_impl(const buffer_list::iterator& it)
    : iterator_impl(it.m_blist, it.m_a_off, it.m_list_it, it.m_r_off) {
}

template<bool is_const>
list::iterator_impl<is_const>::iterator_impl(blist_t *blist, uint64_t off)
    : m_blist(blist), m_list(&m_blist->_buffers),
      m_list_it(m_list->begin()), m_r_off(0), m_a_off(0) {
    *this += off;
}

template<bool is_const>
void list::iterator_impl<is_const>::seek(uint64_t off) {
    m_list_it = m_list->begin();
    m_a_off = 0;
    m_r_off = 0;
    *this += off;
}

template<bool is_const>
char list::iterator_impl<is_const>::operator*() const {
    if (m_list_it == m_list->end()) {
        throw end_of_buffer();
    }
    return (*m_list_it)[m_r_off];
}

template<bool is_const>
auto list::iterator_impl<is_const>::operator+= (uint64_t off)
-> iterator_impl& {
    m_r_off += off;
    while (m_list_it != m_list->end()) {
        if (m_r_off >= m_list_it->length()) {
            // skip this buffer
            m_r_off -= m_list_it->length();
            m_list_it++;
        } else {
            // somewhere in this buffer
            break;
        }
    }

    if (m_list_it == m_list->end() && m_r_off) {
        throw end_of_buffer();
    }
    m_a_off += off;
    return *this;
}

template<bool is_const>
auto list::iterator_impl<is_const>::operator++() ->iterator_impl& {
    if (m_list_it == m_list->end()) {
        throw end_of_buffer();
    }
    *this += 1;
    return *this;
}

template<bool is_const>
ptr list::iterator_impl<is_const>::get_current_ptr() const {
    if (m_list_it == m_list->end()) {
        throw end_of_buffer();
    }
    return ptr(*m_list_it, m_r_off, m_list_it->length() - m_r_off);
}

template<bool is_const>
bool list::iterator_impl<is_const>::is_pointing_same_raw(const ptr& other)
    const {
    if (m_list_it == m_list->end()) {
        throw end_of_buffer();
    }
    return m_list_it->m_raw == other.m_raw;
}

template<bool is_const>
void list::iterator_impl<is_const>::copy(uint64_t len, char *dest) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
    }
    while (len > 0) {
        if (m_list_it == m_list->end()) {
            throw end_of_buffer();
        }

        auto this_copy_len = m_list_it->length() - m_r_off;
        if (len < this_copy_len) {
            this_copy_len = len;
        }

        m_list_it->copy_out(m_r_off, this_copy_len, dest);

        dest += this_copy_len;
        len -= this_copy_len;
        *this += this_copy_len;
    }
}
template<bool is_const>
void list::iterator_impl<is_const>::copy_deep(uint64_t len, ptr &dest) {
    if (!len) {
        return;
    }

    if (m_list_it == m_list->end()) {
        throw end_of_buffer();
    }
    dest = create(len);
    copy(len, dest.c_str());
}
template<bool is_const>
void list::iterator_impl<is_const>::copy_shallow(uint64_t len, ptr &dest) {
    if (!len) {
        return;
    }
    if (m_list_it == m_list->end()) {
        throw end_of_buffer();
    }

    if ((m_list_it->length() - m_r_off) < len) {
        dest = create(len);
        copy(len, dest.c_str());
    } else {
        dest = ptr(*m_list_it, m_r_off, len);
        *this += len;
    }
}
template<bool is_const>
void list::iterator_impl<is_const>::copy(uint64_t len, list &dest) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
    }
    while (len > 0) {
        if (m_list_it == m_list->end()) {
            throw end_of_buffer();
        }

        auto this_copy_len = m_list_it->length() - m_r_off;
        if (len < this_copy_len) {
            this_copy_len = len;
        }
        dest.append(*m_list_it, m_r_off, this_copy_len);

        len -= this_copy_len;
        *this += this_copy_len;
    }
}
template<bool is_const>
void list::iterator_impl<is_const>::copy(uint64_t len, std::string &dest) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
    }
    while (len > 0) {
        if (m_list_it == m_list->end()) {
            throw end_of_buffer();
        }

        auto this_copy_len = m_list_it->length() - m_r_off;
        if (len < this_copy_len) {
            this_copy_len = len;
        }
        dest.append(m_list_it->c_str() + m_r_off, this_copy_len);

        len -= this_copy_len;
        *this += this_copy_len;
    }
}
template<bool is_const>
void list::iterator_impl<is_const>::copy_all(list &dest) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
    }

    while(true) {
        if (m_list_it == m_list->end()) {
            return;
        }

        auto this_copy_len = m_list_it->length() - m_r_off;
        dest.append(m_list_it->c_str() + m_r_off, this_copy_len);

        *this += this_copy_len;
    }
}

/* get a pointer of the current iterator position.
 *
 * return the number of bytes that can be read from
 * that position (up to max_req_size ).
 *
 * move ahead the iterator by that number of bytes.
 */
template<bool is_const>
uint64_t list::iterator_impl<is_const>::get_ptr_and_advance(
        uint64_t max_req_size, const char* *p) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
        if (m_list_it == m_list->end()) {
            return 0;
        }
    }

    *p = m_list_it->c_str() + m_r_off;
    auto rst = std::min(m_list_it->length() - m_r_off, max_req_size);
    m_r_off += rst;
    if (m_r_off == m_list_it->length()) {
        ++m_list_it;
        m_r_off = 0;
    }
    m_a_off += rst;
    return rst;
}

// calculate crc from iterator position
template<bool is_const>
uint32_t list::iterator_impl<is_const>::crc32c(size_t length, uint32_t crc) {
    length = std::min(length, get_remaining());
    while (length > 0) {
        const char *p = nullptr;
        auto len = get_ptr_and_advance(length, &p);
        crc = spec_crc32c(crc, (unsigned char*)p, len);
        length -= len;
    }
    return crc;
}

/* Explicitly instantiate only the needed iterator types, so it can hide
 * the details in this compilation unit without introducing unnecessary
 * link time dependencies.
 */
template class list::iterator_impl<true>;
template class list::iterator_impl<false>;

list::iterator::iterator(blist_t *blist, uint64_t off)
    : iterator_impl(blist, off) {
}
list::iterator::iterator(blist_t *blist, uint64_t a_off, list_iter_t it,
                         uint64_t r_off)
    : iterator_impl(blist, a_off, it, r_off) {
}

void list::iterator::copy_in(uint64_t len, const char* src, bool crc_reset) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
    }
    while (len > 0) {
        if (m_list_it == m_list->end()) {
            throw end_of_buffer();
        }

        auto this_copy_len = m_list_it->length() - m_r_off;
        if (len < this_copy_len) {
            this_copy_len = len;
        }
        m_list_it->copy_in(m_r_off, this_copy_len, src, crc_reset);
        src += this_copy_len;
        len -= this_copy_len;
        *this += this_copy_len;
    }
}
void list::iterator::copy_in(uint64_t len, const list& other) {
    if (m_list_it == m_list->end()) {
        seek(m_a_off);
    }

    auto left = len;
    for (const auto& node : other._buffers) {
        auto l = node.length();
        if (left < l) {
            l = left;
        }
        copy_in(l, node.c_str());
        left -= l;
        if (left == 0) {
            break;
        }
    }
}

ptr_node& list::refill_append_space(const uint64_t len) {
    /* make a new buffer:
     *  fill out a complete page,
     *  factoring in the raw_combined overhead.
     */
    auto need = round_up_to(len, sizeof(uint64_t)) +
                sizeof(raw_combined);
    auto alen = round_up_to(need, SPEC_BUFFER_ALLOC_UNIT) -
                sizeof(raw_combined);
    auto new_back = ptr_node::create(
            raw_combined::create(alen, 0, get_mempool_type()));
    new_back->set_length(0);
    _tail_pnode_cache = new_back.get();
    _buffers.push_back(*new_back.release());
    _num += 1;
    return _buffers.back();
}

uint64_t list::get_wasted_space() const {
    if (_num == 1) {
        return _buffers.back().wasted();
    }

    std::vector<const raw*> raw_vec;
    raw_vec.reserve(_num);
    for (const auto& node: _buffers) {
        raw_vec.push_back(node.m_raw);
    }
    std::sort(raw_vec.begin(), raw_vec.end());

    uint64_t total = 0;
    const raw* last = nullptr;
    for (const auto raw_it : raw_vec) {
        if (raw_it == last) {
            continue;
        }
        last = raw_it;
        total += raw_it->get_len();
    }

    /* If multiple buffers are sharing the same raw buffer and
     * they overlap with each other, the wasted space will be
     * underestimated.
     */
    if (total <= length()) {
        return 0;
    }
    return total - length();
}
int list::get_mempool_type() const {
    if (_buffers.empty()) {
        return mempool::mempool_buffer_anon;
    }
    return _buffers.back().get_mempool_type();
}
void list::reassign_to_mempool(int64_t mempool_type_index) {
    for (auto& node : _buffers) {
        node.m_raw->reassign_to_mempool(mempool_type_index);
    }
}
void list::try_assign_to_mempool(int64_t mempool_type_index) {
    for (auto& node : _buffers) {
        node.m_raw->try_assign_to_mempool(mempool_type_index);
    }
}

void list::swap(buffer_list& other) noexcept {
    std::swap(_len, other._len);
    std::swap(_num, other._num);
    std::swap(_tail_pnode_cache, other._tail_pnode_cache);
    _buffers.swap(other._buffers);
}

bool list::contents_equal(const buffer_list& other) const {
    if (length() != other.length()) {
        return false;
    }

    auto this_buffers_it = std::cbegin(_buffers);
    auto other_buffers_it = std::cbegin(other._buffers);
    uint64_t this_buffers_off = 0, other_buffers_off = 0;

    while (this_buffers_it != std::cend(_buffers)) {
        auto len = this_buffers_it->length() - this_buffers_off;
        if (len > other_buffers_it->length() - other_buffers_off) {
            len = other_buffers_it->length() - other_buffers_off;
        }
        if (memcmp(this_buffers_it->c_str() + this_buffers_off,
                   other_buffers_it->c_str() + other_buffers_off, len)
                   != 0) {
            return false;
        }
        this_buffers_off += len;
        other_buffers_off += len;
        if (this_buffers_off == this_buffers_it->length()) {
            this_buffers_off = 0;
            ++this_buffers_it;
        }

        if (other_buffers_off > other_buffers_it->length()) {
            other_buffers_off = 0;
            ++other_buffers_it;
        }
    }
    return true;
}
bool list::contents_equal(const void *other, uint64_t length) const {
    if (this->length() != length) {
        return false;
    }

    const auto* other_buf = reinterpret_cast<const char*>(other);
    for (const auto& bp : buffers()) {
        const auto round_length = std::min(length, bp.length());
        if (std::memcmp(bp.c_str(), other_buf, round_length) != 0) {
            return false;
        } else {
            length -= round_length;
            other_buf += round_length;
        }
        if (length == 0) {
            return true;
        }
    }
    return false;
}

bool list::is_provided_buffer(const char* dst) const {
    if (_buffers.empty()) {
        return false;
    }
    return (is_contiguous() && (_buffers.front().c_str() == dst));
}
bool list::is_aligned(uint64_t align) const {
    for( const auto& node : _buffers) {
        if (!node.is_aligned(align)) {
            return false;
        }
    }
    return true;
}
bool list::is_page_aligned() const {
    return is_aligned(SPEC_PAGE_SIZE);
}
bool list::is_n_align_sized(uint64_t align) const {
    for (const auto& node : _buffers) {
        if (!node.is_n_align_sized(align)) {
            return false;
        }
    }
    return true;
}
bool list::is_n_page_sized() const {
    return is_n_align_sized(SPEC_PAGE_SIZE);
}
bool list::is_aligned_size_and_memory(uint64_t align_size,
                                uint64_t align_memory) const {
    for (const auto& node : _buffers) {
        if (!node.is_aligned(align_memory)
             || !node.is_n_align_sized(align_size)) {
            return false;
        }
    }
    return true;
}

bool list::is_zero() const {
    for (const auto& node: _buffers) {
        if (!node.is_zero()) {
            return false;
        }
    }
    return true;
}

void list::zero() {
    for (auto&node : _buffers) {
        node.zero();
    }
}
void list::zero(uint64_t off, uint64_t len) {
    spec_assert(off + len <= _len);

    uint64_t pos = 0;
    for (auto& node : _buffers) {
        if (off + len <= pos) {
            // 'off'-- len --|
            //                 'pos'-- node.length() --|
            break;
        }
        if (node.length() == 0 || pos + node.length() <= off) {
            //                          'pos'-- len --|
            // 'pos'-- node.length() --|
            pos += node.length();
            continue;
        }

        if (pos >= off && pos + node.length() < off + len) {
            /* 'off'--------------- len -----------------|
             *       'pos'------ node.length() -------|
             */
            node.zero();
        } else if (pos >= off) {
            /* 'off'-------------- len -----------------|
             *       'pos'------------ node.length() ---------|
             */
            node.zero(0, off + len - pos);
        } else if (pos + node.length() <= off + len) {
            /*       'off'---------- len ---------------|
             * 'pos'---------node.length() --------|
             */
            node.zero(off - pos, node.length() - (off - pos));
        } else {
            /*       'off'---------- len --------------|
             * 'pos'------------node.length()-----------------|
             */
            node.zero(off - pos, len);
        }
        pos += node.length();
    }
}

bool list::is_contiguous() const {
    return _num <= 1;
}
void list::rebuild() {
    if (_len == 0) {
        _tail_pnode_cache = &always_empty_bptr;
        _buffers.clear_and_dispose();
        _num = 0;
        return;
    }
    if ((_len & ~(SPEC_PAGE_MASK)) == 0) {
        rebuild(ptr_node::create(buffer::create_page_aligned(_len)));
    } else {
        rebuild(ptr_node::create(buffer::create(_len)));
    }
}
void list::rebuild(std::unique_ptr<ptr_node, ptr_node::disposer> nb) {
    uint64_t pos = 0;
    for (auto& node : _buffers) {
        nb->copy_in(pos, node.length(), node.c_str(), false);
        pos += node.length();
    }
    _buffers.clear_and_dispose();
    if (likely(nb->length())) {
        _tail_pnode_cache = nb.get();
        _buffers.push_back(*nb.release());
        _num = 1;
    } else {
        _tail_pnode_cache = &always_empty_bptr;
        _num = 0;
    }
    invalidate_crc();
}
bool list::rebuild_aligned(uint64_t align) {
    return rebuild_aligned_size_and_memory(align, align);
}


bool list::rebuild_aligned_size_and_memory(uint64_t align_size,
                                     uint64_t align_memory,
                                     uint64_t max_buffers) {
    bool must_rebuild = false;

    if (max_buffers && _num > max_buffers &&
        _len > (max_buffers * align_size)) {
        align_size = round_up_to(round_up_to(_len, max_buffers) /
                                 max_buffers, align_size);
    }
    auto it = std::begin(_buffers);
    auto it_prev = _buffers.before_begin();
    while (it != std::end(_buffers)) {
        // keep anything that's already align and sized aligned
        if (it->is_aligned(align_memory) &&
            it->is_n_align_sized(align_size)) {
            it_prev = it;
            ++it;
            continue;
        }
        /* consolidate unaligned items before reaching at std::end(_buffers)
         * 1. if current iterator node(ptr, ptr->m_raw->get_data() + m_off)
         *    start address isn't aligend with align_memory
         * 2. if current iterator node(ptr, ptr->m_len) size isn't aligned
         *    with align_size
         * 3. if the total unaligned list length(cached inunaligned_list_len)
         *    isn't aligned with align_size.
         */
        buffer_list unaligned_list;
        uint64_t unaligned_list_len = 0;
        do {
            unaligned_list_len += it->length();
            // no need to reallocate, relinking is enough
            auto it_after = _buffers.erase_after(it_prev);
            _num -= 1;
            unaligned_list._buffers.push_back(*it);
            unaligned_list._len += it->length();
            unaligned_list._num += 1;
            it = it_after;
        } while (it != std::end(_buffers) &&
                  (!it->is_aligned(align_memory) ||
                   !it->is_n_align_sized(align_size) ||
                   (unaligned_list_len % align_size)));

        if (!(unaligned_list.is_contiguous() &&
              unaligned_list._buffers.front().is_aligned(align_memory))) {
            unaligned_list.rebuild(
                    ptr_node::create(
                        buffer::create_aligned(unaligned_list._len,
                                               align_memory)
                        ));
            must_rebuild = true;
        }
        _buffers.insert_after(it_prev,
             *ptr_node::create(unaligned_list._buffers.front()).release());
        _num += 1;
        ++it_prev;
    }
    return must_rebuild;
}
bool list::rebuild_page_aligned() {
    return rebuild_aligned(SPEC_PAGE_SIZE);
}

void list::reserve(uint64_t pre_alloc_size) {
    if (get_append_buffer_unused_tail_length() < pre_alloc_size) {
        auto bptr =
            ptr_node::create(buffer::create_page_aligned(pre_alloc_size));

        bptr->set_length(0);
        _tail_pnode_cache = bptr.get();
        _buffers.push_back(*bptr.release());
        _num += 1;
    }
}

void list::claim_append(buffer_list& other_blist) {
    // steal the other guy's buffers
    _len += other_blist._len;
    _num += other_blist._num;
    _buffers.splice_back(other_blist._buffers);
    other_blist.clear();
}
void list::claim_append(buffer_list&& rvalue_blist) {
    claim_append(rvalue_blist);
}

/* If the ptr points to same raw, and data space are
 * continous, it will remove one extra ptr node from
 * buffer list by using only one ptr node.
 */
void list::claim_append_piecewise(buffer_list& other_blist) {
    // steal the other guy's buffers
    for (const auto& node : other_blist.buffers()) {
        append(node, 0, node.length());
    }
    other_blist.clear();
}

void list::append(char c) {
    // get the possible space size that can be used to put content into
    // the existing append_buffer;
    auto gap = get_append_buffer_unused_tail_length();
    if (!gap) {
        auto bptr = ptr_node::create(
                raw_combined::create(SPEC_BUFFER_APPEND_SIZE, 0,
                                     get_mempool_type()));
        bptr->set_length(0);
        _tail_pnode_cache = bptr.get();
        _buffers.push_back(*bptr.release());
        _num += 1;
    } else if (unlikely(_tail_pnode_cache != &_buffers.back())) {
        auto bptr = ptr_node::create(*_tail_pnode_cache, _tail_pnode_cache->length(), 0);
        _tail_pnode_cache = bptr.get();
        _buffers.push_back(*bptr.release());
        _num += 1;
    }
    _tail_pnode_cache->append(c);
    _len++;
}
void list::append(const char *data, uint64_t len) {
    _len += len;

    const auto tail_pnode_unused_length = get_append_buffer_unused_tail_length();
    const auto first_append_len = std::min(len, tail_pnode_unused_length);
    if (first_append_len) {
        /* _buffers and _tail_pnode_cache can desynchronize when
         *    1) a new ptr(we don't own it) has been added into the _buffers
         *    2) _buffers has been emptied as a result of std::move or
         *       stolen by claim_append.
         */
        if (unlikely(_tail_pnode_cache != &_buffers.back())) {
            auto bptr = ptr_node::create(*_tail_pnode_cache,
                                         _tail_pnode_cache->length(), 0);
            _tail_pnode_cache = bptr.get();
            _buffers.push_back(*bptr.release());
            _num += 1;
        }
        _tail_pnode_cache->append(data, first_append_len);
    }

    const auto left_append_len = len - first_append_len;
    if (left_append_len) {
        auto& new_back = refill_append_space(left_append_len);
        new_back.append(data + first_append_len, left_append_len);
    }
}
void list::append(const ptr& bptr) {
    push_back(bptr);
}
void list::append(ptr&& bptr) {
    push_back(std::move(bptr));
}
void list::append(const ptr& bptr, uint64_t off, uint64_t len) {
    spec_assert(len + off <= bptr.length());
    if (!_buffers.empty()) {
        ptr& tail_bptr = _buffers.back();
        if (tail_bptr.m_raw == bptr.m_raw &&
            tail_bptr.end() == bptr.start() + off) {
            // contiguous with tail bptr
            tail_bptr.set_length(tail_bptr.length() + len);
            _len += len;
            return;
        }
    }
    // add new item to list
    _buffers.push_back(*ptr_node::create(bptr, off, len).release());
    _len += len;
    _num += 1;
}
void list::append(const buffer_list& blist) {
    _len += blist._len;
    _num += blist._num;
    for (const auto& node : blist._buffers) {
        /* auto unique_pnode = ptr_node::create(node);
         * auto* pnode = unique_pnode.release();
         * _buffers.push_back(*pnode);
         */
        _buffers.push_back(*ptr_node::create(node).release());
    }
}
void list::append(std::istream& in) {
    while (!in.eof()) {
        std::string s;
        getline(in, s);
        append(s.c_str(), s.length());
        if (s.length()) {
            append("\n", 1);
        }
    }
}

// reserve space(hole) to be used in future
list::contiguous_filler list::append_hole(uint64_t len) {
    _len += len;

    if (unlikely(get_append_buffer_unused_tail_length() < len)) {
        /* make a new buffer:
         *  fill out a complete page,
         *  factoring in the raw_combined overhead.
         */
        auto& new_back = refill_append_space(len);
        new_back.set_length(len);
        return {new_back.c_str()};
    } else if (unlikely(_tail_pnode_cache != &_buffers.back())) {
        auto bptr = ptr_node::create(*_tail_pnode_cache, _tail_pnode_cache->length(), 0);
        _tail_pnode_cache = bptr.get();
        _buffers.push_back(*bptr.release());
        _num += 1;
    }
    _tail_pnode_cache->set_length(_tail_pnode_cache->length() + len);
    return {_tail_pnode_cache->end_c_str() - len};
}
void list::prepend_zero(uint64_t len) {
    auto bptr = ptr_node::create(len);
    bptr->zero(false);
    _len += len;
    _num += 1;
    _buffers.push_front(*bptr.release());
}
void list::append_zero(uint64_t len) {
    _len += len;

    const auto tail_pnode_unused_length = get_append_buffer_unused_tail_length();
    const auto first_append_len = std::min(len, tail_pnode_unused_length);
    if (first_append_len) {
        if (unlikely(_tail_pnode_cache != &_buffers.back())) {
            auto bptr = ptr_node::create(*_tail_pnode_cache,
                                         _tail_pnode_cache->length(), 0);
            _tail_pnode_cache = bptr.get();
            _buffers.push_back(*bptr.release());
            _num += 1;
        }
        _tail_pnode_cache->append_zeros(first_append_len);
    }

    const auto left_append_len = len - first_append_len;
    if (left_append_len) {
        auto& new_back = refill_append_space(left_append_len);
        new_back.set_length(left_append_len);
        new_back.zero(false);
    }
}

list::reserve_t list::obtain_contiguous_space(const uint64_t len) {
    /* If len < (the normal append_buffer size), it might
     * be better to allocate a normal-sized append_buffer
     * and use part of it. However, that optimizes for the
     * case of old-style types including new-style types.
     * And, in most such cases, this won't be the very first
     * thing encoded to the list, so append_buffer will
     * already be allocated.
     * If everything is new-style, we should allocate only
     * what we need and conserve memory.
     */
    if (unlikely(get_append_buffer_unused_tail_length() < len)) {
        auto new_back =
            buffer::ptr_node::create(buffer::create(len)).release();
        new_back->set_length(0);
        _buffers.push_back(*new_back);
        _num += 1;
        _tail_pnode_cache = new_back;
        return {new_back->c_str(), &new_back->m_len, &_len};
    } else {
        if (unlikely(_tail_pnode_cache != &_buffers.back())) {
            auto bptr = ptr_node::create(*_tail_pnode_cache,
                                         _tail_pnode_cache->length(), 0);
            _tail_pnode_cache = bptr.get();
            _buffers.push_back(*bptr.release());
            _num += 1;
        }
        return {_tail_pnode_cache->end_c_str(), &_tail_pnode_cache->m_len, &_len};
    }
}

const char& list::operator[] (uint64_t pos) const {
    if (pos >= _len) {
        throw end_of_buffer();
    }

    for (const auto& node : _buffers) {
        if (pos >= node.length()) {
            pos -= node.length();
            continue;
        }
        return node[pos];
    }
    spec_abort();

    /* unreached code to remove warning:
     * warning: control reaches end of non-void function [-Wreturn-type]
     */
    return (*std::cbegin(_buffers))[0];
}
// return a contiguous ptr to the whole bufferlist contents.
char* list::c_str() {
    if (_buffers.empty()) {
        return nullptr;
    }

    auto iter = std::cbegin(_buffers);
    ++iter;

    if (iter != std::cend(_buffers)) {
        rebuild();
    }
    return _buffers.front().c_str();
}
std::string list::to_str() const {
    std::string s;
    s.reserve(length());
    for (const auto& node : _buffers) {
        if (node.length()) {
            s.append(node.c_str(), node.length());
        }
    }
    return s;
}

void list::substr_of(const list& other, uint64_t off, uint64_t len) {
    if (off + len > other.length()) {
        throw end_of_buffer();
    }
    clear();

    // skip off
    auto curbuf = std::cbegin(other._buffers);
    while (off > 0 && off >= curbuf->length()) {
        // skip this buffer
        off -= (*curbuf).length();
        ++curbuf;
    }
    spec_assert(len == 0 || curbuf != std::cend(other._buffers));

    while (len >= 0) {
        if (off + len < curbuf->length()) {
            // partial
            _buffers.push_back(
                    *ptr_node::create(*curbuf, off, len).release());
            _len += len;
            _num += 1;
            break;
        }

        // direct through the end
        auto create_len = curbuf->length() - off;
        _buffers.push_back(
                *ptr_node::create(*curbuf, off, create_len).release());
        _len += create_len;
        _num += 1;
        len -= create_len;
        off = 0;
        ++curbuf;
    }
}
void list::splice(uint64_t off, uint64_t len, list* claim_by) {
    if (len == 0) {
        return;
    }

    if (off >= length()) {
        throw end_of_buffer();
    }

    auto curbuf = std::begin(_buffers);
    auto curbuf_prev = _buffers.before_begin();
    while (off > 0) {
        spec_assert(curbuf != std::end(_buffers));
        if (off >= (*curbuf).length()) {
            // skip this buffer
            off -= (*curbuf).length();
            curbuf_prev = curbuf++;
        } else {
            // somewhere in this buffer
            break;
        }
    }

    if (off) {
        // add a reference to the front bit
        // insert it before curbuf
        _buffers.insert_after(curbuf_prev,
                *ptr_node::create(*curbuf, 0, off).release());
        _len += off;
        _num += 1;
        ++curbuf_prev;
    }

    _tail_pnode_cache = &always_empty_bptr;
    while (len > 0) {
        if (off + len < (*curbuf).length()) {
            // partial
            if (claim_by) {
                claim_by->append(*curbuf, off, len);
            }
             // ignore the beginning
             (*curbuf).set_offset(off + len + (*curbuf).offset());
             (*curbuf).set_length((*curbuf).length() - (len + off));
             _len -= off + len;
             break;
        }

        // direct through the end
        auto create_len = (*curbuf).length() - off;
        if (claim_by) {
            claim_by->append(*curbuf, off, create_len);
        }
        _len -= (*curbuf).length();
        _num -= 1;
        curbuf = _buffers.erase_after_and_dispose(curbuf_prev);
        len -= create_len;
        off = 0;
    }
}

void list::write(uint64_t off, uint64_t len, std::ostream& out) const {
    buffer_list blist;
    blist.substr_of(*this, off, len);

    for (const auto& node : blist._buffers) {
        if (node.length()) {
            out.write(node.c_str(), node.length());
        }
    }
}

void list::encode_base64(buffer_list& waiting_encode_list) {
    buffer_ptr bptr(length() * 4 /3 + 3);
    uint64_t len = spec_armor(bptr.c_str(), bptr.c_str() + bptr.length(),
                              c_str(), c_str() + length());
    bptr.set_length(len);
    waiting_encode_list.push_back(std::move(bptr));
}

void list::decode_base64(buffer_list& encoded_list) {
    buffer_ptr bptr(4 + ((encoded_list.length() * 3) / 4));
    int64_t len = spec_unarmor(bptr.c_str(), bptr.c_str() + bptr.length(),
                                encoded_list.c_str(),
                                encoded_list.c_str() + encoded_list.length());
    if (len < 0) {
        std::ostringstream oss;
        oss << "decode_base64: decoding failed:\n";
        hexdump(oss);
        throw malformed_input(oss.str().c_str());
    }
    spec_assert(len <= (int64_t)bptr.length());
    bptr.set_length(len);
    push_back(std::move(bptr));
}

void list::write_stream(std::ostream &out) const {
    for (const auto& node : _buffers) {
        if (node.length() > 0) {
            out.write(node.c_str(), node.length());
        }
    }
}
void list::hexdump(std::ostream &out, bool trailing_newline) const {
    if (!length()) {
        return;
    }

    std::ios_base::fmtflags original_flags = out.flags();
    out.setf(std::ios::right);
    out.fill('0');

    uint64_t per = 16;
    char last_row_char = '\0';
    bool was_same = false, did_star = false;
    for (uint64_t off = 0; off < length(); off += per) {
        if (off == 0) {
            last_row_char = (*this)[off];
        }

        if (off + per < length()) {
            bool row_is_same = true;
            for (uint64_t i = 0; i < per && off + i < length(); i++) {
                char current_char = (*this)[off + i];
                if (current_char != last_row_char) {
                    if (i == 0) {
                        last_row_char = current_char;
                        was_same = false;
                        did_star = false;
                    } else {
                        row_is_same = false;
                    }
                }
            }

            if (row_is_same) {
                if (was_same) {
                    if (!did_star) {
                        out << "\n*";
                        did_star = true;
                    }
                    continue;
                }
                was_same = true;
            } else {
                was_same = false;
                did_star = false;
            }
        }

        if (off) {
            out << "\n";
        }
        out << std::hex << std::setw(8) << off << " ";

        uint64_t i = 0;
        for (i = 0; i < per && off + i < length(); i++) {
            if (i == 8) {
                out << ' ';
            }
            out << " " << std::setw(2)
                << ((unsigned)(*this)[off + i] & 0xff);
        }
        for (; i < per; i++) {
            if (i == 8) {
                out << ' ';
            }
            out << "   ";
        }

        out << "  |";
        for (i = 0; i < per && off + i < length(); i++) {
            char c = (*this)[off + i];
            if (isupper(c) || islower(c) || isdigit(c) ||
                c == ' ' || ispunct(c)) {
                out << c;
            } else {
                out << '.';
            }
        }
        out << '|' << std::dec;
    }
    if (trailing_newline) {
        out << "\n" << std::hex << std::setw(8) << length();
        out << "\n";
    }
    out.flags(original_flags);
}
ssize_t list::pread_file(const char* fn, uint64_t off,
                   uint64_t len, std::string *error) {
    int fd = TEMP_FAILURE_RETRY(::open(fn, O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
        int err = errno;
        std::ostringstream oss;
        oss << "can't open " << fn << " : " << cpp_strerror(err);
        *error = oss.str();
        return -err;
    }

    struct stat st;
    memset(&st, 0, sizeof(st));
    if (::fstat(fd, &st) < 0) {
        int err = errno;
        std::ostringstream oss;
        oss << "buffer_list::read_file(" << fn << "): stat error: "
            << cpp_strerror(err);
        *error = oss.str();
        VOID_TEMP_FAILURE_RETRY(::close(fd));
        return -err;
    }

    if (off > (uint64_t)st.st_size) {
        std::ostringstream oss;
        oss <<"buffer_list::read_file(" << fn << "): "
            <<"read error: size < offset";
        *error = oss.str();
        VOID_TEMP_FAILURE_RETRY(::close(fd));
        return 0;
    }

    if (len > st.st_size - off) {
        len = st.st_size - off;
    }
    ssize_t ret = lseek64(fd, off, SEEK_SET);
    if (ret != (ssize_t)off) {
        return -errno;
    }

    ret = read_fd(fd, len);
    if (ret < 0) {
        std::ostringstream oss;
        oss << "buffer_list::read_file(" << fn << "): "
            << "read error: " << cpp_strerror(ret);
        *error = oss.str();
        VOID_TEMP_FAILURE_RETRY(::close(fd));
        return ret;
    } else if (ret != (ssize_t)len) {
        // premature EOF. The file may be changed between stat() and read()
        std::ostringstream oss;
        oss << "buffer_list::read_file(" << fn << "): "
            << "warning: got premature EOF.";
        *error = oss.str(); // not actually an error, but weird
    }
    VOID_TEMP_FAILURE_RETRY(::close(fd));
    return 0;
}
int list::read_file(const char* fn, std::string *error) {
    int fd = TEMP_FAILURE_RETRY(::open(fn, O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
        int err = errno;
        std::ostringstream oss;
        oss << "can't open " << fn << " : " << cpp_strerror(err);
        *error = oss.str();
        return -err;
    }

    struct stat st;
    memset(&st, 0, sizeof(st));
    if (::fstat(fd, &st) < 0) {
        int err = errno;
        std::ostringstream oss;
        oss << "buffer_list::read_file(" << fn << "): stat error: "
            << cpp_strerror(err);
        *error = oss.str();
        VOID_TEMP_FAILURE_RETRY(::close(fd));
        return -err;
    }

    ssize_t ret = read_fd(fd, st.st_size);
    if (ret < 0) {
        std::ostringstream oss;
        oss << "buffer_list::read_file(" << fn << "): "
            << "read error: " << cpp_strerror(ret);
        *error = oss.str();
        VOID_TEMP_FAILURE_RETRY(::close(fd));
        return ret;
    } else if (ret != st.st_size) {
        // premature EOF. The file may be changed between stat() and read()
        std::ostringstream oss;
        oss << "buffer_list::read_file(" << fn << "): "
            << "warning: got premature EOF.";
        *error = oss.str(); // not actually an error, but weird
    }
    VOID_TEMP_FAILURE_RETRY(::close(fd));
    return 0;
}
ssize_t list::read_fd(int fd, size_t len) {
    auto bptr = ptr_node::create(buffer::create(len));
    ssize_t ret = safe_read(fd, (void*)bptr->c_str(), len);
    if (ret >= 0) {
        bptr->set_length(ret);
        push_back(std::move(bptr));
    }
    return ret;
}
int list::write_file(const char* fn, int mode) {
    int fd = TEMP_FAILURE_RETRY(::open(fn, O_WRONLY | O_CREAT | O_CLOEXEC,
                                       mode));
    if (fd < 0) {
        int err = errno;
        std::cerr << "buffer_list::write_file(" << fn << "): "
                  << "failed to open file: " << cpp_strerror(err)
                  << std::endl;
        return -err;
    }
    int ret = write_fd(fd);
    if (ret) {
        std::cerr << " buffer_list::write_file(" << fn << "): "
                  << "write_fd error: " << cpp_strerror(ret)
                  << std::endl;
        VOID_TEMP_FAILURE_RETRY(::close(fd));
        return ret;
    }
    if (TEMP_FAILURE_RETRY(::close(fd))) {
        int err = errno;
        std::cerr << "buffer_list::write_file(" << fn << "): "
                  << "close error: " << cpp_strerror(err)
                  << std::endl;
        return -err;
    }
    return 0;
}
int list::write_fd(int fd) const {
    // use writev
    iovec iov[IOV_MAX];
    int iovlen = 0;
    ssize_t bytes = 0;

    auto bptr = std::cbegin(_buffers);
    while (bptr != std::cend(_buffers)) {
        if (bptr->length() > 0) {
            iov[iovlen].iov_base = (void*)bptr->c_str();
            iov[iovlen].iov_len = bptr->length();
            bytes += bptr->length();
            iovlen++;
        }
        ++bptr;

        if (iovlen == IOV_MAX ||
            bptr == _buffers.end()) {
            iovec *start = iov;
            int num = iovlen;
            ssize_t wrote;
        retry:
            wrote = ::writev(fd, start, num);
            if (wrote < 0) {
                int err = errno;
                if (err == EINTR) {
                    goto retry;
                }
                return -err;
            }
            if (wrote < bytes) {
                // partial write, recover
                while ((size_t)wrote >= start[0].iov_len) {
                    wrote -= start[0].iov_len;
                    bytes -= start[0].iov_len;
                    start++;
                    num--;
                }
                if (wrote > 0) {
                    start[0].iov_len -= wrote;
                    start[0].iov_base = (char*)start[0].iov_base + wrote;
                    bytes -= wrote;
                }
                goto retry;
            }
            iovlen = 0;
            bytes = 0;
        }
    }
    return 0;
}

static int do_writev(int fd, struct iovec *vec,
                    uint64_t offset, uint64_t veclen, uint64_t bytes) {
    while (bytes > 0) {
        ssize_t r = 0;
        r = ::pwritev(fd, vec, veclen, offset);
        if (r < 0) {
            if (errno = EINTR) {
                continue;
            }
            return -errno;
        }
        bytes -= r;
        offset += r;
        if (bytes == 0) {
            break;
        }

        while (r > 0) {
            if (vec[0].iov_len <= (size_t)r) {
                // drain this whole item
                r -= vec[0].iov_len;
                ++vec;
                --veclen;
            } else {
                vec[0].iov_base = (char *)vec[0].iov_base + r;
                vec[0].iov_len -= r;
                break;
            }
        }
    }
    return 0;
}

int list::write_fd(int fd, uint64_t offset) const {
    iovec iov[IOV_MAX];
    auto bptr = std::cbegin(_buffers);

    uint64_t left_pbrs = get_num_buffers();
    while (left_pbrs) {
        ssize_t bytes = 0;
        uint64_t iovlen = 0;
        uint64_t size = std::min<uint64_t>(left_pbrs, IOV_MAX);
        left_pbrs -= size;
        while (size > 0) {
            iov[iovlen].iov_base = (void*)bptr->c_str();
            iov[iovlen].iov_len = bptr->length();
            iovlen++;
            bytes += bptr->length();
            ++bptr;
            size--;
        }

        int r = do_writev(fd, iov, offset, iovlen, bytes);
        if (r < 0) {
            return r;
        }
        offset += bytes;
    }
    return 0;
}

uint32_t list::crc32c(uint32_t crc) const {
    int cache_misses = 0;
    int cache_hits = 0;
    int cache_adjusts = 0;
    for (const auto& node : _buffers) {
        if (node.length() == 0) {
            continue;
        }
        raw *const pbraw = node.m_raw;
        std::pair<uint64_t, uint64_t>ofs(node.offset(),
                                    node.offset() + node.length());
        std::pair<uint32_t, uint32_t> ccrc;
        if (pbraw->get_crc(ofs, &ccrc)) {
            if (ccrc.first == crc) {
                // got it ready
                crc = ccrc.second;
                cache_hits++;
            } else {
                /* If we have cached crc32c(buf, v) for initial value v,
                 * we can covert this to a different initial value v' by:
                 *   crc32c(buf, v') = crc32c(buf, v) ^ adjustment
                 * where adjustment = crc32c(0 * len (buf), v ^ v')
                 *
                 * http://crcutil.googlecode.com/files/crc-doc.1.0.pdf
                 * note, u for our crc32c implementation is 0
                 */
                crc = ccrc.second ^ spec_crc32c(ccrc.first ^ crc, NULL,
                                                node.length());
                cache_adjusts++;
            }
        } else {
            cache_misses++;
            uint32_t base = crc;
            crc = spec_crc32c(crc, (unsigned char*)node.c_str(),
                              node.length());
            pbraw->set_crc(ofs, std::make_pair(base, crc));
        }
    }

    if (buffer_track_crc) {
        if (cache_adjusts) {
            buffer_cached_crc_adjusted += cache_adjusts;
        }
        if (cache_hits) {
            buffer_cached_crc += cache_hits;
        }
        if (cache_misses) {
            buffer_missed_crc += cache_misses;
        }
    }
    return crc;
}
void list::invalidate_crc() {
    for (const auto& node : _buffers) {
        if (node.m_raw) {
            node.m_raw->invalidate_crc();
        }
    }
}

buffer_list list::static_from_mem(char* c, size_t len) {
    buffer_list blist;
    blist.push_back(ptr_node::create(create_static(len, c)));
    return blist;
}
buffer_list list::static_from_cstring(char* c) {
    return static_from_mem(c, std::strlen(c));
}
buffer_list list::static_from_string(std::string& s) {
    // C++14 just has string::data return a char* from a non-const string
    return static_from_mem(const_cast<char*>(s.data()), s.length());
}

buffer_ptr buffer_list::always_empty_bptr;


std::ostream& operator<<(std::ostream& out, const buffer_list& blist) {
    out << "buffer_list:(len=" << blist.length() << "," << std::endl;

    for (const auto& node : blist.buffers()) {
        out << "\t" << node;
        if (&node != &blist.buffers().back()) {
            out << "," << std::endl;
        }
    }
    out << std::endl << ")";
    return out;
}
} //namespace spec::buffer
