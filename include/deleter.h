/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_COMMON_DELETER_H
#define SPEC_COMMON_DELETER_H

#include <atomic>
#include <cstdlib>
#include <new>

/* A deleter object is used to inform the consumer of some buffer
 * how to delete the buffer.
 * This is be done by calling some pre-defined function or destroying
 * an object carried by the delete.
 * Examples of a deleter's encapsulated actions are:
 *  - call std::free(p) on some captured pointer
 *  - call delete p on some captured pointer
 *  - decrement a reference count
 * The deleter object performs its action from its destructor.
 */
class deleter final {
public:
    struct impl;
    struct raw_object_tag {};

private:
    impl* _impl = nullptr;
    deleter(const deleter&) = delete;
    deleter& operator=(deleter&) = delete;

public:
    // an empty deleter does nothing in its destructor.
    deleter() = default;

    explicit deleter(impl* i) : _impl(i) {
    }

    // Moves a deleter.
    deleter(deleter&& other) noexcept
        : _impl(other._impl) {
        other._impl = nullptr;
    }

    deleter(raw_object_tag tag, void* object)
        : _impl(form_raw_object(object)) {
    }

    // Destroys the deleter and carries out the encapsulated action.
    ~deleter();

    deleter& operator=(deleter&& other);

    /* Performs a sharing operation. The encapsulated action will only
     * be carried out after both the original deleter and the returned
     * deleter are both destroyed.
     *
     * return a deleter with the same encapsulated action as this one.
     */
    deleter share();

    // Checks whether the deleter has an associated action.
    explicit operator bool() const {
        return static_cast<bool>(_impl);
    }

    void reset(impl* i) {
        this->~deleter();
        new (this) deleter(i);
    }

    /* Appends another deleter to this deleter. When this deleter is
     * destroyed, both encapsulated actions will be carried out.
     */
    void append(deleter d);

private:
    static bool is_raw_object(impl* i) {
        auto x = reinterpret_cast<uintptr_t>(i);
        return x & 1;
    }

    bool is_raw_object() const {
        return is_raw_object(_impl);
    }

    static void* raw_object_restore(impl* i) {
        auto x = reinterpret_cast<uintptr_t>(i);
        return reinterpret_cast<void*>(x & ~uintptr_t(1));
    }

    void* raw_object_restore() const {
        return raw_object_restore(_impl);
    }

    impl* form_raw_object(void* object) {
        auto x = reinterpret_cast<uintptr_t>(object);
        return reinterpret_cast<impl*>(x | 1);
    }
};

struct deleter::impl {
    std::atomic_uint refs;

    deleter next;

    impl(deleter next)
        : refs(1), next(std::move(next)) {
    }

    virtual ~impl() {
    }
};

inline deleter::~deleter() {
    if (is_raw_object()) {
        std::free(raw_object_restore());
        _impl = nullptr;
        return;
    }
    if (_impl && --_impl->refs == 0) {
        delete _impl;
        _impl = nullptr;
    }
}

inline deleter& deleter::operator=(deleter&& other) {
    if (this != &other) {
        this->~deleter();
        new (this) deleter(std::move(other));
    }
    return *this;
}

template <typename Deleter>
struct lambda_deleter_impl final : deleter::impl {
    Deleter del;

    lambda_deleter_impl(deleter next, Deleter&& del)
        : impl(std::move(next)), del(std::move(del)) {
    }

    ~lambda_deleter_impl() override {
        del();
    }
};

template <typename Object>
struct object_deleter_impl final : deleter::impl {
    Object obj;

    object_deleter_impl(deleter next, Object&& obj)
        : impl(std::move(next)), obj(std::move(obj)) {
    }
};

template <typename Object>
inline
object_deleter_impl<Object>* make_object_deleter_impl(deleter next, Object obj) {
    return new object_deleter_impl<Object>(std::move(next), std::move(obj));
}

/* Makes a deleter object that encapsulates the action of destroying an object and
 * running another deleter.
 * The input object is moved to the deleter and destroyed when the deleter is destroyed.
 *
 * The deleter "next" will become part of the new deleter's encapsulated action.
 * The Object "obj", its destructor becomes part of the new deleter's encapsulated action.
 */
template <typename Object>
deleter make_deleter(deleter next, Object obj) {
    return deleter(new lambda_deleter_impl<Object>(std::move(next), std::move(obj)));
}

/* Makes a deleter object that encapsulates the action of destroying an object.
 * The input object is moved to the deleter and destroyed when the deleter is destroyed.

 * The Object "obj", its destructor becomes the new deleter's encapsulated action
 */
template <typename Object>
deleter make_deleter(Object obj) {
    return make_deleter(deleter(), std::move(obj));
}

struct free_deleter_impl final : deleter::impl {
    void* obj;

    free_deleter_impl(void* obj)
        : impl(deleter()), obj(obj) {
    }

    ~free_deleter_impl() override {
        std::free(obj);
    }
};

inline deleter deleter::share() {
    if (!_impl) {
        return deleter();
    }

    if (is_raw_object()) {
        _impl = new free_deleter_impl(raw_object_restore());
    }
    ++_impl->refs;
    return deleter(_impl);
}

/* Append 'd' to the chain of deleters.
 * For performance reasons, the current chain should be shorter and
 * 'd' should be longer.
 */
inline
void deleter::append(deleter d) {
    if (!d._impl) {
        return;
    }

    impl* next_impl = _impl;
    deleter* next_d = this;
    while (next_impl) {
        if (next_impl == d._impl) {
            return;
        }

        if (is_raw_object(next_impl)) {
            next_d->_impl = next_impl = new free_deleter_impl(raw_object_restore(next_impl));
        }

        if (next_impl->refs != 1) {
            next_d->_impl = next_impl = make_object_deleter_impl(std::move(next_impl->next), deleter(next_impl));
        }

        next_d = &next_impl->next;
        next_impl = next_d->_impl;
    }

    next_d->_impl = d._impl;
    d._impl = nullptr;
}

/* Makes a deleter object that calls std::free() when it is destroyed.
 *
 * The "obj" object is to be released.
 */
inline
deleter make_free_deleter(void* obj) {
    if (!obj) {
        return deleter();
    }
    return deleter(deleter::raw_object_tag(), obj);
}

/* Makes a deleter object that calls std::free() when it is destroyed,
 * as well as invoking the encapsulated action of another deleter.
 *
 * The "obj" object is to be released.
 * The "next" deleter is to be invoked.
 */
inline
deleter make_free_deleter(deleter next, void* obj) {
    return make_deleter(std::move(next), [obj] () mutable { std::free(obj); });
}

template <typename T>
inline
deleter make_object_deleter(T&& obj) {
    return deleter{make_object_deleter_impl(deleter(), std::move(obj))};
}

template <typename T>
inline
deleter make_object_deleter(deleter del, T&& obj) {
    return deleter{make_object_deleter_impl(std::move(del), std::move(obj))};
}

#endif /* SPEC_COMMON_DELETER_H */
