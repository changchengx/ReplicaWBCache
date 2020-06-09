/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include "arch/probe_arch.h"
#include "arch/intel.h"

int probe_arch(void) {
    if (arch_probed) {
        return 1;
    }
#if defined(__i386__) || defined(__x86_64__)
    arch_intel_probe();
#endif
    arch_probed = 1;
    return 1;
}

// c++ tricky: initialize global var first
int arch_probed = probe_arch();
