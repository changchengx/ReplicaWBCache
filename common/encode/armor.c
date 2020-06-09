/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#if defined(__linux__)
#include <linux/errno.h>
#else
#include <sys/errno.h>
#endif

#include "encode/armor.h"

static int encode_bits(int c) {
/*
 * base64 encode/decode.
 */
    static const char *pem_key = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "0123456789+/";
	return pem_key[c];
}

static int decode_bits(char c)
{
	if (c >= 'A' && c <= 'Z') {
		return c - 'A';
    }
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 26;
    }
	if (c >= '0' && c <= '9') {
		return c - '0' + 52;
    }
	if (c == '+' || c == '-') {
		return 62;
    }
	if (c == '/' || c == '_') {
		return 63;
    }
	if (c == '=') {
		return 0; /* just non-negative, please */
    }
	return -EINVAL;
}

static int set_str_val(char **dst, const char *dst_end, char c)
{
	if (*dst < dst_end) {
		char *p = *dst;
		*p = c;
		(*dst)++;
	} else {
		return -ERANGE;
    }
	return 0;
}

uint64_t spec_armor_line_break(char *dst, const char *dst_end,
                               const char *src, const char *src_end,
                               int line_width) {
	uint64_t out_len = 0;
	uint64_t track_line = 0;

#define SET_DST(c) do { \
	int __ret = set_str_val(&dst, dst_end, c); \
	if (__ret < 0) return __ret; \
} while (0);

	while (src < src_end) {
		unsigned char a;

		a = *src++;
		SET_DST(encode_bits(a >> 2));
		if (src < src_end) {
			unsigned char b;
			b = *src++;
			SET_DST(encode_bits(((a & 3) << 4) | (b >> 4)));
			if (src < src_end) {
				unsigned char c;
				c = *src++;
				SET_DST(encode_bits(((b & 15) << 2) |
								(c >> 6)));
				SET_DST(encode_bits(c & 63));
			} else {
				SET_DST(encode_bits((b & 15) << 2));
				SET_DST('=');
			}
		} else {
			SET_DST(encode_bits(((a & 3) << 4)));
			SET_DST('=');
			SET_DST('=');
		}
		out_len += 4;
		track_line += 4;
		if (line_width && track_line == line_width) {
			track_line = 0;
			SET_DST('\n');
			out_len++;
		}
	}
	return out_len;
}

uint64_t spec_armor(char *dst, const char *dst_end,
                    const char *src, const char *src_end) {
	return spec_armor_line_break(dst, dst_end, src, src_end, 0);
}

int64_t spec_unarmor(char *dst, const char *dst_end,
                     const char *src, const char *src_end) {
	int64_t out_len = 0;

	while (src < src_end) {
		int a, b, c, d;

		if (src[0] == '\n') {
			src++;
			continue;
		}

		if (src + 4 > src_end) {
			return -EINVAL;
        }
		a = decode_bits(src[0]);
		b = decode_bits(src[1]);
		c = decode_bits(src[2]);
		d = decode_bits(src[3]);
		if (a < 0 || b < 0 || c < 0 || d < 0) {
			return -EINVAL;
        }

		SET_DST((a << 2) | (b >> 4));
		if (src[2] == '=') {
			return out_len + 1;
        }
		SET_DST(((b & 15) << 4) | (c >> 2));
		if (src[3] == '=') {
			return out_len + 2;
        }
		SET_DST(((c & 3) << 6) | d);
		out_len += 3;
		src += 4;
	}
	return out_len;
}
