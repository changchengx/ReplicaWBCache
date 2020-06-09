/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <limits.h>
#include <cstring>
#include <type_traits>
#include "buffer_create.h"
#include "buffer_ptr.h"
#include "buffer_fwd.h"
#include "../inline_memory.h"

#ifndef BUFFER_LIST_H
#define BUFFER_LIST_H
namespace spec {

namespace buffer {

class list {
public:
    /* low level implementation of single linked list.
     * buffer::list is built on it.
     */
    class buffers_t {
    private:
        // http://stable.ascii-flow.appspot.com/
        /*          +--------------------------------------+
         *     _root|       ptr_node              ptr_node |
         *    +-----v----+  +----------+             +-----|----+
         *    | +------+ |  | +------+ |             | +---+--+ |
         *    | | next +----> | next +---------------> | next | |
         *    | +------+ |  | +------+ |             | +------+ |
         *    +----------+  +----------+             +-----^----+
         *     +-------+                                   |
         *     | _tail +-----------------------------------+
         *     +-------+
         */
        ptr_hook _root; //_root.next could be thought as head
        ptr_hook* _tail;

    public:
        template <typename T>
        class buffers_iterator {
        private:
            typename std::conditional_t<std::is_const<T>::value,
                                        const ptr_hook*,
                                        ptr_hook*> cur;

            template <typename U> friend class buffers_iterator;
        public:
            using value_type = T;
            using reference = typename std::add_lvalue_reference_t<T>;
            using pointer = typename std::add_pointer_t<T>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            template <typename U>
            buffers_iterator(U* const p)
                : cur(p) {
            }
            template <typename U>
            buffers_iterator(const buffers_iterator<U>& other)
                : cur(other.cur) {
            }
            buffers_iterator() = default;

            T& operator*() const {
                return *reinterpret_cast<T*>(cur);
            }

            T* operator->() const {
                return reinterpret_cast<T*>(cur);
            }

            buffers_iterator& operator++() {
                cur = cur->next;
                return *this;
            }
            buffers_iterator operator++(int) {
                const auto temp(*this);
                ++*this;
                return temp;
            }

            template <typename U>
            buffers_iterator& operator= (buffers_iterator<U>& other) {
                cur = other.cur;
                return *this;
            }
            buffers_iterator& operator=(const buffers_iterator&) = default;

            bool operator== (const buffers_iterator& rhs) const {
                return cur == rhs.cur;
            }

            bool operator!= (const buffers_iterator& rhs) const {
                return !(*this == rhs);
            }

            using citer_t = buffers_iterator<typename std::add_const_t<T>>;
            operator citer_t() const {
                return citer_t(cur);
            }
        };

        using const_iterator = buffers_iterator<const ptr_node>;
        using iterator = buffers_iterator<ptr_node>;

        using reference = std::add_lvalue_reference_t<ptr_node>;
        using const_reference = std::add_const_t<reference>;

        buffers_t(): _root(&_root), _tail(&_root) {
        }

        buffers_t(const buffers_t&) = delete;
        buffers_t(buffers_t&& other)
            : _root(other._root.next == &other._root ? &_root: other._root.next),
              _tail(other._tail == &other._root ? &_root: other._tail) {
            other._root.next = &other._root;
            other._tail = &other._root;
            _tail->next = &_root;
        }

        buffers_t& operator=(buffers_t&& other) {
            if(&other != this) {
                clear_and_dispose();
                swap(other);
            }
            return *this;
        }

        void push_back(reference item) {
            item.next = &_root;
            _tail->next = &item;
            _tail = &item;
        }

        void push_front(reference item) {
            item.next = _root.next;
            _root.next = &item;
            _tail = (_tail == &_root ? &item : _tail);
        }

        bool empty() const {
            return _tail == &_root;
        }

        const_iterator begin() const {
            return _root.next;
        }
        const_iterator before_begin() const {
            return &_root;
        }
        const_iterator end() const {
            return &_root;
        }
        iterator begin() {
            return _root.next;
        }
        iterator before_begin() {
            return &_root;
        }
        iterator end() {
            return &_root;
        }

        const_reference front() const {
            return reinterpret_cast<const_reference>(*_root.next);
        }
        const_reference back() const {
            return reinterpret_cast<const_reference>(*_tail);
        }

        reference front() {
            return reinterpret_cast<reference>(*_root.next);
        }
        reference back() {
            return reinterpret_cast<reference>(*_tail);
        }

