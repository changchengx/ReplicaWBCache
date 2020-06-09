/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <unistd.h>
#include <stdint.h>

namespace spec {

uint64_t spec_get_bits_of(uint64_t v) {
    uint64_t n = 0;

    while (v) {
        n++;
        v = v >> 1;
    }
    return n;
}

uint64_t spec_page_size = (uint64_t)sysconf(_SC_PAGESIZE);
uint64_t spec_page_mask = ~(uint64_t)(spec_page_size - 1);
uint64_t spec_page_shift = spec_get_bits_of(spec_page_size - 1);

}
