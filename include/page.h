/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_PAGE_H
#define SPEC_PAGE_H

namespace spec {
  extern uint64_t spec_page_size;
  extern uint64_t spec_page_mask;
  extern uint64_t spec_page_shift;
}

#endif //SPEC_PAGE_H

#define SPEC_PAGE_SIZE spec::spec_page_size
#define SPEC_PAGE_MASK spec::spec_page_mask
#define SPEC_PAGE_SHIFT spec::spec_page_shift