        void swap(buffers_t& other) {
            const auto copy_root = _root;
            _root.next = (other._root.next == &other._root) ?
                         &_root : other._root.next;
            other._root.next = (copy_root.next == &_root) ?
                               &other._root : copy_root.next;

            const auto copy_tail = _tail;
            _tail = (other._tail == &other._root) ?
                    &_root : other._tail;
            other._tail = (copy_tail == &_root) ?
                          &other._root : copy_tail;

            _tail->next = &_root;
            other._tail->next = &other._root;
        }

        void clear_and_dispose() {
            for (auto it = begin(); it != end(); /* nop */) {
                auto& node = *it;
                it = it->next;
                ptr_node::disposer()(&node);
            }
            _root.next = &_root;
            _tail = &_root;
        }

        iterator erase_after(const_iterator it) {
            const auto* to_erase = it->next;
#if defined(BUFFER_DEBUG)
            spec_assert(to_erase != &_root); //suspected bug
#endif

            it->next = to_erase->next;
            _root.next = (_root.next == to_erase ? to_erase->next : _root.next);
            _tail = (_tail == to_erase ? (ptr_hook*)&*it : _tail);
            return it->next;
        }

        iterator erase_after_and_dispose(iterator it) {
            auto* to_dispose = &*std::next(it);
            auto ret = erase_after(it);
            ptr_node::disposer()(to_dispose);
            return ret;
        }

        void insert_after(const_iterator it, reference item) {
            item.next = it->next;
            it->next = &item;
            _root.next = (it == end() ? &item : _root.next);
            _tail = (const_iterator(_tail) == it ? &item : _tail);
        }

        void splice_back(buffers_t& other) {
            if (other.empty()) {
                return;
            }

            other._tail->next = &_root;
            _tail->next = other._root.next;
            _tail = other._tail;

            other._root.next = &other._root;
            other._tail = &other._root;
        }

        void clone_from(const buffers_t& other) {
            clear_and_dispose();
            for (auto& node : other) {
                ptr_node* clone = ptr_node::cloner()(node);
                push_back(*clone);
            }
        }
    };

    class iterator;

private:
    buffers_t _buffers; //low level list

    ptr* _tail_pnode_cache;
    uint64_t _len, _num;

    template <bool is_const>
    class iterator_impl {
        friend class iterator_impl<true>;
    protected:
        /*
         *
         *         <------------------ iterator: m_list_it -------------------->
         *
         * m_list-->+----------+           +----------+           +------------+
         *         | ptr_hook  ------------> ptr_hook ------------>  ptr_hook  |
         *         +----^------+           +----^-----+           +-----^------+
         *              |                       |                       |
         *              |                       |                       |
         *         +----+------+           +----+-----+           +-----+------+
         *         | ptr_node  |           | ptr_node |           |  ptr_node  |
         *         +----+------+           +----+-----+           +-----+------+
         *              |                       |                       |
         *              |                       |                       |
         *         +----v------+           +----v-----+           +-----v------+
         *         |   ptr     |           |   ptr    |           |    ptr     |
         *         ++----------+           ++---------+           ++-----------+
         *          |                       |                      |
         *          | +---------+           | +--------+           | +----------+
         *          +->   raw   +           +->  raw   +           +->   raw    +
         *            +---------+-->----+     +--------+-->----+     +----------+-->----+
         *                         |    |                 |    |                   |    |
         *                         |    |      m_r_off---->    |          m_a_off-->    |
         *                         |    |    (in cur raw) |    |       (in all raw)|    |
         *                         +----+                 +----+                   +----+
         */
        using blist_t = typename std::conditional_t<is_const, const list, list>;
        using list_t = typename std::conditional_t<is_const, const buffers_t, buffers_t>;
        using list_iter_t = typename std::conditional_t<is_const,
              typename buffers_t::const_iterator, typename buffers_t::iterator>;


        blist_t* m_blist;       // point to buffer::list
        list_t* m_list;         // point to low level single list
        list_iter_t m_list_it;  // iterator on the low level single list
        uint64_t m_r_off;       // the offset in one buffer::ptr::m_raw, *m_list_it
        uint64_t m_a_off;       // the offset in all of the buffer::ptr::m_raw


    public:
        using value_type = typename std::conditional_t<is_const, const char, char>;
        using difference_type = std::ptrdiff_t;
        using pointer = typename std::add_pointer_t<value_type>;
        using reference = typename std::add_lvalue_reference_t<value_type>;
        using iterator_category = std::forward_iterator_tag;

