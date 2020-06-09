/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef ARCH_INTEL_H
#define ARCH_INTEL_H

#ifdef __cplusplus
extern "C" {
#endif

extern int arch_intel_pclmul;   /* true if it has PCLMUL features */
extern int arch_intel_sse42;    /* true if it has sse 4.2 features */
extern int arch_intel_sse41;    /* true if it has sse 4.1 features */
extern int arch_intel_ssse3;    /* true if it has ssse 3 features */
extern int arch_intel_sse3;     /* true if it has sse 3 features */
extern int arch_intel_sse2;     /* true if it has sse 2 features */
extern int arch_intel_aesni;    /* true if it has aesni features */
extern int arch_intel_avx;      /* ture if it has avx features */
extern int arch_intel_avx2;     /* ture if it has avx2 features */
extern int arch_intel_avx512f;  /* ture if it has avx512 features */
extern int arch_intel_avx512er; /* ture if it has 28-bit RCP, RSQT and EXP transcendentals features */
extern int arch_intel_avx512pf; /* ture if it has avx512 prefetch features */
extern int arch_intel_avx512vl; /* ture if it has avx512 variable length features */
extern int arch_intel_avx512cd; /* ture if it has avx512 conflict detection features */
extern int arch_intel_avx512dq; /* ture if it has new 32-bit and 64-bit AVX-512 instructions */
extern int arch_intel_avx512bw; /* ture if it has new 8-bit and 16-bit AVX-512 instructions */

extern int arch_intel_probe(void);

#ifdef __cplusplus
}
#endif

#endif
