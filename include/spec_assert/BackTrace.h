/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_BACKTRACE_H
#define SPEC_BACKTRACE_H

#include <iosfwd>
#include <execinfo.h>
#include <stdlib.h>

namespace spec {

class BackTrace {
public:
    const static int max = 100;

    int skip;
    void *array[max]{};
    size_t size;
    char **strings;

    explicit BackTrace(int s) : skip(s) {
        size = backtrace(array, max);
        strings = backtrace_symbols(array, size);
    }
    ~BackTrace() {
        free(strings);
    }

    BackTrace(const BackTrace& other);
    const BackTrace& operator=(const BackTrace& other);

    void print(std::ostream& out) const;
};

inline std::ostream& operator<<(std::ostream& os, const BackTrace& bt) {
    bt.print(os);
    return os;
}

} //namespace: spec

#endif //SPEC_BACKTRACE_H
