/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <atomic>

namespace spec {

inline void spin_lock(std::atomic_flag& lock) {
    while(lock.test_and_set(std::memory_order_acquire)) {
        continue;
    }
}

inline void spin_lock(std::atomic_flag *lock) {
    spin_lock(*lock);
}

inline void spin_unlock(std::atomic_flag& lock) {
    lock.clear(std::memory_order_release);
}

inline void spin_unlock(std::atomic_flag *lock) {
    spin_unlock(*lock);
}

class spinlock final {
private:
    std::atomic_flag af = ATOMIC_FLAG_INIT;

public:
    void lock() {
        spec::spin_lock(af);
    }

    void unlock() noexcept {
        spec::spin_unlock(af);
    }
};


inline void spin_lock(spec::spinlock& lock) {
    lock.lock();
}

inline void spin_lock(spec::spinlock *lock) {
    spin_lock(*lock);
}

inline void spin_unlock(spec::spinlock& lock) {
    lock.unlock();
}

inline void spin_unlock(spec::spinlock *lock) {
    spin_unlock(*lock);
}

} //namespace spec

#endif //SPINLOCK_H
