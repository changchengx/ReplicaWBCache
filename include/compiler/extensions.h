/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_COMPILER_EXTENSIONS_H
#define SPEC_COMPILER_EXTENSIONS_H

/* Take advantage of nice nonstandard features of gcc
 * and other compilers, but still maintain portability.
 */
#ifdef __GNUC__
// GCC
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
// some other compiler - just make it a no-op
#define WARN_UNUSED_RESULT
#endif

#endif //SPEC_COMPILER_EXTENSIONS_H