        iterator_impl()
            : m_blist(nullptr), m_list(nullptr),
              m_r_off(0), m_a_off(0) {
        }

        iterator_impl(const buffer_list::iterator& it);
        iterator_impl(blist_t *blist, uint64_t off = 0);

        iterator_impl(blist_t *blist, uint64_t a_off, list_iter_t it, uint64_t r_off)
            : m_blist(blist), m_list(&m_blist->_buffers),
              m_list_it(it), m_a_off(a_off), m_r_off(r_off) {
        }

        uint64_t get_off() const {
            return m_a_off;
        }

        uint64_t get_remaining() const {
            return m_blist->length() - m_a_off;
        }

        bool end() const {
            return m_list_it == m_list->end();
        }

        iterator_impl& operator= (const iterator_impl&) = default;
        void seek(uint64_t off);
        char operator*() const;
        iterator_impl& operator+= (uint64_t off);
        iterator_impl& operator++();
        ptr get_current_ptr() const;
        bool is_pointing_same_raw(const ptr& other) const;

        blist_t& get_blist() const {
            return *m_blist;
        }

        // Copy data out to be appended after dest.
        void copy(uint64_t len, char *dest);
        void copy_deep(uint64_t len, ptr &dest);
        void copy_shallow(uint64_t len, ptr &dest);
        void copy(uint64_t len, list &dest);
        void copy(uint64_t len, std::string &dest);
        void copy_all(list &dest);

        /* get a pointer of the current iterator position,
         * return the number of bytes that can be read from
         * that position (up to max_req_size ), move ahead
         * the iterator by that number of bytes.
         */
        uint64_t get_ptr_and_advance(uint64_t max_req_size, const char* *p);

        // calculate crc from iterator position
        uint32_t crc32c(size_t length, uint32_t crc);

        friend bool operator== (const iterator_impl& lhs,
                                const iterator_impl& rhs) {
            return &lhs.get_blist() == &rhs.get_blist() &&
                   lhs.get_off() == rhs.get_off();
        }

        friend bool operator!= (const iterator_impl& lhs,
                                const iterator_impl& rhs) {
            return !(lhs == rhs);
        }
    };

public:
    using const_iterator = iterator_impl<true>;

    class iterator: public iterator_impl<false> {
    public:
        iterator() = default;
        iterator(blist_t *blist, uint64_t off = 0);
        iterator(blist_t *blist, uint64_t a_off,
                 list_iter_t it, uint64_t r_off);

        iterator& operator= (const iterator&) = default;
        void copy_in(uint64_t len, const char* src, bool crc_reset = true);
        void copy_in(uint64_t len, const list& other);
    };

    struct reserve_t {
        char* bptr_data;
        uint64_t* bptr_len;
        uint64_t* blist_len;
    };

    class contiguous_appender {
    private:
        friend class list;

        buffer_list& _blist;
        buffer_list::reserve_t _space;
        char* _pos;
        bool _deep_copy;

        // running count of bytes appended that are not reflected by @_pos
        uint64_t _out_of_band_offset = 0;

        contiguous_appender(buffer_list& blist, uint64_t len, bool deep_copy)
            : _blist(blist),
              _space(blist.obtain_contiguous_space(len)),
              _pos(_space.bptr_data),
              _deep_copy(deep_copy) {
        }

        void flush_and_continue() {
            const uint64_t step_advance = _pos - _space.bptr_data;
            *_space.bptr_len += step_advance;
            *_space.blist_len += step_advance;
            _space.bptr_data = _pos;
        }

    public:
        ~contiguous_appender() {
            flush_and_continue();
        }

        uint64_t get_out_of_band_offset() const {
            return _out_of_band_offset;
        }

        void append(const char* __restrict__ pos, size_t len) {
            maybe_inline_memcpy(_pos, pos, len, 16);
            _pos += len;
        }

        char *get_pos_add(uint64_t len) {
            char *rst = _pos;
            _pos += len;
            return rst;
        }

        char *get_pos() const {
            return _pos;
        }

