/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <iostream>
#include "dout.h"

void dout_emergency(const std::string &str)
{
  std::cerr << str;
  std::cerr.flush();
}

void dout_emergency(const char * const str)
{
  std::cerr << str;
  std::cerr.flush();
}
