/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_MEMPOOL_H
#define SPEC_MEMPOOL_H

#include <cstddef>
#include <unordered_map>
#include <vector>
#include <list>
#include <mutex>
#include <typeinfo>

#include "spec_assert/spec_assert.h"
#include "spec_atomic.h"
#include "compact_map.h"
#include "compact_set.h"

/* A memory pool is used to audit the memory usage of inner & struct &
 * & class & template-paratermized type.
 *
 * To see what types are consuming the pool resources, the debug mode needs
 * to be enabled and there could be some other accounting info for debugging.
 *
 * Every allocator can be declared and bound with a type so that it could
 * be tracked independently in that memory pool.
 *
 * When declaring one mempool, it will also create a namespace automatically.
 * In this namespace, some common STL containers are prefefined there with
 * the appropriate allocators.
 *     For example, it could exist below containers in mempool "foo"
 *        mempool::foo::map
 *        mempool::foo::multimap
 *        mempool::foo::unordered_map
 *        mempool::foo::set
 *        mempool::foo::multiset
 *        mempool::foo::list
 *        mempool::foo::vector
 *
 * 1. Put objects in particular mempool/allocator
 *    In order to put objects(e.g. an elephant object initialized from elephant
 *    class) in a mempool e.g. foo and particular allocator e.g. zoo, a few
 *    additional declarations are needed.
 *         ||  class elephant{
 *         ||      MEMPOOL_CLASS_HELPERS();
 *         ||      ...
 *         ||  };
 *    Then, write down below content in an appropriate .cc file:
 *         ||   MEMPOOL_DEFINE_OBJECT_FACTORY(elephant, zoo, foo);
 *    Note:
 *         a. The second argument can generally be identical to the first, except
 *            when the type contains a nested scope. For example:
 *            ||   MEMPOOL_DEFINE_OBJECT_FACTORY(Africa::elephant, zoo, foo);
 *         b. The derived class also need define the help/factory to use mempool
 *            whatever the parent define it or not.
 *
 * 2. Put objects in particular mempool/stl-container
 *         ||   mempool::osd::vectory<int> intv;
 *
 *
 * Observability
 * -------------
 *  1. Observe specific memory pool usage:
 *         ||  size_t bytes = mempool::foo::allocated_bytes();
 *         ||  size_t items = mempool::foo::allocated_items();
 *     The runtime complexity is O(num_shards);
 */

namespace mempool {

#define DEFINE_MEMORY_POOLS_HELPER(f) \
    f(buffer_anon)                    \
    f(buffer_meta)                    \
    f(unittest_1)


#define P(x) mempool_##x,
enum pool_type_id {
    DEFINE_MEMORY_POOLS_HELPER(P)
    num_pools
};
#undef P

extern bool debug_mode;
extern void set_debug_mode(bool d);
extern const char *get_pool_name(pool_type_id pool_index);

// shard pool stats across many shard_t's to reduce the amount
// of cacheline ping pong.
enum {
    num_shard_bits = 5,
    num_shards = 1 << num_shard_bits
};

struct shard_t {
  spec::atomic<size_t> allocated_bytes{0};
  spec::atomic<size_t> allocated_items{0};
  char __padding[128 - sizeof(spec::atomic<size_t>) * 2]; // cacheline aligned
} __attribute__ ((aligned (128)));
static_assert(sizeof(shard_t) == 128, "shard_t should be cacheline-sized");


struct type_info_hash {
    std::size_t operator()(const std::type_info& key) const {
        return key.hash_code();
    }
};

struct object_attr {
    const char *type_name{nullptr};
    size_t item_size{0};
    spec::atomic<ssize_t> object_items{0};
};

class pool_type {
private:
    shard_t shard[num_shards];

    mutable std::mutex lock;
    std::unordered_map<const char *, object_attr> object_type_map;

public:
    shard_t* pick_a_shard() {
      auto shard_map = (size_t)pthread_self();
      shard_map = (shard_map >> 3) & ((int)(num_shards) - 1);
      return &shard[shard_map];
    }

    size_t allocated_bytes() const {
        ssize_t result = 0;
        for (size_t i = 0; i < num_shards; ++i) {
            result += shard[i].allocated_bytes;
        }

        if (result < 0) {
            // BUG: unbalance allocation/deallocation
            result = 0;
        }
        return (size_t) result;
    }

    size_t allocated_items() const {
        ssize_t result = 0;
        for (size_t i = 0; i < num_shards; ++i) {
            result += shard[i].allocated_items;
        }

        if (result < 0) {
            // BUG: unbalance allocation/deallocation
            result = 0;
        }
        return (size_t) result;
    }

