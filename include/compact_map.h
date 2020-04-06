/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_COMPACT_MAP_H
#define SPEC_COMPACT_MAP_H

#include <iostream>
#include <memory>
#include <map>

#include "spec_assert/spec_assert.h"

template <class Key, class T, class Map>
class compact_map_base {
protected:
    std::unique_ptr<Map> m_map;

    void alloc_internal() {
        if (!m_map) {
            m_map.reset(new Map);
        }
    }
    void free_internal() {
        m_map.reset();
    }

    template <class It>
    class iterator_base {
    private:
        friend class compact_map_base;

        const compact_map_base* _map;
        It _it;

        iterator_base(const compact_map_base* map = nullptr)
            : _map(map) {
        }
        iterator_base(const compact_map_base* map, const It& it)
            : _map(map), _it(it) {
        }

    public:
        iterator_base(const iterator_base& other): _map(other._map), _it(other._it) {
        }

        bool operator==(const iterator_base& other) const {
            return (_map == other._map) && (!_map->m_map || _it == other._it);
        }

        bool operator!=(const iterator_base& other) const {
            return !(*this == other);
        }

        iterator_base& operator=(const iterator_base& other) {
            _map = other._map;
            _it = other._it;
            return *this;
        }

        iterator_base& operator++() {
            ++_it;
            return *this;
        }

        iterator_base operator++(int) {
            iterator_base tmp = *this;
            ++_it;
            return tmp;
        }

        iterator_base& operator--() {
            --_it;
            return *this;
        }

        const std::pair<const Key, T>& operator*() {
            return *_it;
        }

        const std::pair<const Key, T>* operator->() {
            return _it.operator->();
        }

    };

public:
    class const_iterator : public iterator_base<typename Map::const_iterator> {
    public:
        const_iterator() {
        }

        const_iterator(const iterator_base<typename Map::const_iterator>& other)
            : iterator_base<typename Map::const_iterator>(other) {
        }

        const_iterator(const compact_map_base* map)
            : iterator_base<typename Map::const_iterator>(map) {
        }
        const_iterator(const compact_map_base* map,
                       const typename Map::const_iterator& it)
            : iterator_base<typename Map::const_iterator>(map, it) {
        }
    };

    class iterator : public iterator_base<typename Map::iterator> {
    public:
        iterator() {
        }

        iterator(const iterator_base<typename Map::iterator>& other)
            : iterator_base<typename Map::iterator>(other) {
        }

        iterator(compact_map_base* map)
            : iterator_base<typename Map::iterator>(map) {
        }

        iterator(compact_map_base* map, const typename Map::iterator& it)
            : iterator_base<typename Map::iterator>(map, it) {
        }

        operator const_iterator() const {
            return const_iterator(this->_map, this->_it);
        }
    };

    class const_reverse_iterator
        : public iterator_base<typename Map::const_reverse_iterator> {
    public:
        const_reverse_iterator() {
        }

        const_reverse_iterator(const iterator_base<typename Map::const_reverse_iterator>& other)
            : iterator_base<typename Map::const_reverse_iterator>(other) {
        }

        const_reverse_iterator(const compact_map_base* map)
            : iterator_base<typename Map::const_reverse_iterator>(map) {
        }
        const_reverse_iterator(const compact_map_base* map,
                               const typename Map::const_reverse_iterator& it)
            : iterator_base<typename Map::const_reverse_iterator>(map, it) {
        }
    };

    class reverse_iterator : public iterator_base<typename Map::reverse_iterator> {
    public:
        reverse_iterator() {
        }

        reverse_iterator(const iterator_base<typename Map::reverse_iterator>& other)
            : iterator_base<typename Map::reverse_iterator>(other) {
        }

        reverse_iterator(compact_map_base* map)
            : iterator_base<typename Map::reverse_iterator>(map) {
        }

        reverse_iterator(compact_map_base* map, const typename Map::reverse_iterator& it)
            : iterator_base<typename Map::reverse_iterator>(map, it) {
        }

        operator const_iterator() const {
            return const_iterator(this->_map, this->_it);
        }
    };

    compact_map_base() {
    }

    compact_map_base(const compact_map_base& other) {
        if (other.m_map) {
            alloc_internal();
            *m_map = *other.m_map;
        }
    }

    ~compact_map_base() {
    }

    bool empty() const {
        return !m_map || m_map->empty();
    }
    size_t size() const {
        return m_map ? m_map->size() : 0;
    }
    bool operator==(const compact_map_base& other) const {
        return (empty() && other.empty()) ||
               (m_map && other.m_map && *m_map == *other.m_map);
    }
    bool operator!=(const compact_map_base& other) const {
        return !(*this == other);
    }
    size_t count (const Key& k) const {
        return m_map ? m_map->count(k) : 0;
    }
    iterator erase (iterator p) {
        if (m_map) {
            spec_assert(this == p._map);
            auto it = m_map->erase(p._it);
            if (m_map->empty()) {
                free_internal();
                return iterator(this);
            } else {
                return iterator(this, it);
            }
        } else {
            return iterator(this);
        }
    }