        void append(const ptr& p) {
            const auto plen = p.length();
            if (!plen) {
                return;
            }
            if (_deep_copy) {
                append(p.c_str(), plen);
            } else {
                flush_and_continue();
                _blist.append(p);
                _space = _blist.obtain_contiguous_space(0);
                _out_of_band_offset += plen;
            }
        }
        void append(const buffer_list& blist) {
            if (_deep_copy) {
                for (const auto &buf: blist._buffers) {
                    append(buf.c_str(), buf.length());
                }
            } else {
                flush_and_continue();
                _blist.append(blist);
                _space = _blist.obtain_contiguous_space(0);
                _out_of_band_offset += blist.length();
            }
        }

        size_t get_logical_offset() const {
            return _out_of_band_offset + (_pos - _space.bptr_data);
        }
    };

    contiguous_appender
    get_contiguous_appender(uint64_t len, bool deep_copy = false) {
        return contiguous_appender(*this, len, deep_copy);
    }

    /*Keep contiguous_filler simple to be not "bigger" than a single pointer.*/
    class contiguous_filler {
    private:
        friend list;
        char *_pos;

        contiguous_filler(char *const pos): _pos(pos) {
        }

    public:
        void advance(const uint64_t len) {
            _pos += len;
        }

        void copy_in(const uint64_t len, const char* const src) {
            memcpy(_pos, src, len);
            advance(len);
        }
        char* c_str() {
            return _pos;
        }
    };
    static_assert(sizeof(contiguous_filler) == sizeof(void *),
                  "contiguous_filler shouldn't be bigger than one pointer");

    // boost performance
    class page_aligned_appender {
        friend class list;
    private:
        buffer_list *_page_blist;
        uint64_t _min_alloc;
        ptr _buffer;
        char *_pos, *_end;

        page_aligned_appender(buffer_list *blist, uint64_t min_pages)
            : _page_blist(blist),
              _min_alloc(min_pages * SPEC_PAGE_SIZE),
              _pos(nullptr), _end(nullptr) {
        }

    public:
        ~page_aligned_appender() {
            flush();
        }

        void flush() {
            if (_pos && _pos != _buffer.c_str()) {
                uint64_t len = _pos - _buffer.c_str();
                _page_blist->append(_buffer, 0, len);
                _buffer.set_length(_buffer.length() - len);
                _buffer.set_offset(_buffer.offset() + len);
            }
        }

        void append(const char *buf, uint64_t len) {
            while (len > 0) {
                if (!_pos) {
                    uint64_t alloc = (len + SPEC_PAGE_SIZE - 1) & SPEC_PAGE_MASK;
                    if (alloc < _min_alloc) {
                        alloc = _min_alloc;
                    }
                    _buffer = create_page_aligned(alloc);
                    _pos = _buffer.c_str();
                    _end = _buffer.end_c_str();
                }
                uint64_t length = len;
                if (length > static_cast<uint64_t>(_end - _pos)) {
                    length = static_cast<uint64_t>(_end - _pos);
                }
                memcpy(_pos, buf, length);
                _pos += length;
                buf += length;
                len -= length;
                if (_pos == _end) {
                    _page_blist->append(_buffer, 0, _buffer.length());
                    _pos = _end = nullptr;
                }
            }
        }
    };

    page_aligned_appender get_page_aligned_appender(uint64_t min_elements = 1) {
        return page_aligned_appender(this, min_elements);
    }

private:
    /* no underlying raw and the _len is always 0 for always_empty_bptr; */
    static ptr always_empty_bptr;
    ptr_node& refill_append_space(const uint64_t len);

public:
    list(): _tail_pnode_cache(&always_empty_bptr), _len(0), _num(0) {
    }

    list(uint64_t pre_alloc_size)
        : _tail_pnode_cache(&always_empty_bptr), _len(0), _num(0) {
        reserve(pre_alloc_size);
    }

    list(const list& other)
        : _tail_pnode_cache(&always_empty_bptr),
          _len(other._len), _num(other._num) {
        _buffers.clone_from(other._buffers);
    }

    list(list&& other) noexcept
        : _buffers(std::move(other._buffers)),
          _tail_pnode_cache(other._tail_pnode_cache),
          _len(other._len),
          _num(other._num) {
        other.clear();
    }

    ~list() {
        _buffers.clear_and_dispose();
    }

    list& operator= (const list& other) {
        if (this != &other) {
            _tail_pnode_cache = &always_empty_bptr;
            _buffers.clone_from(other._buffers);
            _len = other._len;
            _num = other._num;
        }
        return *this;
    }
    list& operator= (list&& other) noexcept {
        _buffers = std::move(other._buffers);
        _tail_pnode_cache = other._tail_pnode_cache;
        _len = other._len;
        _num = other._num;
        other.clear();
        return *this;
    }