    void adjust_count(ssize_t adjust_allocated_items, ssize_t adjust_allocated_bytes) {
        shard_t *shard = pick_a_shard();
        shard->allocated_items += adjust_allocated_items;
        shard->allocated_bytes += adjust_allocated_bytes;
    }

    object_attr *get_type(const std::type_info& ti, size_t item_size) {
        std::lock_guard<std::mutex> lk(lock);

        auto p = object_type_map.find(ti.name());
        if (p != object_type_map.end()) {
            return &p->second;
        }

        object_attr &t = object_type_map[ti.name()];
        t.type_name = ti.name();
        t.item_size = item_size;
        return &t;
    }
};

extern pool_type& get_pool(pool_type_id pool_index);

// STL allocator for use with containers.  All actual state
// is stored in the static pool_allocator_base_t, which saves us from
// passing the allocator to container constructors.
template<pool_type_id pool_index, typename T>
class pool_allocator {
    pool_type *pool;
    object_attr *type = nullptr;

public:
    using allocator_type = pool_allocator<pool_index, T>;
    using value_type = T;

    using pointer = value_type *;
    using const_pointer = const value_type *;

    using reference = value_type&;
    using const_reference = const value_type&;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U> struct rebind {
        using other = pool_allocator<pool_index, U>;
    };

    void init(bool force_register) {
        pool = &get_pool(pool_index);
        if (debug_mode || force_register) {
            type = pool->get_type(typeid(T), sizeof(T));
        }
    }

    pool_allocator(bool force_register = false) {
        init(force_register);
    }

    template<typename U>
    pool_allocator(const pool_allocator<pool_index, U>&) {
        init(false);
    }

    T* allocate(size_t n) {
        size_t allocating_size = sizeof(T) * n;
        T* r = reinterpret_cast<T*>(new char[allocating_size]);

        shard_t *shard = pool->pick_a_shard();
        shard->allocated_bytes += allocating_size;
        shard->allocated_items += n;
        if (type) {
            type->object_items += n;
        }
        return r;
    }

    void deallocate(T* p, size_t n) {
        size_t releasing_size = sizeof(T) * n;

        shard_t *shard = pool->pick_a_shard();
        shard->allocated_bytes -= releasing_size;
        shard->allocated_items -= n;
        if (type) {
          type->object_items -= n;
        }

        delete[] reinterpret_cast<char*>(p);
    }

    T* allocate_aligned(size_t n, size_t align) {
        size_t allocating_size = sizeof(T) * n;

        char *ptr;
        int rc = ::posix_memalign((void**)(void*)&ptr, align, allocating_size);
        if (rc) {
          throw std::bad_alloc();
        }

        shard_t *shard = pool->pick_a_shard();
        shard->allocated_bytes += allocating_size;
        shard->allocated_items += n;
        if (type) {
          type->object_items += n;
        }

        T* r = reinterpret_cast<T*>(ptr);
        return r;
    }

    void deallocate_aligned(T* p, size_t n) {
        size_t releasing_size = sizeof(T) * n;

        shard_t *shard = pool->pick_a_shard();
        shard->allocated_bytes -= releasing_size;
        shard->allocated_items -= n;
        if (type) {
          type->object_items -= n;
        }

        ::free(p);
    }

    void destroy(T* p) {
        p->~T();
    }

    template<typename U>
    void destroy(U *p) {
        p->~U();
    }

    void construct(T* p, const T& val) {
        ::new ((void *)p) T(val); //placement new
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new((void *)p) U(std::forward<Args>(args)...);
    }

    bool operator==(const pool_allocator&) const {
        return true;
    }

