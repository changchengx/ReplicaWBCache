/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */
#ifndef INLINE_MEMORY_H
#define INLINE_MEMORY_H

#include <cstring>
#include <cstdint>

namespace {
#if defined(__GNUC__)

//small size copy optimization
__attribute__((always_inline))
inline void *maybe_inline_memcpy(void *dest, const void *src, size_t len, size_t small_len) {
    if (len > small_len) {
        return memcpy(dest, src, len);
    }

    switch (len) {
    case 8:
        return __builtin_memcpy(dest, src, 8);
    case 4:
        return __builtin_memcpy(dest, src, 4);
    case 3:
        return __builtin_memcpy(dest, src, 3);
    case 2:
        return __builtin_memcpy(dest, src, 2);
    case 1:
        return __builtin_memcpy(dest, src, 1);
    default:
        int cursor = 0; //right under c++ grammar
        while (len >= sizeof(uint64_t)) {
            __builtin_memcpy((char*)dest + cursor, (char*)src + cursor, sizeof(uint64_t));
            cursor += sizeof(uint64_t);
            len -= sizeof(uint64_t);
        }
        while (len >= sizeof(uint32_t)) {
            __builtin_memcpy((char*)dest + cursor, (char*)src + cursor, sizeof(uint32_t));
            cursor += sizeof(uint32_t);
            len -= sizeof(uint32_t);
        }
        while (len > 0) {
            *((char*)dest + cursor) = *((char*)src + cursor);
            cursor++;
            len--;
        }
    }

    return dest;
}

#if defined(__x86_64__)

__attribute__((always_inline))
inline bool mem_is_zero(const char *data, size_t len)
{
    typedef unsigned uint128_t __attribute__ ((mode (TI)));
    if (len / sizeof(uint128_t) > 0) {
        while (((unsigned long long)data) & 15) {
            if (*(uint8_t*)data != 0) { // use x86::XMM registers
	            return false;
            }
            data += sizeof(uint8_t);
            --len;
        }

        const char* data_start = data;
        const char* max128 = data +
                             (len / sizeof(uint128_t)) * sizeof(uint128_t);

        while (data < max128) {
            if (*(uint128_t*)data != 0) {
	            return false;
            }
            data += sizeof(uint128_t);
        }
        len -= (data - data_start);
    }

    const char* max = data + len;
    const char* max32 = data + (len / sizeof(uint32_t)) * sizeof(uint32_t);
    while (data < max32) {
        if (*(uint32_t*)data != 0) {
            return false;
        }
        data += sizeof(uint32_t);
    }

    while (data < max) {
        if (*(uint8_t*)data != 0) {
            return false;
        }
        data += sizeof(uint8_t);
    }
    return true;
}

#endif // defined(__GNUC__) && defined(__x86_64__)

#else // defined(__GNUC__)

#define maybe_inline_memcpy(d, s, l, x) memcpy(d, s, l)
inline bool mem_is_zero(const char *data, size_t len) {
    const char *end = data + len;
    const char* end64 = data + (len / sizeof(uint64_t)) * sizeof(uint64_t);

    while (data < end64) {
        if (*(uint64_t*)data != 0) {
            return false;
        }
        data += sizeof(uint64_t);
    }

    while (data < end) {
        if (*data != 0) {
            return false;
        }
        ++data;
    }
    return true;
}
#endif  // !defined(__GNUC__)
}

#endif // INLINE_MEMORY_H
