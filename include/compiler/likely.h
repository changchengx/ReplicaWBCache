/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_COMPILE_LIKELY_H
#define SPEC_COMPILE_LIKELY_H

/*
 * compiler likely&unlikely macros
 */
#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif
#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif
#ifndef expect
#define expect(x, hint) __builtin_expect((x),(hint))
#endif

#endif //SPEC_COMPILE_LIKELY_H
