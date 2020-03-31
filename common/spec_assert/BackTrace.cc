/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <ostream>
#include <cxxabi.h>
#include <string.h>

#include "spec_assert/BackTrace.h"

namespace spec {

void BackTrace::print(std::ostream& os) const {
    for (size_t i = skip; i < size; i++) {
        size_t sz = 1024; // assumption, template names will go much wider
        char *function = (char *)malloc(sz);
        if (!function) {
            return;
        }

        char *begin = 0, *end = 0;
        static constexpr char OPEN = '(';
        for (char *j = strings[i]; *j; ++j) {
            if (*j == OPEN) {
                begin = j + 1;
            } else if (*j == '+') {
                end = j;
            }
        }

        if (begin && end) {
            int len = end - begin;
            char *foo = (char *)malloc(len+1);
            if (!foo) {
                free(function);
                return;
            }
            memcpy(foo, begin, len);
            foo[len] = 0;

            int status;
            char *ret = nullptr;
            // only demangle a C++ mangled name
            if (foo[0] == '_' && foo[1] == 'Z') {
                ret = abi::__cxa_demangle(foo, function, &sz, &status);
            }
            if (ret) {
                // return value may be a realloc() of the input
                function = ret;
            } else {
                // demangling failed, just pretend it's a C function with no args
                strncpy(function, foo, sz);
                strncat(function, "()", sz);
                function[sz-1] = 0;
            }
            os << " " << (i - skip + 1) << ": " << OPEN << function << end << std::endl;
            free(foo);
        } else {
            // didn't find the mangled name, just print the whole line
            os << " " << (i-skip+1) << ": " << strings[i] << std::endl;
        }
        free(function);
    }
}

} //namespace: spec
