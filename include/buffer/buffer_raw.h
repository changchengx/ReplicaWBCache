/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_RAW_H
#define BUFFER_RAW_H

#include <utility>
#include <type_traits>

#include "../spec_atomic.h"
#include "../inline_memory.h"
#include "../mempool/mempool.h"
#include "../spinlock/spinlock.h"

#include "buffer_ptr.h"

namespace spec {

namespace buffer {

class raw {
protected:
    char* m_data;
    uint64_t m_len;
public:
    std::aligned_storage_t<sizeof(ptr_node), alignof(ptr_node)> bptr_storage;

    spec::atomic<uint64_t> nref{0};
    int64_t mempool_type_id;

    std::pair<size_t, size_t>
        last_crc_offset{std::numeric_limits<size_t>::max(),
                        std::numeric_limits<size_t>::max()};

    std::pair<uint32_t, uint32_t> last_crc_val;

    mutable spec::spinlock crc_spinlock;

    explicit
    raw(uint64_t len, int64_t mempool_type_index = mempool::mempool_buffer_anon)
        : raw(nullptr, len, mempool_type_index) {
    }

    raw(char *data, uint64_t len,
        int64_t mempool_type_index = mempool::mempool_buffer_anon)
        : m_data(data), m_len(len), nref(0), mempool_type_id(mempool_type_index) {
        mempool::get_pool(mempool::pool_type_id(mempool_type_id)).adjust_count(1, m_len);
    }
    virtual ~raw() {
        mempool::get_pool(mempool::pool_type_id(mempool_type_id)).adjust_count(-1, -(int)m_len);
    }

    void set_len(uint64_t len) {
        mempool::get_pool(mempool::pool_type_id(mempool_type_id)).adjust_count(-1, -(int)m_len);
        m_len = len;
        mempool::get_pool(mempool::pool_type_id(mempool_type_id)).adjust_count(1, m_len);
    }

    void reassign_to_mempool(int64_t mempool_type_index) {
        if (mempool_type_index == mempool_type_id) {
            return;
        }

        mempool::get_pool(mempool::pool_type_id(mempool_type_id)).adjust_count(-1, -(int)m_len);
        mempool_type_id = mempool_type_index;
        mempool::get_pool(mempool::pool_type_id(mempool_type_id)).adjust_count(1, m_len);
    }

    void try_assign_to_mempool(int64_t mempool_type_index) {
        if (mempool_type_id == mempool::mempool_buffer_anon) {
            reassign_to_mempool(mempool_type_index);
        }
    }

private:
    // no copying
    raw(const raw&) = delete;
    const raw& operator=(const raw&) = delete;

public:
    virtual raw* clone_empty() = 0;

    char* get_data() const {
        return m_data;
    }
    uint64_t get_len() const {
        return m_len;
    }

    spec::unique_leakable_ptr<raw> clone() {
        raw* const praw = clone_empty();
        memcpy(praw->m_data, m_data, m_len);
        return spec::unique_leakable_ptr<raw>(praw);
    }

    bool get_crc(const std::pair<size_t, size_t> &fromto,
                 std::pair<uint32_t, uint32_t> *crc) const {
        std::lock_guard lg(crc_spinlock);
        if (last_crc_offset == fromto) {
            *crc = last_crc_val;
            return true;
        }
        return false;
    }

    void set_crc(const std::pair<size_t, size_t> &fromto,
                 const std::pair<uint32_t, uint32_t> &crc) {
        std::lock_guard lg(crc_spinlock);
        last_crc_offset = fromto;
        last_crc_val = crc;
    }

    void invalidate_crc() {
        std::lock_guard lg(crc_spinlock);
        last_crc_offset.first = std::numeric_limits<size_t>::max();
        last_crc_offset.second = std::numeric_limits<size_t>::max();
    }
};

extern std::ostream& operator<<(std::ostream& out, const buffer::raw& braw);

} // namespace:buffer

} // namespace:spec

#endif // BUFFER_RAW_H
