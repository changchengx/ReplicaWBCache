/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <stdint.h>

#include "crc32c_intel_baseline.h"

extern unsigned int crc32_iscsi_00(unsigned char const *buffer, uint64_t len, uint64_t crc) asm("crc32_iscsi_00");

extern unsigned int crc32_iscsi_zero_00(unsigned char const *buffer, uint64_t len, uint64_t crc) asm("crc32_iscsi_zero_00");

#ifdef HAVE_GOOD_YASM_ELF64

uint32_t crc32c_intel_fast(uint32_t crc, unsigned char const *buffer, unsigned len) {
	uint32_t v;
	unsigned left;

	if (!buffer) {
	    return crc32_iscsi_zero_00(buffer, len, crc);
	}

	/*
	 * the crc32_iscsi_00 method reads past buffer+len (because it
	 * reads full words) which makes valgrind unhappy.  don't do
	 * that.
	 */
	if (len < 16) {
		return crc32c_intel_baseline(crc, buffer, len);
    }
	left = ((unsigned long)buffer + len) & 7;
	len -= left;
	v = crc32_iscsi_00(buffer, len, crc);
	if (left) {
		v = crc32c_intel_baseline(v, buffer + len, left);
    }

	return v;
}

int crc32c_intel_fast_exists(void) {
	return 1;
}

#else

int crc32c_intel_fast_exists(void) {
	return 0;
}

uint32_t crc32c_intel_fast(uint32_t crc, unsigned char const *buffer, unsigned len) {
	return 0;
}

#endif