    bool operator!=(const pool_allocator&) const {
        return false;
    }
};


// Namespace mempool

#define P(x)                                                                      \
namespace x {                                                                     \
    static const mempool::pool_type_id id = mempool::mempool_##x;                 \
                                                                                  \
    template<typename v>                                                          \
    using pool_allocator = mempool::pool_allocator<id, v>;                        \
                                                                                  \
    using string = std::basic_string<char, std::char_traits<char>,                \
                                     pool_allocator<char>>;                       \
                                                                                  \
    template<typename key, typename value, typename cmp = std::less<key>>         \
    using map = std::map<key, value, cmp,                                         \
                         pool_allocator<std::pair<const key, value>>>;            \
                                                                                  \
    template<typename key, typename value, typename cmp = std::less<key>>         \
    using compact_map = compact_map<key, value, cmp,                              \
                                    pool_allocator<std::pair<const key, value>>>; \
                                                                                  \
    template<typename key, typename value, typename cmp = std::less<key>>         \
    using compact_multimap = compact_multimap<key, value, cmp,                    \
                                    pool_allocator<std::pair<const key, value>>>; \
                                                                                  \
    template<typename key, typename value, typename cmp = std::less<key>>         \
    using multimap = std::multimap<key, value, cmp,                               \
                                   pool_allocator<std::pair<const key, value>>>;  \
                                                                                  \
    template<typename key, typename value, typename h = std::hash<key>,           \
             typename eq = std::equal_to<key>>                                    \
    using unordered_map = std::unordered_map<key, value, h, eq,                   \
                                  pool_allocator<std::pair<const key, value>>>;   \
                                                                                  \
    template<typename key, typename cmp = std::less<key>>                         \
    using set = std::set<key, cmp, pool_allocator<key>>;                          \
                                                                                  \
    template<typename key, typename cmp = std::less<key>>                         \
    using compact_set = compact_set<key, cmp, pool_allocator<key>>;               \
                                                                                  \
    template<typename value>                                                      \
    using list = std::list<value, pool_allocator<value>>;                         \
                                                                                  \
    template<typename value>                                                      \
    using vector = std::vector<value, pool_allocator<value>>;                     \
                                                                                  \
    inline size_t allocated_bytes() {                                             \
        return mempool::get_pool(id).allocated_bytes();                           \
    }                                                                             \
    inline size_t allocated_items() {                                             \
        return mempool::get_pool(id).allocated_items();                           \
    }                                                                             \
}

DEFINE_MEMORY_POOLS_HELPER(P)

#undef P
} //namespace: mempool

template<typename T, mempool::pool_type_id pool_index>
bool operator==(const std::vector<T, std::allocator<T>>& lhs,
                const std::vector<T, mempool::pool_allocator<pool_index, T>>& rhs) {
    return (lhs.size() == rhs.size() &&
            std::equal(lhs.begin(), lhs.end(), rhs.begin()));
}

template<typename T, mempool::pool_type_id pool_index>
bool operator!=(const std::vector<T, std::allocator<T>>& lhs,
                const std::vector<T, mempool::pool_allocator<pool_index, T>>& rhs) {
    return !(lhs == rhs);
}

template<typename T, mempool::pool_type_id pool_index>
bool operator==(const std::vector<T, mempool::pool_allocator<pool_index, T>>& lhs,
                const std::vector<T, std::allocator<T>>& rhs) {
    return rhs == lhs;
}

template<typename T, mempool::pool_type_id pool_index>
bool operator!=(const std::vector<T, mempool::pool_allocator<pool_index, T>>& lhs,
                const std::vector<T, std::allocator<T>>& rhs) {
    return !(lhs == rhs);
}

// Use this for any type that is contained by a container (unless it
// is a class you defined; see below).
#define MEMPOOL_DECLARE_FACTORY(obj, factoryname, pool)                \
namespace mempool {                                                    \
    namespace pool {                                                   \
        extern pool_allocator<obj> alloc_##factoryname;                \
    }                                                                  \
}

#define MEMPOOL_DEFINE_FACTORY(obj, factoryname, pool)                 \
namespace mempool {                                                    \
    namespace pool {                                                   \
        pool_allocator<obj> alloc_##factoryname = {true};              \
    }                                                                  \
}

#define MEMPOOL_CLASS_HELPERS()                                        \
    void *operator new(size_t size);                                   \
                                                                       \
    void *operator new[](size_t size) noexcept {                       \
        spec_abort_msg("no array new");                                \
        return nullptr;                                                \
    }                                                                  \
                                                                       \
    void operator delete(void *);                                      \
                                                                       \
    void operator delete[](void *) {                                   \
        spec_abort_msg("no array delete");                             \
    }


#define MEMPOOL_DEFINE_OBJECT_FACTORY(obj, factoryname, pool)             \
                                                                          \
    MEMPOOL_DEFINE_FACTORY(obj, factoryname, pool)                        \
                                                                          \
    void *obj::operator new(size_t size) {                                \
        return mempool::pool::alloc_##factoryname.allocate(1);            \
    }                                                                     \
                                                                          \
    void obj::operator delete(void *p)  {                                 \
        return mempool::pool::alloc_##factoryname.deallocate((obj*)p, 1); \
    }

#endif //SPEC_INCLUDE_MEMPOOL_H
