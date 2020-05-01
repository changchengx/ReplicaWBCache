/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef UNIQUE_LEAKABLE_PTR_H
#define UNIQUE_LEAKABLE_PTR_H

#include <memory>

namespace spec{

// "empty" delete
template <typename T>
class nop_delete {
public:
    void operator()(T*) {}
};

/* Implement a replacement for raw pointer through unique_ptr
 * with "empty" deleter.
 *  1. Raw pointer doesn't have ownership enforcement i.e. std::move
 *  2. default unique_ptr use std::delete deleter
 * This is used to replace raw pointer with ownership enforcement.
 */
template <typename T>
class unique_leakable_ptr: public std::unique_ptr<T, nop_delete<T>> {
public:
   /* inherit parent construction function. */
    using std::unique_ptr<T, nop_delete<T>>::unique_ptr;
};

} //namespace: spec

#endif //UNIQUE_LEAKABLE_PTR_H
