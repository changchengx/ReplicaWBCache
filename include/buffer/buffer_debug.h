/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_BUFFER_DEBUG_H
#define SPEC_BUFFER_DEBUG_H

#include <utility>

#include "../spinlock/spinlock.h"

#ifdef BUFFER_DEBUG
extern spec::spinlock buffer_debug_lock;
# define bdout { std::lock_guard lg(buffer_debug_lock); std::cout
# define bendl std::endl; }
#else
# define bdout if (0) { std::cout
# define bendl std::endl; }
#endif

#endif //SPEC_BUFFER_DEBUG_H
