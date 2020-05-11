/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include <memory>

template <typename T>
inline std::shared_ptr<T>
shared_from_base(std::enable_shared_from_this<T>* base) {
  return base->shared_from_this();
}

template <typename T>
inline std::shared_ptr<const T>
shared_from_base(std::enable_shared_from_this<T> const* base) {
  return base->shared_from_this();
}

template <typename T>
inline std::shared_ptr<T>
shared_from(T* derived) {
  return std::static_pointer_cast<T>(shared_from_base(derived));
}

#endif //SHARED_PTR_H
