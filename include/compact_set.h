/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_COMPACT_SET_H
#define SPEC_COMPACT_SET_H

#include <iostream>
#include <memory>
#include <set>

#include "spec_assert/spec_assert.h"

template <typename T, typename SET>
class compact_set_base {
protected:
    std::unique_ptr<SET> m_set;

    void alloc_internal() {
        if (!m_set)
            m_set.reset(new SET);
    }

    void free_internal() {
        m_set.reset();
    }

    template <class It>
    class iterator_base {
    private:
        friend class compact_set_base;

        const compact_set_base* _set;
        It _it;

        iterator_base(const compact_set_base* set = nullptr) : _set(set) {
        }
        iterator_base(const compact_set_base* set, const It& it) : _set(set), _it(it) {
        }

    public:
        iterator_base(const iterator_base& other): _set(other._set), _it(other._it) {
        }

        bool operator==(const iterator_base& other) const {
            return (_set == other._set) && (!_set->m_set || _it == other._it);
        }

        bool operator!=(const iterator_base& other) const {
            return !(*this == other);
        }

        iterator_base& operator=(const iterator_base& other) {
          _set->m_set = other._set;
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

        const T& operator*() {
            return *_it;
        }
    };

public:
    class const_iterator : public iterator_base<typename SET::const_iterator> {
    public:
        const_iterator() {
        }

        const_iterator(const iterator_base<typename SET::const_iterator>& other)
            : iterator_base<typename SET::const_iterator>(other) {
        }

        const_iterator(const compact_set_base* set)
            : iterator_base<typename SET::const_iterator>(set) {
        }
        const_iterator(const compact_set_base* set,
                       const typename SET::const_iterator& it)
            : iterator_base<typename SET::const_iterator>(set, it) {
        }
    };

    class iterator : public iterator_base<typename SET::iterator> {
    public:
        iterator() {
        }

        iterator(const iterator_base<typename SET::iterator>& other)
            : iterator_base<typename SET::iterator>(other) {
        }

        iterator(compact_set_base* set)
            : iterator_base<typename SET::iterator>(set) {
        }

        iterator(compact_set_base* set, const typename SET::iterator& it)
            : iterator_base<typename SET::iterator>(set, it) {
        }

        operator const_iterator() const {
            return const_iterator(this->_set, this->_it);
        }
    };

    class const_reverse_iterator
        : public iterator_base<typename SET::const_reverse_iterator> {
    public:
        const_reverse_iterator() {
        }

        const_reverse_iterator(const iterator_base<typename SET::const_reverse_iterator>& other)
            : iterator_base<typename SET::const_reverse_iterator>(other) {
        }

        const_reverse_iterator(const compact_set_base* set)
            : iterator_base<typename SET::const_reverse_iterator>(set) {
        }

        const_reverse_iterator(const compact_set_base* set,
                               const typename SET::const_reverse_iterator& it)
            : iterator_base<typename SET::const_reverse_iterator>(set, it) {
        }
    };

    class reverse_iterator : public iterator_base<typename SET::reverse_iterator> {
    public:
        reverse_iterator() {
        }

        reverse_iterator(const iterator_base<typename SET::reverse_iterator>& other)
            : iterator_base<typename SET::reverse_iterator>(other) {
        }

        reverse_iterator(compact_set_base* set)
            : iterator_base<typename SET::reverse_iterator>(set) {
        }

        reverse_iterator(compact_set_base* set,
                         const typename SET::reverse_iterator& it)
            : iterator_base<typename SET::reverse_iterator>(set, it) {
        }

        operator const_iterator() const {
            return const_iterator(this->_set, this->_it);
        }
    };

    compact_set_base() {
    }

    compact_set_base(const compact_set_base& other) {
        if (other.m_set) {
            alloc_internal();
            *m_set = *other.m_set;
        }
    }

    ~compact_set_base() {
    }

