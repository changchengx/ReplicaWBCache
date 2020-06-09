/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_ASSERT_H
#define SPEC_ASSERT_H

#include <cstdlib>
#include <string>

#include <features.h>

#ifndef __STRING
# define __STRING(x) #x
#endif

namespace spec {

class BackTrace;

//https://gcc.gnu.org/onlinedocs/gcc/Function-Names.html
#define __SPEC_ASSERT_FUNCTION __PRETTY_FUNCTION__

struct assert_data {
  const char *assertion;
  const char *file;
  const int line;
  const char *function;
};

extern void __spec_assert_fail(const char *assertion, const char *file, int line, const char *function);
extern void __spec_assert_fail(const assert_data &ctx);

extern void __spec_assertf_fail(const char *assertion, const char *file, int line, const char *function, const char* msg, ...);
extern void __spec_assert_warn(const char *assertion, const char *file, int line, const char *function);

extern void __spec_abort(const char *file, int line, const char *func, const std::string& msg);

extern void __spec_abortf(const char *file, int line, const char *func, const char* msg, ...);

#define _SPEC_ASSERT_VOID_CAST static_cast<void>

#define assert_warn(expr)                                                                                    \
    ((expr) ? _SPEC_ASSERT_VOID_CAST (0)                                                                     \
    : __spec_assert_warn (__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION))

} //namespace: spec

using namespace spec;

/* spec_abort aborts the program with a nice backtrace.
 *
 * Currently, it's the same as assert(0), but we may one day make assert a
 * debug-only thing, like it is in many projects.
 */
#define spec_abort(msg, ...)                                                                                 \
    ::spec::__spec_abort( __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION, "abort() called")

#define spec_abort_msg(msg)                                                                                  \
    ::spec::__spec_abort( __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION, msg)

#define spec_abort_msgf(...)                                                                                 \
    ::spec::__spec_abortf( __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION, __VA_ARGS__)

#ifdef __SANITIZE_ADDRESS__
#define spec_assert(expr)                                                                                    \
    do {                                                                                                     \
        ((expr)) ? _SPEC_ASSERT_VOID_CAST (0)                                                                \
        : ::spec::__spec_assert_fail(__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION);            \
    } while (false)
#else
#define spec_assert(expr)                                                                                    \
    do {                                                                                                     \
        static const spec::assert_data assert_data_ctx =                                                     \
                    {__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION};                            \
        ((expr) ? _SPEC_ASSERT_VOID_CAST (0)                                                                 \
        : ::spec::__spec_assert_fail(assert_data_ctx));                                                      \
    } while(false)
#endif

#ifdef __SANITIZE_ADDRESS__
#define spec_assert_always(expr)                                                                             \
    do {                                                                                                     \
        ((expr)) ? _SPEC_ASSERT_VOID_CAST (0)                                                                \
        : ::spec::__spec_assert_fail(__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION);            \
  } while(false)
#else
#define spec_assert_always(expr)                                                                             \
    do {                                                                                                     \
        static const spec::assert_data assert_data_ctx =                                                     \
                    {__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION};                            \
        ((expr) ? _SPEC_ASSERT_VOID_CAST (0)                                                                 \
        : ::spec::__spec_assert_fail(assert_data_ctx));                                                      \
    } while(false)
#endif

#define assertf(expr, ...)                                                                                   \
    ((expr) ? _SPEC_ASSERT_VOID_CAST (0)                                                                     \
    : ::spec::__spec_assertf_fail (__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION, __VA_ARGS__))
#define spec_assertf(expr, ...)                                                                              \
    ((expr) ? _SPEC_ASSERT_VOID_CAST (0)                                                                     \
    : ::spec::__spec_assertf_fail (__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION, __VA_ARGS__))

#define spec_assertf_always(expr, ...)                                                                       \
    ((expr) ? _SPEC_ASSERT_VOID_CAST (0)                                                                     \
    : ::spec::__spec_assertf_fail (__STRING(expr), __FILE__, __LINE__, __SPEC_ASSERT_FUNCTION, __VA_ARGS__))

#endif //SPEC_ASSERT_H