    size_t erase (const Key& k) {
        if (!m_map) {
            return 0;
        }
        size_t r = m_map->erase(k);
        if (m_map->empty()) {
            free_internal();
        }
        return r;
    }
    void clear() {
        free_internal();
    }
    void swap(compact_map_base& other) {
        m_map.swap(other.m_map);
    }
    compact_map_base& operator=(const compact_map_base& other) {
        if (other.m_map) {
            alloc_internal();
            *m_map = *other.m_map;
        } else {
            free_internal();
        }
        return *this;
    }

    iterator insert(const std::pair<const Key, T>& val) {
        alloc_internal();
        return iterator(this, m_map->insert(val));
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace (Args&&... args) {
        alloc_internal();
        auto em = m_map->emplace(std::forward<Args>(args)...);
        return std::make_pair(iterator(this, em.first), em.second);
    }

    iterator begin() {
        if (!m_map) {
            return iterator(this);
        }
        return iterator(this, m_map->begin());
    }
    iterator end() {
        if (!m_map) {
            return iterator(this);
        }
        return iterator(this, m_map->end());
    }
    iterator find(const Key& k) {
        if (!m_map) {
            return iterator(this);
        }
        return iterator(this, m_map->find(k));
    }
    iterator lower_bound(const Key& k) {
        if (!m_map) {
            return iterator(this);
        }
        return iterator(this, m_map->lower_bound(k));
    }
    iterator upper_bound(const Key& k) {
        if (!m_map) {
            return iterator(this);
        }
        return iterator(this, m_map->upper_bound(k));
    }

    const_iterator begin() const {
        if (!m_map) {
            return const_iterator(this);
        }
        return const_iterator(this, m_map->begin());
    }
    const_iterator end() const {
        if (!m_map) {
            return const_iterator(this);
        }
        return const_iterator(this, m_map->end());
    }
    const_iterator find(const Key& k) const {
        if (!m_map) {
            return const_iterator(this);
        }
        return const_iterator(this, m_map->find(k));
    }
    const_iterator lower_bound(const Key& k) const {
        if (!m_map) {
            return const_iterator(this);
        }
        return const_iterator(this, m_map->lower_bound(k));
    }
    const_iterator upper_bound(const Key& k) const {
        if (!m_map) {
            return const_iterator(this);
        }
        return const_iterator(this, m_map->upper_bound(k));
    }

    reverse_iterator rbegin() {
        if (!m_map) {
            return reverse_iterator(this);
        }
        return reverse_iterator(this, m_map->rbegin());
    }
    reverse_iterator rend() {
        if (!m_map) {
            return reverse_iterator(this);
        }
        return reverse_iterator(this, m_map->rend());
    }

    const_reverse_iterator rbegin() const {
        if (!m_map) {
            return const_reverse_iterator(this);
        }
        return const_reverse_iterator(this, m_map->rbegin());
    }
    const_reverse_iterator rend() const {
        if (!m_map) {
            return const_reverse_iterator(this);
        }
        return const_reverse_iterator(this, m_map->rend());
    }
};

template <typename Key, typename T,
          typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, T>>>
class compact_map
    : public compact_map_base<Key, T, std::map<Key, T, Compare, Alloc>> {
public:
    T& operator[](const Key& key) {
        this->alloc_internal();
        return (*(this->map))[key];
    }
};

template <typename Key, typename T,
          typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, T>>>
inline std::ostream& operator<<(std::ostream& os,
                                const compact_map<Key, T, Compare, Alloc>& m)
{
  os << "{";

  auto it = m.cbegin();
  if (it != m.cend()) {
      os << it->first << " = " << it->second;
      ++it;
  }
  while (it != m.cend()) {
      os << ", "  << it->first << " = " << it->second;
      ++it;
  }

  os << "}";
  return os;
}

template <typename Key, typename T,
          typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, T>>>
class compact_multimap
    : public compact_map_base<Key, T, std::multimap<Key, T, Compare, Alloc>> {
};

template <typename Key, typename T,
          typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, T>>>
inline std::ostream& operator<<(std::ostream& os,
                                const compact_multimap<Key, T, Compare, Alloc>& m)
{
  os << "{{";

  auto it = m.cbegin();
  if (it != m.cend()) {
      os << it->first << " = " << it->second;
      ++it;
  }
  while (it != m.cend()) {
      os << ", "  << it->first << " = " << it->second;
      ++it;
  }

  os << "}}";
  return os;
}
#endif //SPEC_COMPACT_MAP_H
