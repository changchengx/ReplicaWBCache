/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_CLOCK_H
#define SPEC_CLOCK_H

#include "../time/utime.h"

static inline utime_t spec_clock_now()
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    utime_t n(tp);
    return n;
}

#endif //SPEC_CLOCK_H
