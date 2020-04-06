/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <atomic>

namespace spec {
  template <class T> using atomic = std::atomic<T>;
}