    uint64_t get_wasted_space() const;
    uint64_t get_num_buffers() const {
        return _num;
    }
    const ptr_node& front() const {
        return _buffers.front();
    }
    const ptr_node& back() const {
        return _buffers.back();
    }

    int get_mempool_type() const;
    void reassign_to_mempool(int64_t mempool_type_index);
    void try_assign_to_mempool(int64_t mempool_type_index);

    uint64_t get_append_buffer_unused_tail_length() const {
        return _tail_pnode_cache->unused_tail_length();
    }

    const buffers_t& buffers() const {
        return _buffers;
    }
    void swap(buffer_list& other) noexcept;
    uint64_t length() const {
#if defined(BUFFER_DEBUG)
        unsigned len = 0;
        for (std::list<ptr>::const_iterator it = _buffers.begin();
             it != _buffers.end();
             it++) {
            len += (*it).length();
        }
        spec_assert(len == _len);
#endif
        return _len;
    }

    bool contents_equal(const buffer_list& other) const;
    bool contents_equal(const void *other, uint64_t length) const;

    bool is_provided_buffer(const char* dst) const;
    bool is_aligned(uint64_t align) const;
    bool is_page_aligned() const;
    bool is_n_align_sized(uint64_t align) const;
    bool is_n_page_sized() const;
    bool is_aligned_size_and_memory(uint64_t align_size,
                                    uint64_t align_memory) const;

    bool is_zero() const;

    void clear() noexcept {
        _tail_pnode_cache = &always_empty_bptr;
        _buffers.clear_and_dispose();
        _len = 0;
        _num = 0;
    }
    void push_back(const ptr& bptr) {
        if (bptr.length() == 0) {
            return;
        }
        /* auto unique_pnode = ptr_node::create(bptr);
         * auto* pnode = unique_pnode.release();
         * _buffers.push_back(*pnode);
         */
        _buffers.push_back(*ptr_node::create(bptr).release());
        _len += bptr.length();
        _num += 1;
    }
    void push_back(ptr&& bptr) {
        if (bptr.length() == 0) {
            return;
        }
        _len += bptr.length();
        _num += 1;
        /* auto unique_pnode = ptr_node::create(std::move(bptr));
         * auto* pnode = unique_pnode.release();
         * _buffers.push_back(*pnode);
         */
        _buffers.push_back(*ptr_node::create(std::move(bptr)).release());
        _tail_pnode_cache = &always_empty_bptr;
    }
    void push_back(const ptr_node&) = delete;
    void push_back(ptr_node&) = delete;
    void push_back(ptr_node&&) = delete;
    void push_back(std::unique_ptr<ptr_node, ptr_node::disposer> bptr) {
        if (bptr->length() == 0) {
            return;
        }
        _tail_pnode_cache = bptr.get();
        _len += bptr->length();
        _num += 1;
        _buffers.push_back(*bptr.release());
    }
    void push_back(raw* const braw) = delete;
    void push_back(unique_leakable_ptr<raw> pbraw) {
        _buffers.push_back(*ptr_node::create(std::move(pbraw)).release());
        _tail_pnode_cache = &_buffers.back();
        _len += _buffers.back().length();
        _num += 1;
    }

    void zero();
    void zero(uint64_t off, uint64_t len);

    bool is_contiguous() const;
    void rebuild();
    void rebuild(std::unique_ptr<ptr_node, ptr_node::disposer> nb);
    bool rebuild_aligned(uint64_t align);

    /* max_buffers = 0 means that it doesn't care _buffers.size().
     * In other case, it must meet with below requirement after
     * rebuilding:
     *   _buffers.size() <= max_buffers
     */
    bool rebuild_aligned_size_and_memory(uint64_t align_size,
                                         uint64_t align_memory,
                                         uint64_t max_buffers = 0);
    bool rebuild_page_aligned();

    void reserve(uint64_t pre_alloc_size);

    void claim_append(buffer_list& other_blist);
    void claim_append(buffer_list&& rvalue_blist);
    void claim_append_piecewise(buffer_list& other_blist);

    void share(const buffer_list& blist) {
        if (this != &blist) {
            clear();
            for (const auto& bptr: blist._buffers) {
                _buffers.push_back(*ptr_node::create(bptr).release());
            }
            _len = blist._len;
            _num = blist._num;
        }
    }

