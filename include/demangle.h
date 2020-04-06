/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_DEMANGLE_H
#define SPEC_DEMANGLE_H

#if defined(__GNUC__) && defined(__cplusplus)
#include <cstdlib>
#include <memory>
#include <cxxabi.h>

static std::string spec_demangle(const char* name) {
  int status = 0;

  std::unique_ptr<char, void(*)(void*)> res {
    abi::__cxa_demangle(name, NULL, NULL, &status),
    std::free
  };

  return (status == 0) ? res.get() : name ;
}
#else

static std::string spec_demangle(const char* name) {
  return name;
}

#endif // defined(__GNUC__) && defined(__cplusplus)

#endif //SPEC_DEMANGEL_H