    bool empty() const {
        return !m_set || m_set->empty();
    }
    size_t size() const {
      return m_set ? m_set->size() : 0;
    }
    bool operator==(const compact_set_base& other) const {
        return (empty() && other.empty()) ||
               (m_set && other.m_set && *m_set == *other.m_set);
    }
    bool operator!=(const compact_set_base& other) const {
        return !(*this == other);
    }
    size_t count(const T& t) const {
        return m_set ? m_set->count(t) : 0;
    }
    iterator erase(iterator p) {
        if (m_set) {
            spec_assert(this == p._set);
            auto it = m_set->erase(p._it);
            if (m_set->empty()) {
                free_internal();
                return iterator(this);
            } else {
                return iterator(this, it);
            }
        } else {
            return iterator(this);
        }
    }
    size_t erase (const T& t) {
        if (!m_set) {
            return 0;
        }
        size_t r = m_set->erase(t);
        if (m_set->empty()) {
            free_internal();
        }
        return r;
    }
    void clear() {
        free_internal();
    }
    void swap(compact_set_base& other) {
        m_set.swap(other.m_set);
    }
    compact_set_base& operator=(const compact_set_base& other) {
        if (other.m_set) {
            alloc_internal();
            *m_set = *other.m_set;
        } else {
            free_internal();
        }
        return *this;
    }

    std::pair<iterator, bool> insert(const T& t) {
        alloc_internal();
        auto r = m_set->insert(t);
        return std::make_pair(iterator(this, r.first), r.second);
    }
    template <typename... Args>
    std::pair<iterator, bool> emplace ( Args&&... args ) {
        alloc_internal();
        auto em = m_set->emplace(std::forward<Args>(args)...);
        return std::make_pair(iterator(this, em.first), em.second);
    }

    iterator begin() {
        if (!m_set) {
            return iterator(this);
        }
        return iterator(this, m_set->begin());
    }
    iterator end() {
        if (!m_set) {
            return iterator(this);
        }
        return iterator(this, m_set->end());
    }
    iterator find(const T& t) {
        if (!m_set) {
            return iterator(this);
        }
        return iterator(this, m_set->find(t));
    }
    iterator lower_bound(const T& t) {
        if (!m_set) {
            return iterator(this);
        }
        return iterator(this, m_set->lower_bound(t));
    }
    iterator upper_bound(const T& t) {
        if (!m_set) {
            return iterator(this);
        }
        return iterator(this, m_set->upper_bound(t));
    }

    const_iterator begin() const {
        if (!m_set) {
            return const_iterator(this);
        }
        return const_iterator(this, m_set->begin());
    }
    const_iterator end() const {
        if (!m_set) {
            return const_iterator(this);
        }
        return const_iterator(this, m_set->end());
    }
    const_iterator find(const T& t) const {
        if (!m_set) {
            return const_iterator(this);
        }
        return const_iterator(this, m_set->find(t));
    }
    const_iterator lower_bound(const T& t) const {
        if (!m_set) {
            return const_iterator(this);
        }
        return const_iterator(this, m_set->lower_bound(t));
    }
    const_iterator upper_bound(const T& t) const {
        if (!m_set) {
            return const_iterator(this);
        }
        return const_iterator(this, m_set->upper_bound(t));
    }

    reverse_iterator rbegin() {
        if (!m_set) {
            return reverse_iterator(this);
        }
        return reverse_iterator(this, m_set->rbegin());
    }
    reverse_iterator rend() {
        if (!m_set) {
            return reverse_iterator(this);
        }
        return reverse_iterator(this, m_set->rend());
    }

    const_reverse_iterator rbegin() const {
        if (!m_set) {
            return const_reverse_iterator(this);
        }
        return const_reverse_iterator(this, m_set->rbegin());
    }
    const_reverse_iterator rend() const {
        if (!m_set) {
            return const_reverse_iterator(this);
        }
        return const_reverse_iterator(this, m_set->rend());
    }
};

template<typename T,
         typename Compare = std::less<T>,
         typename Alloc = std::allocator<T>>
class compact_set : public compact_set_base<T, std::set<T, Compare, Alloc>> {
};

template<typename T,
         typename Compare = std::less<T>,
         typename Alloc = std::allocator<T>>
inline std::ostream& operator<<(std::ostream& os,
                                const compact_set<T, Compare, Alloc>& s) {
    auto it = s.cbegin();
    if (it != s.cend()) {
        os << *it;
        ++it;
    }
    while(it != s.cend()) {
        os << ", " << *it;
    }
    return os;
}
#endif //SPEC_COMPACT_SET_H
