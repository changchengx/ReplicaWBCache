/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <pthread.h>
#include <iostream>
#include <sstream>
#include <stdarg.h>

#include "dout.h"
#include "spec_assert/BackTrace.h"
#include "spec_assert/spec_assert.h"
#include "clock/spec_clock.h"

using std::ostringstream;

namespace spec {

void __spec_assert_fail(const char *assertion, const char *file, int line, const char *func) {
    ostringstream tss;
    tss << spec_clock_now();

    char assert_msg[2048] = {0};
    snprintf(assert_msg, sizeof(assert_msg),
             "file: %s\n"
             "func: %s\n"
             "thread: %llx\n"
             "time: %s\n"
             "%s: %d: FAILED spec_assert(%s)\n",
             file, func, (unsigned long long)pthread_self(), tss.str().c_str(),
             file, line, assertion);
    dout_emergency(assert_msg);

    ostringstream oss;
    oss << BackTrace(1);
    dout_emergency(oss.str());

    abort();
  }

void __spec_assert_fail(const assert_data &ctx) {
    __spec_assert_fail(ctx.assertion, ctx.file, ctx.line, ctx.function);
}

class BufAppender {
private:
    char* bufptr;
    int remaining;

public:
    BufAppender(char* buf, int size) : bufptr(buf), remaining(size) {}

    void printf(const char * format, ...) {
        va_list args;
        va_start(args, format);
        this->vprintf(format, args);
        va_end(args);
    }

    void vprintf(const char * format, va_list args) {
        int n = vsnprintf(bufptr, remaining, format, args);
        if (n >= 0) {
            if (n < remaining) {
                remaining -= n;
                bufptr += n;
            } else {
                remaining = 0;
            }
        }
    }
};

void __spec_assertf_fail(const char *assertion,
                                       const char *file, int line,
                                       const char *func, const char* msg, ...) {
    ostringstream tss;
    tss << spec_clock_now();

    char assert_msg[8096] = {0};
    BufAppender ba(assert_msg, sizeof(assert_msg));
    ba.printf("file: %s\n"
              "func: %s\n"
              "thread: %llx\n"
              "time: %s\n"
              "%s: %d: FAILED spec_assert(%s)\n",
              file, func, (unsigned long long)pthread_self(), tss.str().c_str(),
              file, line, assertion);

    ba.printf("Assertion details: ");
    va_list args;
    va_start(args, msg);
    ba.vprintf(msg, args);
    va_end(args);
    ba.printf("\n");

    dout_emergency(assert_msg);

    ostringstream oss;
    oss << BackTrace(1);
    dout_emergency(oss.str());

    abort();
}

void __spec_abort(const char *file, int line, const char *func, const std::string& msg) {
    ostringstream tss;
    tss << spec_clock_now();

    char assert_msg[4096] = {0};
    snprintf(assert_msg, sizeof(assert_msg),
             "file: %s\n"
             "func: %s\n"
             "thread: %llx\n"
             "time: %s\n"
             "%s: %d: spec_abort_msg(\"%s\")\n", file, func,
             (unsigned long long)pthread_self(),
             tss.str().c_str(), file, line,
             msg.c_str());
    dout_emergency(assert_msg);

    ostringstream oss;
    oss << BackTrace(1);
    dout_emergency(oss.str());

    abort();
}

void __spec_abortf(const char *file, int line, const char *func, const char* msg, ...) {
    ostringstream tss;
    tss << spec_clock_now();

    char assert_msg[8096] = {0};
    BufAppender ba(assert_msg, sizeof(assert_msg));
    ba.printf("file: %s\n"
              "func: %s\n"
              "thread: %llx\n"
              "time: %s\n"
              "%s: %d: abort()\n",
              file, func, (unsigned long long)pthread_self(), tss.str().c_str(),
              file, line);
    ba.printf("Abort details: ");
    va_list args;
    va_start(args, msg);
    ba.vprintf(msg, args);
    va_end(args);
    ba.printf("\n");

    dout_emergency(assert_msg);

    ostringstream oss;
    oss << BackTrace(1);
    dout_emergency(oss.str());

    abort();
}

void __spec_assert_warn(const char *assertion, const char *file, int line, const char *func) {
    char buf[8096];
    snprintf(buf, sizeof(buf),
             "WARNING: spec_assert(%s) at: %s: %d: %s()\n",
             assertion, file, line, func);
    dout_emergency(buf);
}

} //namespace: spec
