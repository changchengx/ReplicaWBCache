/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef ARCH_PROBE_H
#define ARCH_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

extern int arch_probed;  /* non-zero if the arch feature is probed*/

extern int probe_arch(void);

#ifdef __cplusplus
}
#endif

#endif