    iterator begin(uint64_t offset = 0) {
        return iterator(this, offset);
    }
    iterator end() {
        return iterator(this, _len, _buffers.end(), 0);
    }
    const_iterator begin(uint64_t offset = 0) const {
        return const_iterator(this, offset);
    }
    const_iterator cbegin(uint64_t offset = 0) const {
        return begin(offset);
    }
    const_iterator end() const {
        return const_iterator(this, _len, _buffers.end(), 0);
    }

    void append(char c);
    void append(const char *data, uint64_t len);
    void append(std::string s) {
        append(s.c_str(), s.size());
    }

    template<std::size_t N>
    void append(const char(&s)[N]) {
        append(s, N);
    }
    void append(const char* s) {
        append(s, strlen(s));
    }
    void append(std::string_view s) {
        append(s.data(), s.length());
    }
    void append(const ptr& bptr);
    void append(ptr&& bptr);
    void append(const ptr& bptr, uint64_t off, uint64_t len);
    void append(const buffer_list& blist);
    void append(std::istream& in);
    contiguous_filler append_hole(uint64_t len);
    void append_zero(uint64_t len);
    void prepend_zero(uint64_t len);

    reserve_t obtain_contiguous_space(const uint64_t len);

    const char& operator[] (uint64_t pos) const;
    char *c_str();
    std::string to_str() const;

    void substr_of(const list& other, uint64_t off, uint64_t len);
    void splice(uint64_t off, uint64_t len, list* claim_by = nullptr);
    void write(uint64_t off, uint64_t len, std::ostream& out) const;

    void encode_base64(list& wating_encode_list);
    void decode_base64(list& encoded_list);

    void write_stream(std::ostream &out) const;
    void hexdump(std::ostream &out, bool trailing_newline = true) const;
    ssize_t pread_file(const char* fn, uint64_t off,
                       uint64_t len, std::string *error);
    int read_file(const char* fn, std::string *error);
    ssize_t read_fd(int fd, size_t len);
    int write_file(const char* fn, int mode=0644);
    int write_fd(int fd) const;
    int write_fd(int fd, uint64_t offset) const;

    template <typename VectorT>
    void prepare_iov(VectorT *piov) const {
        spec_assert(_num <= IOV_MAX);
        piov->resize(_num);
        uint64_t i = 0;
        for (auto& p : _buffers) {
            (*piov)[i].iov_base = (void *)p.c_str();
            (*piov)[i].iov_len = p.length();
            ++i;
        }
    }

    uint32_t crc32c(uint32_t crc) const;
    void invalidate_crc();

    static buffer_list static_from_mem(char* c, size_t len);
    static buffer_list static_from_cstring(char* c);
    static buffer_list static_from_string(std::string& s);
};

inline bool operator==(const buffer_list& lhs, const buffer_list& rhs) {
    if (lhs.length() != rhs.length()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

inline bool operator<(const buffer_list& lhs, const buffer_list& rhs) {
    auto lhs_iter = lhs.begin(), rhs_iter = rhs.begin();
    while (lhs_iter != lhs.end() && rhs_iter != rhs.end()) {
        if (*lhs_iter < *rhs_iter) {
            return true;
        }
        if (*lhs_iter > *rhs_iter) {
            return false;
        }
        ++lhs_iter;
        ++rhs_iter;
    }
    return lhs_iter == lhs.end() && rhs_iter != rhs.end();
}

inline bool operator<=(const buffer_list& lhs, const buffer_list& rhs) {
    auto lhs_iter = lhs.begin(), rhs_iter = rhs.begin();
    while (lhs_iter != lhs.end() && rhs_iter != rhs.end()) {
        if (*lhs_iter < *rhs_iter) {
            return true;
        }
        if (*lhs_iter > *rhs_iter) {
            return false;
        }
        ++lhs_iter;
        ++rhs_iter;
    }
    return lhs_iter == lhs.end();
}

inline bool operator!=(const buffer_list &lhs, const buffer_list& rhs) {
    return !(lhs == rhs);
}

inline bool operator>(const buffer_list& lhs, const buffer_list& rhs) {
    return rhs < lhs;
}

inline bool operator>=(const buffer_list& lhs, const buffer_list& rhs) {
    return rhs <= lhs;
}

extern std::ostream& operator<<(std::ostream& out, const buffer_list& blist);

}//namespace: spec
}//namespace: buffer
#endif //BUFFER_LIST_H
