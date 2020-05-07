/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#include <limits.h>
#include <errno.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "buffer/buffer_create.h"
#include "buffer/buffer_audit.h"
#include "buffer/buffer_hash.h"
#include "buffer/buffer_raw.h"
#include "buffer/buffer_ptr.h"
#include "buffer/buffer_list.h"
#include "clock/spec_clock.h"
#include "safe_io.h"

#include "gtest/gtest.h"
#include "crc32/crc32c.h"
#include "crc32/sctp_crc32.h"

#define MAX_TEST 1000000
#define FILENAME "buffer_list"

static char cmd[128];

struct instrumented_bptr : public spec::buffer::ptr {
    const spec::buffer::raw* get_raw() const {
        return m_raw;
    }
};

TEST(Buffer, constructors) {
    uint64_t len = 17;
    { // buffer::create
    buffer_ptr ptr(buffer::create(len));
    EXPECT_EQ(len, ptr.length());
    }

    { // buffer::claim_char
    char* str = new char[len];
    ::memset(str, 'm', len);
    buffer_ptr ptr(buffer::claim_char(len, str));
    EXPECT_EQ(len, ptr.length());
    EXPECT_EQ(str, ptr.c_str());
    buffer_ptr clone = ptr.clone();
    EXPECT_EQ(0, ::memcmp(clone.c_str(), ptr.c_str(), len));
    delete [] str;
    }

    { // buffer::create_static
    char* str = new char[len];
    buffer_ptr ptr(buffer::create_static(len, str));
    EXPECT_EQ(len, ptr.length());
    EXPECT_EQ(str, ptr.c_str());
    delete [] str;
    }

    { // buffer::create_malloc
    buffer_ptr ptr(buffer::create_malloc(len));
    EXPECT_EQ(len, ptr.length());
    }

    { // buffer::claim_malloc
    char* str = (char*)malloc(len);
    ::memset(str, 'm', len);
    buffer_ptr ptr(buffer::claim_malloc(len, str));
    EXPECT_EQ(len, ptr.length());
    EXPECT_EQ(str, ptr.c_str());
    buffer_ptr clone = ptr.clone();
    EXPECT_EQ(0, ::memcmp(clone.c_str(), ptr.c_str(), len));
    }

    { // buffer::copy
    const std::string expected(len, 'x');
    buffer_ptr ptr(buffer::copy(expected.c_str(), expected.size()));
    EXPECT_NE(expected.c_str(), ptr.c_str());
    EXPECT_EQ(0, ::memcmp(expected.c_str(), ptr.c_str(), len));
    }

    { // buffer::create_page_aligned
    buffer_ptr ptr(buffer::create_page_aligned(len));
    ::memset(ptr.c_str(), 'y', len);
    #ifndef DARWIN
    ASSERT_TRUE(ptr.is_page_aligned());
    #endif // DARWIN
    buffer_ptr clone = ptr.clone();
    EXPECT_EQ(0, ::memcmp(clone.c_str(), ptr.c_str(), len));
    }
}

void bench_buffer_alloc(int size, int num) {
    utime_t start = spec_clock_now();
    for (int i = 0; i < num; ++i) {
        buffer_ptr p = buffer::create(size);
        p.zero();
    }
    utime_t end = spec_clock_now();
    std::cout << num << " rounds allocation, "
              << "every round allocate " << size << " bytes, "
              << "totoal time: " << (end - start) << std::endl;
}

TEST(Buffer, BenchAlloc) {
    bench_buffer_alloc(16384, 1000000);
    bench_buffer_alloc(4096, 1000000);
    bench_buffer_alloc(1024, 1000000);
    bench_buffer_alloc(256, 1000000);
    bench_buffer_alloc(32, 1000000);
    bench_buffer_alloc(4, 1000000);
}

TEST(BufferRaw, ostream) {
    buffer_ptr ptr(1);
    std::ostringstream stream;
    stream << *static_cast<instrumented_bptr&>(ptr).get_raw();
    EXPECT_GT(stream.str().size(), stream.str().find("buffer::raw("));
    EXPECT_GT(stream.str().size(), stream.str().find("len 1 nref 1)"));
}

/* +-----------+                +-----+
 * |           |                |     |
 * |  offset   +----------------+     |
 * |           |                |     |
 * |  length   +----            |     |
 * |           |    \-------    |     |
 * +-----------+            \---+     |
 * |   ptr     |                +-----+
 * +-----------+                | raw |
 *                              +-----+
 */
TEST(BufferPtr, constructors) {
    uint64_t len = 17;
    { // ptr::ptr()
    buffer::ptr ptr;
    EXPECT_FALSE(ptr.have_raw());
    EXPECT_EQ(0, ptr.offset());
    EXPECT_EQ(0, ptr.length());
    }

    { // ptr::ptr(raw *r)
    buffer_ptr ptr(buffer::create(len));
    EXPECT_TRUE(ptr.have_raw());
    EXPECT_EQ(0, ptr.offset());
    EXPECT_EQ(len, ptr.length());
    EXPECT_EQ(ptr.raw_length(), ptr.length());
    EXPECT_EQ(1, ptr.raw_nref());
    }

    { // ptr::ptr(uint64_t len)
    buffer_ptr ptr(len);
    EXPECT_TRUE(ptr.have_raw());
    EXPECT_EQ(0, ptr.offset());
    EXPECT_EQ(len, ptr.length());
    EXPECT_EQ(1, ptr.raw_nref());
    }

    { // ptr(const char *d, unsigned l)
    const std::string str(len, 'n');
    buffer_ptr ptr(str.c_str(), len);
    EXPECT_TRUE(ptr.have_raw());
    EXPECT_EQ(0, ptr.offset());
    EXPECT_EQ(len, ptr.length());
    EXPECT_EQ(1, ptr.raw_nref());
    EXPECT_EQ(0, ::memcmp(str.c_str(), ptr.c_str(), len));
    }

    { // ptr(const ptr& p)
    const std::string str(len, 'm');
    buffer_ptr original(str.c_str(), len);
    buffer_ptr ptr(original);
    EXPECT_TRUE(ptr.have_raw());
    EXPECT_EQ(static_cast<instrumented_bptr&>(original).get_raw(),
              static_cast<instrumented_bptr&>(ptr).get_raw());
    EXPECT_EQ(2, ptr.raw_nref());
    EXPECT_EQ(0, ::memcmp(original.c_str(), ptr.c_str(), len));
    }

    { // ptr(const ptr& p, uint64_t off, uint64_t len)
    const std::string str(len, 'x');
    buffer_ptr original(str.c_str(), len);
    buffer_ptr ptr(original, 0, 0);
    EXPECT_TRUE(ptr.have_raw());
    EXPECT_EQ(static_cast<instrumented_bptr&>(original).get_raw(),
              static_cast<instrumented_bptr&>(ptr).get_raw());
    EXPECT_EQ(2, ptr.raw_nref());
    EXPECT_EQ(0, ::memcmp(original.c_str(), ptr.c_str(), len));
    EXPECT_DEATH(buffer_ptr(original, 0, original.length() + 1), "");
    EXPECT_DEATH(buffer_ptr(buffer_ptr(), 0, 0), "");
    }

    { // ptr(ptr&& p)
    const std::string str(len, 'y');
    buffer_ptr original(str.c_str(), len);
    buffer_ptr ptr(std::move(original));
    EXPECT_TRUE(ptr.have_raw());
    EXPECT_FALSE(original.have_raw());
    EXPECT_EQ(0, ::memcmp(str.c_str(), ptr.c_str(), len));
    EXPECT_EQ(1, ptr.raw_nref());
    }
}

TEST(BufferPtr, operator_assign) {
    // ptr& operator= (const ptr& p)
    buffer_ptr ptr(10);
    ptr.copy_in(0, 3, "ABC");
    char dest[1];
    {
    buffer_ptr copy = ptr;
    copy.copy_out(1, 1, dest);
    ASSERT_EQ('B', dest[0]);
    }

    // ptr& operator= (ptr&& p)
    buffer_ptr move = std::move(ptr);
    {
    move.copy_out(1, 1, dest);
    ASSERT_EQ('B', dest[0]);
    }
    EXPECT_FALSE(ptr.have_raw());
}

TEST(BufferPtr, assignment) {
    uint64_t len = 17;
    { // override a buffer_ptr set with the same raw
    buffer_ptr original(len);
    buffer_ptr same_raw(original);
    uint64_t offset = 5;
    uint64_t length = len - offset;
    original.set_offset(offset);
    original.set_length(length);
    same_raw = original;
    ASSERT_EQ(2, original.raw_nref());
    ASSERT_EQ(static_cast<instrumented_bptr&>(same_raw).get_raw(),
              static_cast<instrumented_bptr&>(original).get_raw());
    ASSERT_EQ(same_raw.offset(), original.offset());
    ASSERT_EQ(same_raw.length(), original.length());
    }

    { // self assignment is a noop
    buffer_ptr original(len);
    original = original;
    ASSERT_EQ(1, original.raw_nref());
    ASSERT_EQ((unsigned)0, original.offset());
    ASSERT_EQ(len, original.length());
    }

    { // a copy points to the same raw
    buffer_ptr original(len);
    uint64_t offset = 5;
    uint64_t length = len - offset;
    original.set_offset(offset);
    original.set_length(length);
    buffer_ptr ptr;
    ptr = original;
    ASSERT_EQ(2, original.raw_nref());
    ASSERT_EQ(static_cast<instrumented_bptr&>(ptr).get_raw(),
              static_cast<instrumented_bptr&>(original).get_raw());
    ASSERT_EQ(original.offset(), ptr.offset());
    ASSERT_EQ(original.length(), ptr.length());
    }
}

TEST(BufferPtr, clone) {
    uint64_t len = 17;
    buffer_ptr ptr(len);
    ::memset(ptr.c_str(), 'm', len);
    buffer_ptr clone = ptr.clone();
    EXPECT_EQ(0, ::memcmp(clone.c_str(), ptr.c_str(), len));
}

TEST(BufferPtr, swap) {
    uint64_t len = 17;

    buffer_ptr ptr1(len);
    ::memset(ptr1.c_str(), 'n', len);
    uint64_t ptr1_offset = 4;
    ptr1.set_offset(ptr1_offset);
    uint64_t ptr1_length = 3;
    ptr1.set_length(ptr1_length);

    buffer_ptr ptr2(len);
    ::memset(ptr2.c_str(), 'm', len);
    uint64_t ptr2_offset = 5;
    ptr2.set_offset(ptr2_offset);
    uint64_t ptr2_length = 7;
    ptr2.set_length(ptr2_length);

    ptr1.swap(ptr2);

    EXPECT_EQ(ptr2_length, ptr1.length());
    EXPECT_EQ(ptr2_offset, ptr1.offset());
    EXPECT_EQ('m', ptr1[0]);

    EXPECT_EQ(ptr1_length, ptr2.length());
    EXPECT_EQ(ptr1_offset, ptr2.offset());
    EXPECT_EQ('n', ptr2[0]);
}

TEST(BufferPtr, release) {
    uint64_t len = 17;

    buffer_ptr ptr1(len);
    {
    buffer_ptr ptr2(ptr1);
    EXPECT_EQ(2, ptr1.raw_nref());
    }
    EXPECT_EQ(1, ptr1.raw_nref());
}

TEST(BufferPtr, have_raw) {
    {
    buffer_ptr ptr;
    EXPECT_FALSE(ptr.have_raw());
    }

    {
    buffer_ptr ptr(1);
    EXPECT_TRUE(ptr.have_raw());
    }
}

TEST(BufferPtr, is_n_page_sized) {
    {
    buffer_ptr ptr(SPEC_PAGE_SIZE);
    EXPECT_TRUE(ptr.is_n_page_sized());
    }

    {
    buffer_ptr ptr(1);
    EXPECT_FALSE(ptr.is_n_page_sized());
    }
}

TEST(BufferPtr, is_partial) {
    buffer_ptr a;
    EXPECT_FALSE(a.is_partial());
    buffer_ptr b(10);
    EXPECT_FALSE(b.is_partial());
    buffer_ptr c(b, 1, 9);
    EXPECT_TRUE(c.is_partial());
    buffer_ptr d(b, 0, 9);
    EXPECT_TRUE(d.is_partial());
}

TEST(BufferPtr, accessors) {
    uint64_t len = 17;
    buffer_ptr ptr(len);
    ptr.c_str()[0] = 'n';
    ptr[1] = 'm';
    const buffer_ptr const_ptr(ptr);

    EXPECT_NE((void*)nullptr, (void*)static_cast<instrumented_bptr&>(ptr).get_raw());
    EXPECT_EQ('n', ptr.c_str()[0]);
    {
    buffer_ptr ptr;
    EXPECT_DEATH(ptr.c_str(), "");
    EXPECT_DEATH(ptr[0], "");
    }

    EXPECT_EQ('n', const_ptr.c_str()[0]);
    {
    const buffer_ptr const_ptr;
    EXPECT_DEATH(const_ptr.c_str(), "");
    EXPECT_DEATH(const_ptr[0], "");
    }

    EXPECT_EQ(len, const_ptr.length());
    EXPECT_EQ(0, const_ptr.offset());
    EXPECT_EQ(0, const_ptr.start());
    EXPECT_EQ(len, const_ptr.end());
    EXPECT_EQ(len, const_ptr.end());

    {
    buffer_ptr ptr(len);
    uint64_t unused = 1;
    ptr.set_length(ptr.length() - unused);
    EXPECT_EQ(unused, ptr.unused_tail_length());
    }

    {
    buffer_ptr ptr;
    EXPECT_EQ(0, ptr.unused_tail_length());
    }

    {
    EXPECT_DEATH(ptr[len], "");
    EXPECT_DEATH(const_ptr[len], "");
    }

    {
    const buffer_ptr const_ptr;
    EXPECT_DEATH(const_ptr.raw_c_str(), "");
    EXPECT_DEATH(const_ptr.raw_length(), "");
    EXPECT_DEATH(const_ptr.raw_nref(), "");
    }

    EXPECT_NE((const char *)NULL, const_ptr.raw_c_str());
    EXPECT_EQ(len, const_ptr.raw_length());
    EXPECT_EQ(2, const_ptr.raw_nref());

    {
    buffer_ptr ptr(len);
    unsigned wasted = 1;
    ptr.set_length(ptr.length() - wasted * 2);
    ptr.set_offset(wasted);
    EXPECT_EQ(wasted * 2, ptr.wasted());
    }
}

TEST(BufferPtr, cmp) {
    buffer_ptr empty;
    buffer_ptr a("A", 1);
    buffer_ptr ab("AB", 2);
    buffer_ptr af("AF", 2);
    buffer_ptr acc("ACC", 3);
    EXPECT_GE(-1, empty.cmp(a));
    EXPECT_LE(1, a.cmp(empty));
    EXPECT_GE(-1, a.cmp(ab));
    EXPECT_LE(1, ab.cmp(a));
    EXPECT_EQ(0, ab.cmp(ab));
    EXPECT_GE(-1, ab.cmp(af));
    EXPECT_LE(1, af.cmp(ab));
    EXPECT_GE(-1, acc.cmp(af));
    EXPECT_LE(1, af.cmp(acc));
}

TEST(BufferPtr, is_zero) {
    char str[2] = { '\0', 'X' };
    {
    const buffer_ptr ptr(buffer::create_static(2, str));
    EXPECT_FALSE(ptr.is_zero());
    }

    {
    const buffer_ptr ptr(buffer::create_static(1, str));
    EXPECT_TRUE(ptr.is_zero());
    }
}

TEST(BufferPtr, copy_out) {
    {
    const buffer_ptr ptr;
    EXPECT_DEATH(ptr.copy_out(0, 0, NULL), "");
    }

    {
    char in[] = "ABC";
    const buffer_ptr ptr(buffer::create_static(strlen(in), in));
    EXPECT_THROW(ptr.copy_out(0, strlen(in) + 1, NULL), buffer::end_of_buffer);
    EXPECT_THROW(ptr.copy_out(strlen(in) + 1, 0, NULL), buffer::end_of_buffer);
    char out[1] = {'X'};
    ptr.copy_out(1, 1, out);
    EXPECT_EQ('B', out[0]);
    }
}

TEST(BufferPtr, copy_out_bench) {
    for (int s = 1; s <= 8; s *= 2) {
        utime_t start = spec_clock_now();
        uint64_t buflen = 1048576;
        int count = 1000;
        uint64_t v;
        for (int i = 0; i < count; ++i) {
            buffer_ptr bp(buflen);
            for (uint64_t j = 0; j < buflen; j += s) {
	            bp.copy_out(j, s, (char *)&v);
            }
        }
        utime_t end = spec_clock_now();
        std::cout << count << " round copy in, every round copy " << buflen << " bytes, "
	              << "copy " << s << " byte every time, "
                  << "copy " << buflen / s << " times, "
	              << "cost time: " << (end - start) << std::endl;
    }
}

TEST(BufferPtr, copy_in) {
    {
    buffer_ptr ptr;
    EXPECT_DEATH(ptr.copy_in(0, 0, NULL), "");
    }

    {
    char in[] = "ABCD";
    buffer_ptr ptr(2);
    {
    EXPECT_DEATH(ptr.copy_in(0, strlen(in) + 1, NULL), "");
    EXPECT_DEATH(ptr.copy_in(strlen(in) + 1, 0, NULL), "");
    }
    ptr.copy_in(0, 2, in);
    EXPECT_EQ(in[0], ptr[0]);
    EXPECT_EQ(in[1], ptr[1]);
    }
}

TEST(BufferPtr, copy_in_bench) {
    for (int s = 1; s <= 8; s *= 2) {
        utime_t start = spec_clock_now();
        int buflen = 1048576;
        int count = 1000;
        for (int i = 0; i < count; ++i) {
            buffer_ptr bp(buflen);
            for (uint64_t j = 0; j < buflen; j += s) {
	            bp.copy_in(j, s, (char *)&j, false);
            }
        }
        utime_t end = spec_clock_now();
        std::cout << count << " round copy out, every round copy " << buflen << " bytes, "
	              << "copy " << s << " byte every time, "
                  << "copy " << buflen / s << " times, "
	              << "cost time: " << (end - start) << std::endl;
    }
}

TEST(BufferPtr, append) {
    {
    buffer_ptr ptr;
    EXPECT_DEATH(ptr.append('A'), "");
    EXPECT_DEATH(ptr.append("B", 1), "");
    }

    {
    buffer_ptr ptr(2);
    {
    EXPECT_DEATH(ptr.append('A'), "");
    EXPECT_DEATH(ptr.append("B", 1), "");
    }
    ptr.set_length(0);
    ptr.append('A');
    EXPECT_EQ(1, ptr.length());
    EXPECT_EQ('A', ptr[0]);
    ptr.append("B", 1);
    EXPECT_EQ(2, ptr.length());
    EXPECT_EQ('B', ptr[1]);
    }
}

TEST(BufferPtr, append_bench) {
    char src[1048576];
    memset(src, 0, sizeof(src));
    for (int s = 4; s <= 16384; s *= 4) {
        utime_t start = spec_clock_now();
        int buflen = 1048576;
        int count = 4000;
        for (int i = 0; i < count; ++i) {
            buffer_ptr bp(buflen);
            bp.set_length(0);
            for (uint64_t j = 0; j < buflen; j += s) {
	            bp.append(src + j, s);
            }
        }
        utime_t end = spec_clock_now();
        std::cout << count << " round append, every round append " << buflen << " bytes, "
	              << "append " << s << " byte every time, "
                  << "append " << buflen / s << " times, "
	              << "cost time: " << (end - start) << std::endl;
    }
}

TEST(BufferPtr, zero) {
    char str[] = "nnnn";
    buffer_ptr ptr(buffer::create_static(strlen(str), str));
    {
    EXPECT_DEATH(ptr.zero(ptr.length() + 1, 0), "");
    }
    ptr.zero(1, 1);
    EXPECT_EQ('n', ptr[0]);
    EXPECT_EQ('\0', ptr[1]);
    EXPECT_EQ('n', ptr[2]);
    ptr.zero();
    EXPECT_EQ('\0', ptr[0]);
}

TEST(BufferPtr, ostream) {
    {
    buffer_ptr ptr;
    std::ostringstream stream;
    stream << ptr;
    EXPECT_GT(stream.str().size(),
              stream.str().find("buffer::ptr(0~0 no raw)"));
    }

    {
    char str[] = "nnnn";
    buffer_ptr ptr(buffer::create_static(strlen(str), str));
    std::ostringstream stream;
    stream << ptr;
    EXPECT_GT(stream.str().size(), stream.str().find("len 4 nref 1)"));
    }
}

/*                                             | +-----+ |
 *    list              ptr                    | |     | |
 * +----------+       +-----+                  | |     | |
 * | append_  >------->     >-------------------->     | |
 * |  buffer  |       +-----+                  | |     | |
 * +----------+                        ptr     | |     | |
 * |   _len   |      list            +-----+   | |     | |
 * +----------+    +------+     ,--->+     >----->     | |
 * | _buffers >---->      >-----     +-----+   | +-----+ |
 * +----------+    +----^-+     \      ptr     |   raw   |
 * |  last_p  |        /         `-->+-----+   | +-----+ |
 * +--------+-+       /              +     >----->     | |
 *          |       ,-          ,--->+-----+   | |     | |
 *          |      /        ,---               | |     | |
 *          |     /     ,---                   | |     | |
 *        +-v--+-^--+--^+-------+              | |     | |
 *        | bl | ls | p | p_off >--------------->|     | |
 *        +----+----+-----+-----+              | +-----+ |
 *        |               | off >------------->|   raw   |
 *        +---------------+-----+              |         |
 *              iterator                       +---------+
 */
TEST(BufferListIterator, constructors) {
    { // iterator()
    buffer_list::iterator it;
    EXPECT_EQ((unsigned)0, it.get_off());
    }

    { // iterator(list *l, uint64_t o=0)
    buffer_list bl;
    bl.append("ABC", 3);
    {
    buffer_list::iterator it(&bl);
    EXPECT_EQ((unsigned)0, it.get_off());
    EXPECT_EQ('A', *it);
    }
    {
    buffer_list::iterator it(&bl, 1);
    EXPECT_EQ('B', *it);
    EXPECT_EQ((unsigned)2, it.get_remaining());
    }
    }

    // iterator(list *l, unsigned o, std::list<ptr>::iterator ip, unsigned po)
    // not tested because of http://tracker.spec.com/issues/4101

    { // iterator(const iterator& other)
    buffer_list bl;
    bl.append("ABC", 3);
    buffer_list::iterator i(&bl, 1);
    buffer_list::iterator j(i);
    EXPECT_EQ(*i, *j);
    ++j;
    EXPECT_NE(*i, *j);
    EXPECT_EQ('B', *i);
    EXPECT_EQ('C', *j);
    }

    { // const_iterator(const iterator& other)
    buffer_list bl;
    bl.append("ABC", 3);
    buffer_list::iterator i(&bl);
    buffer_list::const_iterator ci(i);
    EXPECT_EQ(0u, ci.get_off());
    EXPECT_EQ('A', *ci);
    }
}

TEST(BufferListIterator, empty_create_append_copy) {
    buffer_list bl, bl2, bl3, out;
    bl2.append("bar");
    bl.swap(bl2);
    bl2.append("xxx");
    bl.append(bl2);
    bl.rebuild();
    bl.begin().copy(6, out);
    ASSERT_TRUE(out.contents_equal(bl));
}

TEST(BufferListIterator, operator_assign) {
    buffer_list bl;
    bl.append("ABC", 3);
    buffer_list::iterator i(&bl, 1);

    i = i;
    EXPECT_EQ('B', *i);
    buffer_list::iterator j;
    j = i;
    EXPECT_EQ('B', *j);
}

TEST(BufferListIterator, get_off) {
    buffer_list bl;
    bl.append("ABC", 3);
    buffer_list::iterator it(&bl, 1);
    EXPECT_EQ((unsigned)1, it.get_off());
}

TEST(BufferListIterator, get_remaining) {
    buffer_list bl;
    bl.append("ABC", 3);
    buffer_list::iterator i(&bl, 1);
    EXPECT_EQ((unsigned)2, i.get_remaining());
}

TEST(BufferListIterator, end) {
    buffer_list bl;
    {
    buffer_list::iterator i(&bl);
    EXPECT_TRUE(i.end());
    }
    bl.append("ABC", 3);
    {
    buffer_list::iterator i(&bl);
    EXPECT_FALSE(i.end());
    }
}

static void bench_buffer_listiter_deref(const size_t step,
				       const size_t bufptr_size,
				       const size_t bufptr_num) {
    const std::string buf(bufptr_size, 'a');
    spec::buffer_list bl;

    for (size_t i = 0; i < bufptr_num; i++) {
        bl.append(spec::buffer_ptr(buf.c_str(), buf.size()));
    }

    uint64_t it_count = 0;
    utime_t start = spec_clock_now();
    buffer_list::iterator iter = bl.begin();
    while (iter != bl.end()) {
        iter += step;
        it_count++;
    }
    utime_t end = spec_clock_now();
    std::cout << "bl size is:" << bl.length() << " has " << bufptr_num << " buffers "
              << "each buffer size: " << buf.size() << " "
              << "iterate step: " << step << " iterate count: " << it_count << " "
              << "spend time: " << (end -start) << std::endl;
}

TEST(BufferListIterator, BenchDeref) {
    bench_buffer_listiter_deref(1, 1, 4096000);
    bench_buffer_listiter_deref(1, 10, 409600);
    bench_buffer_listiter_deref(1, 100, 40960);
    bench_buffer_listiter_deref(1, 1000, 4096);

    bench_buffer_listiter_deref(4, 1, 1024000);
    bench_buffer_listiter_deref(4, 10, 102400);
    bench_buffer_listiter_deref(4, 100, 10240);
    bench_buffer_listiter_deref(4, 1000, 1024);
}

TEST(BufferListIterator, advance) {
    buffer_list bl;
    const std::string one("ABC");
    bl.append(buffer_ptr(one.c_str(), one.size()));
    const std::string two("DEF");
    bl.append(buffer_ptr(two.c_str(), two.size()));

    {
    buffer_list::iterator it(&bl);
    EXPECT_THROW(it += 200u, buffer::end_of_buffer);
    }

    {
    buffer_list::iterator it(&bl);
    EXPECT_EQ('A', *it);
    it += 1u;
    EXPECT_EQ('B', *it);
    it += 3u;
    EXPECT_EQ('E', *it);
    }
}

TEST(BufferListIterator, iterate_with_empties) {
    buffer_list bl;
    EXPECT_EQ(bl.get_num_buffers(), 0u);

    bl.push_back(buffer::create(0));
    EXPECT_EQ(bl.length(), 0u);
    EXPECT_EQ(bl.get_num_buffers(), 1u);

    bl.push_back(buffer::create(0));
    EXPECT_EQ(bl.get_num_buffers(), 2u);

    // append buffer_list with single, 0-sized ptr inside
    buffer_list bl_with_empty_ptr;
    bl.append(bl_with_empty_ptr);
}

TEST(BufferListIterator, get_ptr_and_advance)
{
    buffer_ptr a("one", 3);
    buffer_ptr b("two", 3);
    buffer_ptr c("three", 5);
    buffer_list bl;
    bl.append(a);
    bl.append(b);
    bl.append(c);

    const char *ptr = nullptr;
    buffer_list::iterator it = bl.begin();
    ASSERT_EQ(3u, it.get_ptr_and_advance(11u, &ptr));
    ASSERT_EQ(bl.length() - 3u, it.get_remaining());
    ASSERT_EQ(0, memcmp(ptr, "one", 3));

    ASSERT_EQ(2u, it.get_ptr_and_advance(2u, &ptr));
    ASSERT_EQ(0, memcmp(ptr, "tw", 2));

    ASSERT_EQ(1u, it.get_ptr_and_advance(4u, &ptr));
    ASSERT_EQ(0, memcmp(ptr, "o", 1));

    ASSERT_EQ(5u, it.get_ptr_and_advance(5u, &ptr));
    ASSERT_EQ(0, memcmp(ptr, "three", 5));
    ASSERT_EQ(0u, it.get_remaining());
}

TEST(BufferListIterator, iterator_crc32c) {
    buffer_list bl1;

    std::string s1(100, 'a');
    std::string s2(50, 'b');
    std::string s3(7, 'c');
    bl1.append(s1);
    bl1.append(s2);
    bl1.append(s3);

    std::string s = s1 + s2 + s3;
    buffer_list bl2;
    bl2.append(s);

    buffer_list::iterator it = bl2.begin();
    ASSERT_EQ(bl1.crc32c(0), it.crc32c(it.get_remaining(), 0));
    ASSERT_EQ(0u, it.get_remaining());

    it = bl1.begin();
    ASSERT_EQ(bl2.crc32c(0), it.crc32c(it.get_remaining(), 0));

    buffer_list bl3;
    bl3.append(s.substr(98, 55));
    it = bl1.begin();
    it += 98u;
    ASSERT_EQ(bl3.crc32c(0), it.crc32c(55, 0));
    ASSERT_EQ(4u, it.get_remaining());

    bl3.clear();
    bl3.append(s.substr(98 + 55));
    it = bl1.begin();
    it += 98u + 55u;
    ASSERT_EQ(bl3.crc32c(0), it.crc32c(bl3.length(), 0));
    ASSERT_EQ(0u, it.get_remaining());
}

TEST(BufferListIterator, seek) {
    buffer_list bl;
    bl.append("ABC", 3);
    buffer_list::iterator i(&bl, 1);
    EXPECT_EQ('B', *i);
    i.seek(2);
    EXPECT_EQ('C', *i);
}

TEST(BufferListIterator, operator_star) {
    buffer_list bl;
    {
    buffer_list::iterator i(&bl);
    EXPECT_THROW(*i, buffer::end_of_buffer);
    }

    bl.append("ABC", 3);
    {
    buffer_list::iterator i(&bl);
    EXPECT_EQ('A', *i);
    EXPECT_THROW(i += 200u, buffer::end_of_buffer);
    EXPECT_THROW(*i, buffer::end_of_buffer);
    }
}

TEST(BufferListIterator, operator_equal) {
    buffer_list bl;
    bl.append("ABC", 3);
    {
    buffer_list::iterator i(&bl);
    buffer_list::iterator j(&bl);
    EXPECT_EQ(i, j);
    }

    {
    buffer_list::const_iterator ci = bl.begin();
    buffer_list::iterator i = bl.begin();
    EXPECT_EQ(i, ci);
    EXPECT_EQ(ci, i);
    }
}

TEST(BufferListIterator, operator_nequal) {
    buffer_list bl;
    bl.append("ABC", 3);
    {
    buffer_list::iterator i(&bl);
    buffer_list::iterator j(&bl);
    EXPECT_NE(++i, j);
    }

    {
    buffer_list::const_iterator ci = bl.begin();
    buffer_list::const_iterator cj = bl.begin();
    ++ci;
    EXPECT_NE(ci, cj);

    buffer_list::iterator i = bl.begin();
    EXPECT_NE(i, ci);
    EXPECT_NE(ci, i);
    }

    { // tests begin(), end(), operator++() also
    std::string s("ABC");
    int i = 0;
    for (auto c : bl) {
      EXPECT_EQ(s[i++], c);
    }
    }
}

TEST(BufferListIterator, operator_plus_plus) {
    buffer_list bl;
    {
    buffer_list::iterator it(&bl);
    EXPECT_THROW(++it, buffer::end_of_buffer);
    }

    bl.append("ABC", 3);
    {
    buffer_list::iterator it(&bl);
    ++it;
    EXPECT_EQ('B', *it);
    }
}

TEST(BufferListIterator, get_current_ptr) {
    buffer_list bl;
    {
    buffer_list::iterator it(&bl);
    EXPECT_THROW(++it, buffer::end_of_buffer);
    }

    bl.append("ABC", 3);
    {
    buffer_list::iterator it(&bl, 1);
    const buffer::ptr ptr = it.get_current_ptr();
    EXPECT_EQ('B', ptr[0]);
    EXPECT_EQ(1, ptr.offset());
    EXPECT_EQ(2, ptr.length());
    }
}

TEST(BufferListIterator, copy) {
    buffer_list bl;
    const char *expected = "ABC";
    bl.append(expected, 3);

    { // void copy(uint64_t len, char *dest);
    char* temp = (char*)malloc(3);
    ::memset(temp, 'n', 3);

    buffer_list::iterator it(&bl);
    EXPECT_THROW(it += 200u, buffer::end_of_buffer);

    // demonstrates that it seeks back to offset if m_list_it == m_list->end()
    it.copy(2, temp);
    EXPECT_EQ(0, ::memcmp(temp, expected, 2));
    EXPECT_EQ('n', temp[2]);
    it.seek(0);
    it.copy(3, temp);
    EXPECT_EQ(0, ::memcmp(temp, expected, 3));
    free(temp);
    }

    { // void copy(uint64_t len, char *dest) via begin(uint64_t offset)
    buffer_list bl;
    EXPECT_THROW(bl.begin(100).copy(100, nullptr), buffer::end_of_buffer);
    const char *expected = "ABC";
    bl.append(expected);
    char dest[2] = {'\0'};
    bl.begin(1).copy(2, dest);
    EXPECT_EQ(0, ::memcmp(expected + 1, dest, 2));
    }

    { // void buffer_list::iterator::copy_deep(uint64_t len, ptr &dest)
    buffer_ptr ptr;
    buffer_list::iterator i(&bl);
    i.copy_deep(2, ptr);
    EXPECT_EQ(2, ptr.length());
    EXPECT_EQ('A', ptr[0]);
    EXPECT_EQ('B', ptr[1]);
    }

    { // void buffer_list::iterator::copy_shallow(uint64_t len, ptr &dest)
    buffer_ptr ptr;
    buffer_list::iterator i(&bl);
    i.copy_shallow(2, ptr);
    EXPECT_EQ(2, ptr.length());
    EXPECT_EQ('A', ptr[0]);
    EXPECT_EQ('B', ptr[1]);
    }

    { // void buffer_list::iterator::copy(unsigned len, list &dest)
    buffer_list temp;
    buffer_list::iterator it(&bl);

    // demonstrates that it seeks back to offset if m_list_it == m_list->end()
    EXPECT_THROW(it += 200u, buffer::end_of_buffer);
    it.copy(2, temp);
    EXPECT_EQ(0, ::memcmp(temp.c_str(), expected, 2));
    it.seek(0);
    it.copy(3, temp);
    EXPECT_EQ('A', temp[0]);
    EXPECT_EQ('B', temp[1]);
    EXPECT_EQ('A', temp[2]);
    EXPECT_EQ('B', temp[3]);
    EXPECT_EQ('C', temp[4]);
    EXPECT_EQ((2 + 3), temp.length());
    }

    { // void buffer_list::iterator::copy(uint64_t len, list &dest) via begin(uint64_t offset)
    buffer_list bl;
    buffer_list dest;
    EXPECT_THROW(bl.begin((unsigned)100).copy((unsigned)100, dest), buffer::end_of_buffer);
    const char *expected = "ABC";
    bl.append(expected);
    bl.begin(1).copy(2, dest);
    EXPECT_EQ(0, ::memcmp(expected + 1, dest.c_str(), 2));
    }

    { // void buffer_list::iterator::copy_all(list &dest)
    buffer_list copy;
    buffer_list::iterator it(&bl);

    // demonstrates that it seeks back to offset if m_list_it == m_list->end()
    EXPECT_THROW(it += 200u, buffer::end_of_buffer);
    it.copy_all(copy);
    EXPECT_EQ('A', copy[0]);
    EXPECT_EQ('B', copy[1]);
    EXPECT_EQ('C', copy[2]);
    EXPECT_EQ(3, copy.length());
    }

    { // void copy(uint64_t len, std::string &dest)
    std::string temp;
    buffer_list::iterator it(&bl);

    // demonstrates that it seeks back to offset if m_list_it == m_list->end()
    EXPECT_THROW(it += 200u, buffer::end_of_buffer);
    it.copy(2, temp);
    EXPECT_EQ(0, ::memcmp(temp.c_str(), expected, 2));
    it.seek(0);
    it.copy(3, temp);
    EXPECT_EQ('A', temp[0]);
    EXPECT_EQ('B', temp[1]);
    EXPECT_EQ('A', temp[2]);
    EXPECT_EQ('B', temp[3]);
    EXPECT_EQ('C', temp[4]);
    EXPECT_EQ((2 + 3), temp.length());
    }

    { // void copy(uint64_t len, std::string &dest) via begin(uint64_t offset)
    buffer_list bl;
    std::string dest;
    EXPECT_THROW(bl.begin((unsigned)100).copy((unsigned)100, dest), buffer::end_of_buffer);
    const char *expected = "ABC";
    bl.append(expected);
    bl.begin(1).copy(2, dest);
    EXPECT_EQ(0, ::memcmp(expected + 1, dest.c_str(), 2));
    }
}

TEST(BufferListIterator, copy_in) {
    { // void buffer_list::iterator::copy_in(uint64_t len, const char *src)
    buffer_list bl;
    bl.append("MMM", 3);
    buffer_list::iterator i(&bl);

    // demonstrates that it seeks back to offset if m_list_it == m_list->end()
    EXPECT_THROW(i += 200u, buffer::end_of_buffer);
    const char *expected = "ABC";
    i.copy_in(3, expected);
    EXPECT_EQ(0, ::memcmp(bl.c_str(), expected, 3));
    EXPECT_EQ('A', bl[0]);
    EXPECT_EQ('B', bl[1]);
    EXPECT_EQ('C', bl[2]);
    EXPECT_EQ((unsigned)3, bl.length());
    }

    { // void copy_in(uint64_t len, const char *src) via begin(uint64_t offset)
    buffer_list bl;
    bl.append("MMM");
    EXPECT_THROW(bl.begin((unsigned)100).copy_in((unsigned)100, (char*)0), buffer::end_of_buffer);
    bl.begin(1).copy_in(2, "AB");
    EXPECT_EQ(0, ::memcmp("MAB", bl.c_str(), 3));
    }

    { // void buffer_list::iterator::copy_in(uint64_t len, const list& other)
    buffer_list bl;
    bl.append("MMM", 3);
    buffer_list::iterator it(&bl);
    buffer_list expected;
    expected.append("ABC", 3);

    // demonstrates that it seeks back to offset if m_list_it == m_list->end()
    EXPECT_THROW(it += 200u, buffer::end_of_buffer);
    it.copy_in(3, expected);
    EXPECT_EQ(0, ::memcmp(bl.c_str(), expected.c_str(), 3));
    EXPECT_EQ('A', bl[0]);
    EXPECT_EQ('B', bl[1]);
    EXPECT_EQ('C', bl[2]);
    EXPECT_EQ(3, bl.length());
    }

    { // void copy_in(unsigned len, const list& src) via begin(uint64_t offset)
    buffer_list bl;
    bl.append("MMM");

    buffer_list src;
    src.append("ABC");

    EXPECT_THROW(bl.begin((unsigned)100).copy_in((unsigned)100, src), buffer::end_of_buffer);
    bl.begin(1).copy_in(2, src);
    EXPECT_EQ(0, ::memcmp("MAB", bl.c_str(), 3));
    }
}

// iterator& buffer_list::const_iterator::operator++()
TEST(BufferListConstIterator, operator_plus_plus) {
    buffer_list bl;
    {
    buffer_list::const_iterator it(&bl);
    EXPECT_THROW(++it, buffer::end_of_buffer);
    }

    bl.append("ABC", 3);
    {
    const buffer_list const_bl(bl);
    buffer_list::const_iterator it(const_bl.begin());
    ++it;
    EXPECT_EQ('B', *it);
    }
}

TEST(BufferList, constructors) {
    { // list()
    buffer_list bl;
    ASSERT_EQ(0, bl.length());
    }

    { // list(uint64_t prealloc)
    buffer_list bl(1);
    ASSERT_EQ(0, bl.length());
    bl.append('A');
    ASSERT_EQ('A', bl[0]);
    }

    { // list(const list& other)
    buffer_list bl(1);
    bl.append('A');
    ASSERT_EQ('A', bl[0]);
    buffer_list copy(bl);
    ASSERT_EQ('A', copy[0]);
    }

    {
    // list(list&& other)
    buffer_list bl(1);
    bl.append('A');
    buffer_list copy = std::move(bl);
    ASSERT_EQ(0, bl.length());
    ASSERT_EQ(1, copy.length());
    ASSERT_EQ('A', copy[0]);
    }
}

TEST(BufferList, append_after_move) {
    buffer_list bl(6);
    bl.append("ABC", 3);
    EXPECT_EQ(1, bl.get_num_buffers());

    buffer_list moved_to_bl(std::move(bl));
    moved_to_bl.append("123", 3);
    // it's expected that the list(list&&) ctor will preserve the _carriage
    EXPECT_EQ(1, moved_to_bl.get_num_buffers());
    EXPECT_EQ(0, ::memcmp("ABC123", moved_to_bl.c_str(), 6));
}

void bench_buffer_list_alloc(int size, int num, int per)
{
    utime_t start = spec_clock_now();
    for (int i = 0; i < num; ++i) {
        buffer_list bl;
        for (int j = 0; j < per; ++j)
            bl.push_back(buffer::ptr_node::create(buffer::create(size)));
    }
    utime_t end = spec_clock_now();
    std::cout << num << " rounds allocation buffer_list, "
              << "every buffer_list include " << per << " butter ptrs, "
              << "every ptr allocates " << size << " bytes, "
              << "total time: " << (end - start) << std::endl;
}

TEST(BufferList, BenchAlloc) {
  bench_buffer_list_alloc(32768, 100000, 16);
  bench_buffer_list_alloc(25000, 100000, 16);
  bench_buffer_list_alloc(16384, 100000, 16);
  bench_buffer_list_alloc(10000, 100000, 16);
  bench_buffer_list_alloc(8192, 100000, 16);
  bench_buffer_list_alloc(6000, 100000, 16);
  bench_buffer_list_alloc(4096, 100000, 16);
  bench_buffer_list_alloc(1024, 100000, 16);
  bench_buffer_list_alloc(256, 100000, 16);
  bench_buffer_list_alloc(32, 100000, 16);
  bench_buffer_list_alloc(4, 100000, 16);
}

TEST(BufferList, append_bench_with_size_hint) {
    std::array<char, 1048576> src = { 0, };

    for (size_t step = 4; step <= 16384; step *= 4) {
        constexpr size_t rounds = 4000;

        const utime_t start = spec_clock_now();
        for (size_t r = 0; r < rounds; ++r) {
            buffer_list bl(std::size(src));
            for (auto iter = std::begin(src);
                 iter != std::end(src);
                 iter = std::next(iter, step)) {
	            bl.append(&*iter, step);
            }
        }
        std::cout << "Per round: append totoal size " << std::size(src) << ", "
                  << "buffer_list pre allocate space per round; "
                  << "Per round: append " << std::size(src) / step << " "
                  << "times to append " << step << " bytes into buffer_list per time; "
                  << "totoal time: " << (spec_clock_now() - start) << std::endl;
    }
}

TEST(BufferList, append_bench) {
    std::array<char, 1048576> src = { 0, };

    for (size_t step = 4; step <= 16384; step *= 4) {
        constexpr size_t rounds = 4000;

        const utime_t start = spec_clock_now();
        for (size_t r = 0; r < rounds; ++r) {
            buffer_list bl;
            for (auto iter = std::begin(src);
	             iter != std::end(src);
	             iter = std::next(iter, step)) {
	            bl.append(&*iter, step);
            }
        }
        std::cout << "Per round: append totoal size " << std::size(src) << ", "
                  << "no buffer_list pre allocate space per round; "
                  << "Per round: append " << std::size(src) / step << " "
                  << "times to append " << step << " bytes into buffer_list per time; "
                  << "totoal time: " << (spec_clock_now() - start) << std::endl;
    }
}

TEST(BufferList, operator_equal) {
    buffer_list bl;
    bl.append("ABC", 3);
    {
    std::string dest;
    bl.begin(1).copy(1, dest);
    ASSERT_EQ('B', dest[0]);
    }

    { // list& operator= (const list& other)
    buffer_list copy = bl;
    std::string dest;
    copy.begin(1).copy(1, dest);
    ASSERT_EQ('B', dest[0]);
    }

    { // list& operator= (list&& other)
    buffer_list move;
    move = std::move(bl);
    std::string dest;
    move.begin(1).copy(1, dest);
    ASSERT_EQ('B', dest[0]);
    EXPECT_TRUE(move.length());
    EXPECT_TRUE(!bl.length());
    }
}

TEST(BufferList, buffers) {
    buffer_list bl;
    ASSERT_EQ((unsigned)0, bl.get_num_buffers());
    bl.append('A');
    ASSERT_EQ((unsigned)1, bl.get_num_buffers());
}

TEST(BufferList, to_str) {
    {
    buffer_list bl;
    bl.append("foo");
    ASSERT_EQ(bl.to_str(), std::string("foo"));
    }
    {
    buffer_ptr a("foobarbaz", 9);
    buffer_ptr b("123456789", 9);
    buffer_ptr c("ABCDEFGHI", 9);
    buffer_list bl;
    bl.append(a);
    bl.append(b);
    bl.append(c);
    ASSERT_EQ(bl.to_str(), std::string("foobarbaz123456789ABCDEFGHI"));
    }
}

TEST(BufferList, swap) {
    buffer_list b1;
    b1.append('A');

    buffer_list b2;
    b2.append('B');

    b1.swap(b2);

    std::string s1;
    b1.begin().copy(1, s1);
    ASSERT_EQ('B', s1[0]);

    std::string s2;
    b2.begin().copy(1, s2);
    ASSERT_EQ('A', s2[0]);
}

TEST(BufferList, length) {
    buffer_list bl;
    ASSERT_EQ(0, bl.length());
    bl.append('A');
    ASSERT_EQ(1, bl.length());
}

TEST(BufferList, contents_equal) {
    // A BB
    // AB B
    buffer_list bl1;
    bl1.append("A");
    bl1.append("BB");

    buffer_list bl2;
    ASSERT_FALSE(bl1.contents_equal(bl2)); // different length
    bl2.append("AB");
    bl2.append("B");
    ASSERT_TRUE(bl1.contents_equal(bl2)); // same length same content

    // ABC
    buffer_list bl3;
    bl3.append("ABC");
    ASSERT_FALSE(bl1.contents_equal(bl3)); // same length different content
}

TEST(BufferList, is_aligned) {
    const uint64_t SIMD_ALIGN = 32;
    {
    buffer_list bl;
    EXPECT_TRUE(bl.is_aligned(SIMD_ALIGN));
    }

    {
    buffer_ptr ptr(buffer::create_aligned(2, SIMD_ALIGN));
    ptr.set_offset(1);
    ptr.set_length(1);

    buffer_list bl;
    bl.append(ptr);
    EXPECT_FALSE(bl.is_aligned(SIMD_ALIGN));
    bl.rebuild_aligned(SIMD_ALIGN);
    EXPECT_TRUE(bl.is_aligned(SIMD_ALIGN));
    }

    {
    buffer_list bl;
    buffer_ptr ptr(buffer::create_aligned(SIMD_ALIGN + 1, SIMD_ALIGN));
    ptr.set_offset(1);
    ptr.set_length(SIMD_ALIGN);
    bl.append(ptr);
    EXPECT_FALSE(bl.is_aligned(SIMD_ALIGN));
    bl.rebuild_aligned(SIMD_ALIGN);
    EXPECT_TRUE(bl.is_aligned(SIMD_ALIGN));
    }
}

TEST(BufferList, is_n_align_sized) {
    const uint64_t SIMD_ALIGN = 32;
    {
    buffer_list bl;
    EXPECT_TRUE(bl.is_n_align_sized(SIMD_ALIGN));
    }

    {
    buffer_list bl;
    bl.append_zero(1);
    EXPECT_FALSE(bl.is_n_align_sized(SIMD_ALIGN));
    }

    {
    buffer_list bl;
    bl.append_zero(SIMD_ALIGN);
    EXPECT_TRUE(bl.is_n_align_sized(SIMD_ALIGN));
    }
}

TEST(BufferList, is_page_aligned) {
    {
    buffer_list bl;
    EXPECT_TRUE(bl.is_page_aligned());
    }

    {
    buffer_list bl;
    buffer_ptr ptr(buffer::create_page_aligned(2));
    ptr.set_offset(1);
    ptr.set_length(1);
    bl.append(ptr);
    EXPECT_FALSE(bl.is_page_aligned());
    bl.rebuild_page_aligned();
    EXPECT_TRUE(bl.is_page_aligned());
    }

    {
    buffer_list bl;
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE + 1));
    ptr.set_offset(1);
    ptr.set_length(SPEC_PAGE_SIZE);
    bl.append(ptr);
    EXPECT_FALSE(bl.is_page_aligned());
    bl.rebuild_page_aligned();
    EXPECT_TRUE(bl.is_page_aligned());
    }
}

TEST(BufferList, is_n_page_sized) {
    {
    buffer_list bl;
    EXPECT_TRUE(bl.is_n_page_sized());
    }

    {
    buffer_list bl;
    bl.append_zero(1);
    EXPECT_FALSE(bl.is_n_page_sized());
    }

    {
    buffer_list bl;
    bl.append_zero(SPEC_PAGE_SIZE);
    EXPECT_TRUE(bl.is_n_page_sized());
    }
}

//performance boost
TEST(BufferList, page_aligned_appender) {
    buffer_list bl;
    auto a = bl.get_page_aligned_appender(5);
    a.append("asdf", 4);
    a.flush();
    std::cout << bl << std::endl;
    ASSERT_EQ(1u, bl.get_num_buffers());

    a.append("asdf", 4);
    for (int n = 0; n < 3 * SPEC_PAGE_SIZE; ++n) {
      a.append("x", 1);
    }
    a.flush();
    std::cout << bl << std::endl;
    ASSERT_EQ(1u, bl.get_num_buffers());

    for (int n = 0; n < 3 * SPEC_PAGE_SIZE; ++n) {
      a.append("y", 1);
    }
    a.flush();
    std::cout << bl << std::endl;

    ASSERT_EQ(2u, bl.get_num_buffers());
    for (int n = 0; n < 10 * SPEC_PAGE_SIZE; ++n) {
      a.append("asdfasdfasdf", 1);
    }
    a.flush();
    std::cout << bl << std::endl;
}

TEST(BufferList, rebuild_aligned_size_and_memory) {
    const uint64_t SIMD_ALIGN = 32;
    const uint64_t BUFFER_SIZE = 67;
    buffer_list bl;

    // These two must be concatenated into one memory + size aligned
    // buffer_ptr
    {
    buffer_ptr ptr(buffer::create_aligned(2, SIMD_ALIGN));
    ptr.set_offset(1);
    ptr.set_length(1); // 1, 1, 32
    bl.append(ptr);
    }
    {
    buffer_ptr ptr(buffer::create_aligned(BUFFER_SIZE - 1, SIMD_ALIGN));
    bl.append(ptr); // 0, 66, 96
    }

    // This one must be left alone
    {
    buffer_ptr ptr(buffer::create_aligned(BUFFER_SIZE, SIMD_ALIGN));
    bl.append(ptr); // 0, 67, 96
    }

    // These two must be concatenated into one memory + size aligned
    // buffer_ptr
    {
    buffer_ptr ptr(buffer::create_aligned(2, SIMD_ALIGN));
    ptr.set_offset(1);
    ptr.set_length(1);
    bl.append(ptr); // 1, 1, 32
    }
    {
    buffer_ptr ptr(buffer::create_aligned(BUFFER_SIZE - 1, SIMD_ALIGN));
    bl.append(ptr); // 0, 66, 96
    }

    EXPECT_FALSE(bl.is_aligned(SIMD_ALIGN));
    EXPECT_FALSE(bl.is_n_align_sized(BUFFER_SIZE));

    EXPECT_EQ(BUFFER_SIZE * 3, bl.length());
    EXPECT_FALSE(bl.front().is_aligned(SIMD_ALIGN));
    EXPECT_FALSE(bl.front().is_n_align_sized(BUFFER_SIZE));
    EXPECT_EQ(5, bl.get_num_buffers());
    bl.rebuild_aligned_size_and_memory(BUFFER_SIZE, SIMD_ALIGN);
    EXPECT_TRUE(bl.is_aligned(SIMD_ALIGN));
    EXPECT_TRUE(bl.is_n_align_sized(BUFFER_SIZE));
    EXPECT_EQ(3, bl.get_num_buffers());
}

TEST(BufferList, is_zero) {
    {
    buffer_list bl;
    EXPECT_TRUE(bl.is_zero());
    }
    {
    buffer_list bl;
    bl.append('A');
    EXPECT_FALSE(bl.is_zero());
    }
    {
    buffer_list bl;
    bl.append_zero(1);
    EXPECT_TRUE(bl.is_zero());
    }

    for (int i = 1; i <= 256; ++i) {
        buffer_list bl;
        bl.append_zero(i);
        EXPECT_TRUE(bl.is_zero());
        bl.append('A');
        EXPECT_FALSE(bl.is_zero());
    }
}

TEST(BufferList, clear) {
    buffer_list bl;
    uint64_t len = 17;
    bl.append_zero(len);
    bl.clear();
    EXPECT_EQ((unsigned)0, bl.length());
    EXPECT_EQ((unsigned)0, bl.get_num_buffers());
}

TEST(BufferList, push_back) {
    // void push_back(ptr& bp)
    {
    buffer_list bl;
    buffer_ptr ptr;
    bl.push_back(ptr);
    EXPECT_EQ(0, bl.length());
    EXPECT_EQ(0, bl.get_num_buffers());
    }

    uint64_t len = 17;
    {
    buffer_list bl;
    bl.append('A');
    buffer_ptr ptr(len);
    ptr.c_str()[0] = 'B';
    bl.push_back(ptr);
    EXPECT_EQ((1 + len), bl.length());
    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ('B', bl.back()[0]);
    const buffer_ptr& back_bp = bl.back();
    EXPECT_EQ(static_cast<instrumented_bptr&>(ptr).get_raw(),
              static_cast<const instrumented_bptr&>(back_bp).get_raw());
    }

    // void push_back(ptr&& bp)
    {
    buffer_list bl;
    buffer_ptr ptr;
    bl.push_back(std::move(ptr));
    EXPECT_EQ(0, bl.length());
    EXPECT_EQ(0, bl.get_num_buffers());
    }
    {
    buffer_list bl;
    bl.append('A');
    buffer_ptr ptr(len);
    ptr.c_str()[0] = 'B';
    bl.push_back(std::move(ptr));
    EXPECT_EQ((1 + len), bl.length());
    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ('B', bl.buffers().back()[0]);
    EXPECT_FALSE(static_cast<instrumented_bptr&>(ptr).get_raw());
    }
}

TEST(BufferList, is_contiguous) {
    buffer_list bl;
    EXPECT_TRUE(bl.is_contiguous());
    EXPECT_EQ(0, bl.get_num_buffers());

    bl.append('A');
    EXPECT_TRUE(bl.is_contiguous());
    EXPECT_EQ(1, bl.get_num_buffers());

    buffer_ptr ptr(1);
    bl.push_back(ptr);
    EXPECT_FALSE(bl.is_contiguous());
    EXPECT_EQ(2, bl.get_num_buffers());
}

TEST(BufferList, rebuild) {
    {
    buffer_ptr ptr(buffer::create_page_aligned(2));
    ptr[0] = 'X';
    ptr[1] = 'Y';
    ptr.set_offset(1);
    ptr.set_length(1);

    buffer_list bl;
    bl.append(ptr);
    EXPECT_FALSE(bl.is_page_aligned());

    bl.rebuild();
    EXPECT_EQ(1, bl.length());
    EXPECT_EQ('Y', *bl.begin());
    }

    {
    buffer_list bl;
    const std::string str(SPEC_PAGE_SIZE, 'X');
    bl.append(str.c_str(), str.size());
    bl.append(str.c_str(), str.size());
    EXPECT_EQ(2, bl.get_num_buffers());

    bl.rebuild();
    EXPECT_TRUE(bl.is_page_aligned());
    EXPECT_EQ(1, bl.get_num_buffers());
    }

    {
    char t1[] = "X";
    buffer_list a2;
    a2.append(t1, 1);

    buffer_list bl;
    bl.rebuild();
    bl.append(a2);
    EXPECT_EQ(1, bl.length());

    buffer_list::iterator p = bl.begin();
    char dst[1];
    p.copy(1, dst);
    EXPECT_EQ(0, memcmp(dst, "X", 1));
    }
}

TEST(BufferList, rebuild_page_aligned) {
    {
    buffer_list bl;
    {
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE + 1));
    ptr.set_offset(1);
    ptr.set_length(SPEC_PAGE_SIZE);
    bl.append(ptr);
    }
    EXPECT_EQ((unsigned)1, bl.get_num_buffers());
    EXPECT_FALSE(bl.is_page_aligned());
    bl.rebuild_page_aligned();
    EXPECT_TRUE(bl.is_page_aligned());
    EXPECT_EQ((unsigned)1, bl.get_num_buffers());
    }

    {
    buffer_list bl;
    buffer_ptr ptr(buffer::create_page_aligned(1));
    char *p = ptr.c_str();
    bl.append(ptr);
    bl.rebuild_page_aligned();
    EXPECT_EQ(p, bl.front().c_str());
    }

    {
    buffer_list bl;
    {
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE));
    EXPECT_TRUE(ptr.is_page_aligned());
    EXPECT_TRUE(ptr.is_n_page_sized());
    bl.append(ptr);
    }
    {
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE + 1));
    EXPECT_TRUE(ptr.is_page_aligned());
    EXPECT_FALSE(ptr.is_n_page_sized());
    bl.append(ptr);
    }
    {
    buffer_ptr ptr(buffer::create_page_aligned(2));
    ptr.set_offset(1);
    ptr.set_length(1);
    EXPECT_FALSE(ptr.is_page_aligned());
    EXPECT_FALSE(ptr.is_n_page_sized());
    bl.append(ptr);
    }
    {
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE - 2));
    EXPECT_TRUE(ptr.is_page_aligned());
    EXPECT_FALSE(ptr.is_n_page_sized());
    bl.append(ptr);
    }
    {
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE));
    EXPECT_TRUE(ptr.is_page_aligned());
    EXPECT_TRUE(ptr.is_n_page_sized());
    bl.append(ptr);
    }
    {
    buffer_ptr ptr(buffer::create_page_aligned(SPEC_PAGE_SIZE + 1));
    ptr.set_offset(1);
    ptr.set_length(SPEC_PAGE_SIZE);
    EXPECT_FALSE(ptr.is_page_aligned());
    EXPECT_TRUE(ptr.is_n_page_sized());
    bl.append(ptr);
    }
    EXPECT_EQ((unsigned)6, bl.get_num_buffers());
    EXPECT_TRUE((bl.length() & ~SPEC_PAGE_MASK) == 0);
    EXPECT_FALSE(bl.is_page_aligned());
    bl.rebuild_page_aligned();
    EXPECT_TRUE(bl.is_page_aligned());
    EXPECT_EQ((unsigned)4, bl.get_num_buffers());
    }
}

TEST(BufferList, operator_assign_rvalue) {
    buffer_list from;
    {
    buffer_ptr ptr(2);
    from.append(ptr);
    }

    buffer_list to;
    {
    buffer_ptr ptr(4);
    to.append(ptr);
    }

    EXPECT_EQ((unsigned)4, to.length());
    EXPECT_EQ((unsigned)1, to.get_num_buffers());
    to = std::move(from);
    EXPECT_EQ((unsigned)2, to.length());
    EXPECT_EQ((unsigned)1, to.get_num_buffers());
    EXPECT_EQ((unsigned)0, from.get_num_buffers());
    EXPECT_EQ((unsigned)0, from.length());
}

TEST(BufferList, claim_append) {
    buffer_list from;
    buffer_ptr ptr2(2);
    from.append(ptr2);

    buffer_list to;
    buffer_ptr ptr4(4);
    to.append(ptr4);

    EXPECT_EQ(4, to.length());
    EXPECT_EQ(1, to.get_num_buffers());
    to.claim_append(from);
    EXPECT_EQ((4 + 2), to.length());
    EXPECT_EQ(4, to.front().length());
    EXPECT_EQ(2, to.back().length());
    EXPECT_EQ(2, to.get_num_buffers());
    EXPECT_EQ(0, from.get_num_buffers());
    EXPECT_EQ(0, from.length());
}

TEST(BufferList, claim_append_piecewise) {
    buffer_list bl, t, dst;
    auto a = bl.get_page_aligned_appender(4);
    for (uint32_t i = 0; i < (SPEC_PAGE_SIZE + SPEC_PAGE_SIZE - 1333); i++) {
        a.append("x", 1);
    }
    a.flush();

    const char *p = bl.c_str();
    t.claim_append(bl);

    for (uint32_t i = 0; i < (SPEC_PAGE_SIZE + 1333); i++) {
        a.append("x", 1);
    }
    a.flush();
    t.claim_append(bl);

    EXPECT_FALSE(t.is_aligned_size_and_memory(SPEC_PAGE_SIZE, SPEC_PAGE_SIZE));
    dst.claim_append_piecewise(t);
    EXPECT_TRUE(dst.is_aligned_size_and_memory(SPEC_PAGE_SIZE, SPEC_PAGE_SIZE));
    const char *p1 = dst.c_str();
    EXPECT_TRUE(p == p1);
}

TEST(BufferList, begin) {
    buffer_list bl;
    bl.append("ABC");
    buffer_list::iterator it = bl.begin();
    EXPECT_EQ('A', *it);
}

TEST(BufferList, end) {
    buffer_list bl;
    bl.append("AB");
    buffer_list::iterator it = bl.end();
    bl.append("C");
    EXPECT_EQ('C', bl[it.get_off()]);
}

TEST(BufferList, append) {
    { // void append(char c);
    buffer_list bl;
    EXPECT_EQ(0, bl.get_num_buffers());
    bl.append('A');
    EXPECT_EQ(1, bl.get_num_buffers());
    }

    { // void append(const char *data, uint64_t len);
    buffer_list bl(SPEC_PAGE_SIZE);
    std::string str(SPEC_PAGE_SIZE * 2, 'X');
    bl.append(str.c_str(), str.size());
    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ(SPEC_PAGE_SIZE, bl.front().length());
    EXPECT_EQ(SPEC_PAGE_SIZE, bl.back().length());
    }

    { // void append(const std::string& s);
    buffer_list bl(SPEC_PAGE_SIZE);
    std::string str(SPEC_PAGE_SIZE * 2, 'X');
    bl.append(str);
    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ(SPEC_PAGE_SIZE, bl.front().length());
    EXPECT_EQ(SPEC_PAGE_SIZE, bl.back().length());
    }

    { // void append(const ptr& bp);
    buffer_list bl;
    EXPECT_EQ(0, bl.get_num_buffers());
    EXPECT_EQ(0, bl.length());
    {
    buffer_ptr ptr;
    bl.append(ptr);
    EXPECT_EQ(0, bl.get_num_buffers());
    EXPECT_EQ(0, bl.length());
    }
    {
    buffer_ptr ptr(3);
    bl.append(ptr);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(3, bl.length());
    }
    }

    { // void append(const ptr& bp, uint64_t off, uint64_t len);
    buffer_list bl;
    bl.append('A');
    buffer_ptr back(bl.back());
    buffer_ptr in(back);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(1, bl.length());
    {
    EXPECT_DEATH(bl.append(in, 100, 100), "");
    }
    EXPECT_LT(0, in.unused_tail_length());
    in.append('B');
    bl.append(in, back.end(), 1);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(2, bl.length());
    EXPECT_EQ('B', bl[1]);
    }

    {
    buffer_list bl;
    EXPECT_EQ(0, bl.get_num_buffers());
    EXPECT_EQ(0, bl.length());
    buffer_ptr ptr(2);
    ptr.set_length(0);
    ptr.append("AB", 2);
    bl.append(ptr, 1, 1);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(1, bl.length());
    }

    { // void append(const list& bl);
    buffer_list bl;
    bl.append('A');
    buffer_list other;
    other.append('B');
    bl.append(other);
    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ('B', bl[1]);
    }

    { // void append(std::istream& in);
    buffer_list bl;
    std::string expected("ABC\nDEF\n");
    std::istringstream is("ABC\n\nDEF");
    bl.append(is);
    EXPECT_EQ(0, ::memcmp(expected.c_str(), bl.c_str(), expected.size()));
    EXPECT_EQ(expected.size(), bl.length());
    }

    { // void append(ptr&& bp);
    buffer_list bl;
    EXPECT_EQ(0, bl.get_num_buffers());
    EXPECT_EQ(0, bl.length());
    {
    buffer_ptr ptr;
    bl.append(std::move(ptr));
    EXPECT_EQ(0, bl.get_num_buffers());
    EXPECT_EQ(0, bl.length());
    }
    {
    buffer_ptr ptr(3);
    bl.append(std::move(ptr));
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(3, bl.length());
    EXPECT_FALSE(static_cast<instrumented_bptr&>(ptr).get_raw());
    }
    }
}

TEST(BufferList, append_hole) {
    {
    buffer_list bl;
    auto filler = bl.append_hole(1);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(1, bl.length());

    bl.append("BC", 2);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(3, bl.length());

    const char a = 'A';
    filler.copy_in(1, &a);
    EXPECT_EQ(3, bl.length());

    EXPECT_EQ(0, ::memcmp("ABC", bl.c_str(), 3));
    }

    {
    buffer_list bl;
    bl.append('A');
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(1, bl.length());

    auto filler = bl.append_hole(1);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(2, bl.length());

    bl.append('C');
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(3, bl.length());

    const char b = 'B';
    filler.copy_in(1, &b);
    EXPECT_EQ(3, bl.length());

    EXPECT_EQ(0, ::memcmp("ABC", bl.c_str(), 3));
    }
}

TEST(BufferList, append_zero) {
    buffer_list bl;
    bl.append('A');
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(1, bl.length());
    bl.append_zero(1);
    EXPECT_EQ(1, bl.get_num_buffers());
    EXPECT_EQ(2, bl.length());
    EXPECT_EQ('\0', bl[1]);
}

TEST(BufferList, operator_brackets) {
    buffer_list bl;
    EXPECT_THROW(bl[1], buffer::end_of_buffer);
    bl.append('A');
    buffer_list other;
    other.append('B');
    bl.append(other);
    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ('B', bl[1]);
}

TEST(BufferList, c_str) {
    buffer_list bl;
    EXPECT_EQ(nullptr, bl.c_str());

    bl.append('A');

    buffer_list other;
    other.append('B');
    bl.append(other);

    EXPECT_EQ(2, bl.get_num_buffers());
    EXPECT_EQ(0, ::memcmp("AB", bl.c_str(), 2));
}

TEST(BufferList, substr_of) {
    buffer_list bl;
    EXPECT_THROW(bl.substr_of(bl, 1, 1), buffer::end_of_buffer);
    const char *s[] = {
        "ABC",
        "DEF",
        "GHI",
        "JKL"
    };
    for (uint64_t i = 0; i < 4; i++) {
        buffer_ptr ptr(s[i], strlen(s[i]));
        bl.push_back(ptr);
    }
    EXPECT_EQ(4, bl.get_num_buffers());

    buffer_list other;
    other.append("TO BE CLEARED");
    other.substr_of(bl, 4, 4);
    EXPECT_EQ(2, other.get_num_buffers());
    EXPECT_EQ(4, other.length());
    EXPECT_EQ(0, ::memcmp("EFGH", other.c_str(), 4));
}

TEST(BufferList, splice) {
    buffer_list bl;
    EXPECT_THROW(bl.splice(1, 1), buffer::end_of_buffer);
    const char *s[] = {
        "ABC",
        "DEF",
        "GHI",
        "JKL"
    };
    for (uint64_t i = 0; i < 4; i++) {
        buffer_ptr ptr(s[i], strlen(s[i]));
        bl.push_back(ptr);
    }
    EXPECT_EQ(4, bl.get_num_buffers());
    bl.splice(0, 0);

    buffer_list other;
    other.append('X');
    bl.splice(4, 4, &other);
    EXPECT_EQ(3, other.get_num_buffers());
    EXPECT_EQ(5, other.length());
    EXPECT_EQ(0, ::memcmp("XEFGH", other.c_str(), other.length()));
    EXPECT_EQ(8, bl.length());
    {
    buffer_list tmp(bl);
    EXPECT_EQ(0, ::memcmp("ABCDIJKL", tmp.c_str(), tmp.length()));
    }

    bl.splice(4, 4);
    EXPECT_EQ(4, bl.length());
    EXPECT_EQ(0, ::memcmp("ABCD", bl.c_str(), bl.length()));

    {
    bl.clear();
    buffer_ptr ptr1("0123456789", 10);
    bl.push_back(ptr1);

    buffer_ptr ptr2("abcdefghij", 10);
    bl.append(ptr2, 5, 5);

    other.clear();
    bl.splice(10, 4, &other);
    EXPECT_EQ(11, bl.length());
    EXPECT_EQ(0, ::memcmp("fghi", other.c_str(), other.length()));
    }
}

TEST(BufferList, write) {
    std::ostringstream stream;
    buffer_list bl;
    bl.append("ABC");
    bl.write(1, 2, stream);
    EXPECT_EQ("BC", stream.str());
}

//https://www.base64encode.org/
TEST(BufferList, encode_base64) {
    buffer_list bl;
    bl.append("ReplicWBCache");
    buffer_list other;
    bl.encode_base64(other);
    const char* expected = "UmVwbGljV0JDYWNoZQ==";
    EXPECT_EQ(0, ::memcmp(expected, other.c_str(), strlen(expected)));
}

//https://www.base64decode.org/
TEST(BufferList, decode_base64) {
    buffer_list bl;
    bl.append("UmVwbGljV0JDYWNoZQ==");
    buffer_list other;
    other.decode_base64(bl);
    const char *expected = "ReplicWBCache";
    EXPECT_EQ(0, ::memcmp(expected, other.c_str(), strlen(expected)));
    buffer_list malformed;
    malformed.append("UmVwbGljV0JDYWNoZQ");
    EXPECT_THROW(other.decode_base64(malformed), buffer::malformed_input);
}

TEST(BufferList, hexdump) {
    buffer_list bl;
    std::ostringstream stream;
    bl.append("013245678901234\0006789012345678901234", 32);
    bl.hexdump(stream);
    EXPECT_EQ("00000000  30 31 33 32 34 35 36 37  38 39 30 31 32 33 34 00  |013245678901234.|\n"
	          "00000010  36 37 38 39 30 31 32 33  34 35 36 37 38 39 30 31  |6789012345678901|\n"
	          "00000020\n",
	          stream.str());
}

TEST(BufferList, read_file) {
    std::string error;
    buffer_list bl;
    ::unlink(FILENAME);
    EXPECT_EQ(-ENOENT, bl.read_file("UNLIKELY", &error));

    snprintf(cmd, sizeof(cmd), "echo ABC > %s ; chmod 0 %s", FILENAME, FILENAME);
    EXPECT_EQ(0, ::system(cmd));

    if (getuid() != 0) {
        EXPECT_EQ(-EACCES, bl.read_file(FILENAME, &error));
    }

    snprintf(cmd, sizeof(cmd), "chmod +r %s", FILENAME);
    EXPECT_EQ(0, ::system(cmd));
    EXPECT_EQ(0, bl.read_file(FILENAME, &error));
    ::unlink(FILENAME);
    EXPECT_EQ(4, bl.length());
    std::string actual(bl.c_str(), bl.length());
    EXPECT_EQ("ABC\n", actual);
}

TEST(BufferList, read_fd) {
    int fd = -1;
    buffer_list bl;
    EXPECT_EQ(-EBADF, bl.read_fd(fd, 4));

    ::unlink(FILENAME);
    snprintf(cmd, sizeof(cmd), "echo ABC > %s", FILENAME);
    EXPECT_EQ(0, ::system(cmd));
    fd = ::open(FILENAME, O_RDONLY);
    ASSERT_NE(-1, fd);

    EXPECT_EQ(4, bl.read_fd(fd, 4));
    EXPECT_EQ(4, bl.length());
    ::close(fd);
    ::unlink(FILENAME);
}

TEST(BufferList, write_file) {
    ::unlink(FILENAME);

    int mode = 0600;
    buffer_list bl;
    EXPECT_EQ(-ENOENT, bl.write_file("un/like/ly", mode));

    bl.append("ABC");
    EXPECT_EQ(0, bl.write_file(FILENAME, mode));

    struct stat st;
    memset(&st, 0, sizeof(st));
    ASSERT_EQ(0, ::stat(FILENAME, &st));
    EXPECT_EQ(bl.length(), st.st_size);
    EXPECT_EQ((unsigned)(mode | S_IFREG), st.st_mode);

    ::unlink(FILENAME);
}

TEST(BufferList, write_fd) {
    ::unlink(FILENAME);

    int fd = ::open(FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    ASSERT_NE(-1, fd);

    buffer_list bl;
    for (int i = 0; i < IOV_MAX * 2; i++) {
        buffer_ptr ptr("A", 1);
        bl.push_back(ptr);
    }
    EXPECT_EQ(0, bl.write_fd(fd));
    ::close(fd);

    struct stat st;
    memset(&st, 0, sizeof(st));
    ASSERT_EQ(0, ::stat(FILENAME, &st));
    EXPECT_EQ(IOV_MAX * 2, st.st_size);
    ::unlink(FILENAME);
}

TEST(BufferList, write_fd_offset) {
    ::unlink(FILENAME);

    int fd = ::open(FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    ASSERT_NE(-1, fd);

    buffer_list bl;
    for (unsigned i = 0; i < IOV_MAX * 2; i++) {
      buffer_ptr ptr("A", 1);
      bl.push_back(ptr);
    }

    uint64_t offset = 200;
    EXPECT_EQ(0, bl.write_fd(fd, offset));
    ::close(fd);

    struct stat st;
    memset(&st, 0, sizeof(st));
    ASSERT_EQ(0, ::stat(FILENAME, &st));
    EXPECT_EQ(IOV_MAX * 2 + offset, (unsigned)st.st_size);
    ::unlink(FILENAME);
}

TEST(BufferList, crc32c) {
    buffer_list bl;
    uint32_t crc = 0;
    bl.append("A");
    crc = bl.crc32c(crc);
    EXPECT_EQ(0xB3109EBF, crc);
    crc = bl.crc32c(crc);
    EXPECT_EQ(0x5FA5C0CC, crc);
}

TEST(BufferList, crc32c_append) {
    buffer_list bl1;
    buffer_list bl2;

    for (int j = 0; j < 200; ++j) {
        buffer_list bl;
        for (int i = 0; i < 200; ++i) {
            char x = rand();
            bl.append(x);
            bl1.append(x);
        }
        bl.crc32c(rand()); // mess with the cached buffer_ptr crc values
        bl2.append(bl);
    }
    ASSERT_EQ(bl1.crc32c(0), bl2.crc32c(0));
}

TEST(BufferList, crc32c_zeros) {
    char buffer[4 * 1024];
    for (uint64_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] = i;
    }

    buffer_list bla;
    buffer_list blb;

    for (uint64_t j=0; j < 1000; j++) {
        buffer_ptr a(buffer, sizeof(buffer));

        bla.push_back(a);
        uint32_t crca = bla.crc32c(111);

        blb.push_back(a);
        uint32_t crcb = spec_crc32c(111, (unsigned char*)blb.c_str(), blb.length());

        EXPECT_EQ(crca, crcb);
    }
}

TEST(BufferList, crc32c_append_perf) {
    int len = 256 * 1024 * 1024;
    buffer_ptr a(len);
    buffer_ptr b(len);
    buffer_ptr c(len);
    buffer_ptr d(len);
    std::cout << "populating large buffers (a, b=c=d)" << std::endl;
    char *pa = a.c_str();
    char *pb = b.c_str();
    char *pc = c.c_str();
    char *pd = c.c_str();
    for (int i=0; i<len; i++) {
        pa[i] = (i & 0xff) ^ 73;
        pb[i] = (i & 0xff) ^ 123;
        pc[i] = (i & 0xff) ^ 123;
        pd[i] = (i & 0xff) ^ 123;
    }

    // track usage of cached crcs
    buffer::track_cached_crc(true);

    int base_cached = buffer::get_cached_crc();
    int base_cached_adjusted = buffer::get_cached_crc_adjusted();

    buffer_list bla;
    bla.push_back(a);

    buffer_list blb;
    blb.push_back(b);
    {
    utime_t start = spec_clock_now();
    uint32_t r = bla.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)len / (float)(1024*1024) / (float)(end - start);
    std::cout << "a.crc32c(0) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 1138817026u);
    }
    spec_assert(buffer::get_cached_crc() == 0 + base_cached);

    {
    utime_t start = spec_clock_now();
    uint32_t r = bla.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)len / (float)(1024*1024) / (float)(end - start);
    std::cout << "a.crc32c(0) (again) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 1138817026u);
    }
    spec_assert(buffer::get_cached_crc() == 1 + base_cached);

    {
    utime_t start = spec_clock_now();
    uint32_t r = bla.crc32c(5);
    utime_t end = spec_clock_now();
    float rate = (float)len / (float)(1024*1024) / (float)(end - start);
    std::cout << "a.crc32c(5) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 3239494520u);
    }
    spec_assert(buffer::get_cached_crc() == 1 + base_cached);
    spec_assert(buffer::get_cached_crc_adjusted() == 1 + base_cached_adjusted);

    {
    utime_t start = spec_clock_now();
    uint32_t r = bla.crc32c(5);
    utime_t end = spec_clock_now();
    float rate = (float)len / (float)(1024*1024) / (float)(end - start);
    std::cout << "a.crc32c(5) (again) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 3239494520u);
    }
    spec_assert(buffer::get_cached_crc() == 1 + base_cached);
    spec_assert(buffer::get_cached_crc_adjusted() == 2 + base_cached_adjusted);

    {
    utime_t start = spec_clock_now();
    uint32_t r = blb.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)len / (float)(1024*1024) / (float)(end - start);
    std::cout << "b.crc32c(0) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 2481791210u);
    }
    spec_assert(buffer::get_cached_crc() == 1 + base_cached);

    {
    utime_t start = spec_clock_now();
    uint32_t r = blb.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)len / (float)(1024*1024) / (float)(end - start);
    std::cout << "b.crc32c(0) (again)= " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 2481791210u);
    }
    spec_assert(buffer::get_cached_crc() == 2 + base_cached);

    buffer_list ab;
    ab.push_back(a);
    ab.push_back(b);
    {
    utime_t start = spec_clock_now();
    uint32_t r = ab.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)ab.length() / (float)(1024*1024) / (float)(end - start);
    std::cout << "ab.crc32c(0) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 2988268779u);
    }
    spec_assert(buffer::get_cached_crc() == 3 + base_cached);
    spec_assert(buffer::get_cached_crc_adjusted() == 3 + base_cached_adjusted);

    buffer_list ac;
    ac.push_back(a);
    ac.push_back(c);
    {
    utime_t start = spec_clock_now();
    uint32_t r = ac.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)ac.length() / (float)(1024*1024) / (float)(end - start);
    std::cout << "ac.crc32c(0) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 2988268779u);
    }
    spec_assert(buffer::get_cached_crc() == 4 + base_cached);
    spec_assert(buffer::get_cached_crc_adjusted() == 3 + base_cached_adjusted);

    buffer_list ba;
    ba.push_back(b);
    ba.push_back(a);
    {
    utime_t start = spec_clock_now();
    uint32_t r = ba.crc32c(0);
    utime_t end = spec_clock_now();
    float rate = (float)ba.length() / (float)(1024*1024) / (float)(end - start);
    std::cout << "ba.crc32c(0) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 169240695u);
    }
    spec_assert(buffer::get_cached_crc() == 5 + base_cached);
    spec_assert(buffer::get_cached_crc_adjusted() == 4 + base_cached_adjusted);

    {
    utime_t start = spec_clock_now();
    uint32_t r = ba.crc32c(5);
    utime_t end = spec_clock_now();
    float rate = (float)ba.length() / (float)(1024*1024) / (float)(end - start);
    std::cout << "ba.crc32c(5) = " << r << " at " << rate << " MB/sec" << std::endl;
    ASSERT_EQ(r, 1265464778u);
    }
    spec_assert(buffer::get_cached_crc() == 5 + base_cached);
    spec_assert(buffer::get_cached_crc_adjusted() == 6 + base_cached_adjusted);

    std::cout << "crc cache hits (same start) = " << buffer::get_cached_crc() << std::endl;
    std::cout << "crc cache hits (adjusted) = " << buffer::get_cached_crc_adjusted() << std::endl;
}

TEST(BufferList, compare) {
    buffer_list a;
    a.append("A");

    buffer_list ab; // AB in segments
    ab.append(buffer_ptr("A", 1));
    ab.append(buffer_ptr("B", 1));

    buffer_list ac;
    ac.append("AC");

    // bool operator>(buffer_list& lhs, buffer_list& rhs)
    ASSERT_FALSE(a > ab);
    ASSERT_TRUE(ab > a);
    ASSERT_TRUE(ac > ab);
    ASSERT_FALSE(ab > ac);
    ASSERT_FALSE(ab > ab);

    // bool operator>=(buffer_list& lhs, buffer_list& rhs)
    ASSERT_FALSE(a >= ab);
    ASSERT_TRUE(ab >= a);
    ASSERT_TRUE(ac >= ab);
    ASSERT_FALSE(ab >= ac);
    ASSERT_TRUE(ab >= ab);

    // bool operator<(buffer_list& lhs, buffer_list& rhs)
    ASSERT_TRUE(a < ab);
    ASSERT_FALSE(ab < a);
    ASSERT_FALSE(ac < ab);
    ASSERT_TRUE(ab < ac);
    ASSERT_FALSE(ab < ab);

    // bool operator<=(buffer_list& lhs, buffer_list& rhs)
    ASSERT_TRUE(a <= ab);
    ASSERT_FALSE(ab <= a);
    ASSERT_FALSE(ac <= ab);
    ASSERT_TRUE(ab <= ac);
    ASSERT_TRUE(ab <= ab);

    // bool operator==(buffer_list &lhs, buffer_list &rhs)
    ASSERT_FALSE(a == ab);
    ASSERT_FALSE(ac == ab);
    ASSERT_TRUE(ab == ab);
}

TEST(BufferList, ostream) {
    std::ostringstream stream;
    buffer_list bl;
    const char *s[] = {
        "ABC",
        "DEF"
    };
    for (uint64_t i = 0; i < 2; i++) {
        buffer_ptr ptr(s[i], strlen(s[i]));
        bl.push_back(ptr);
    }
    stream << bl;
    std::cerr << stream.str() << std::endl;
    EXPECT_GT(stream.str().size(), stream.str().find("list:(len=6,"));
    EXPECT_GT(stream.str().size(), stream.str().find("len 3 nref 1),\n"));
    EXPECT_GT(stream.str().size(), stream.str().find("len 3 nref 1)\n"));
}

TEST(BufferList, zero) {
    { // void zero()
    buffer_list bl;
    bl.append('A');
    EXPECT_EQ('A', bl[0]);
    bl.zero();
    EXPECT_EQ('\0', bl[0]);
    }

    // void zero(uint64_t off, uint64_t len)
    const char *s[] = {
        "ABC",
        "DEF",
        "GHI",
        "KLM"
    };

    {
    buffer_list bl;
    buffer_ptr ptr(s[0], strlen(s[0]));
    bl.push_back(ptr);
    bl.zero(0, 1);
    EXPECT_EQ(0, ::memcmp("\0BC", bl.c_str(), 3));
    }

    {
    buffer_list bl;
    for (unsigned i = 0; i < 4; i++) {
        buffer_ptr ptr(s[i], strlen(s[i]));
        bl.push_back(ptr);
    }
    EXPECT_DEATH(bl.zero(0, 2000), "");
    bl.zero(2, 5);
    EXPECT_EQ(0, ::memcmp("AB\0\0\0\0\0HIKLM", bl.c_str(), 9));
    }

    {
    buffer_list bl;
    for (unsigned i = 0; i < 4; i++) {
        buffer_ptr ptr(s[i], strlen(s[i]));
        bl.push_back(ptr);
    }
    bl.zero(3, 3);
    EXPECT_EQ(0, ::memcmp("ABC\0\0\0GHIKLM", bl.c_str(), 9));
    }

    {
    buffer_list bl;
    buffer_ptr ptr1(4);
    buffer_ptr ptr2(4);
    memset(ptr1.c_str(), 'a', 4);
    memset(ptr2.c_str(), 'b', 4);
    bl.append(ptr1);
    bl.append(ptr2);
    bl.zero(2, 4);
    EXPECT_EQ(0, ::memcmp("aa\0\0\0\0bb", bl.c_str(), 8));
    }
}

TEST(BufferList, EmptyAppend) {
    buffer_list bl;
    buffer_ptr ptr;
    bl.push_back(ptr);
    ASSERT_EQ(bl.begin().end(), 1);
}

TEST(BufferList, InternalCarriage) {
    spec::buffer_list bl;
    EXPECT_EQ(bl.get_num_buffers(), 0u);

    {
    buffer_list bl_with_foo;
    bl_with_foo.append("foo", 3);
    EXPECT_EQ(bl_with_foo.length(), 3u);
    EXPECT_EQ(bl_with_foo.get_num_buffers(), 1u);

    bl.append(bl_with_foo);
    EXPECT_EQ(bl.get_num_buffers(), 1u);
    }
}

//???
TEST(BufferList, ContiguousAppender) {
    spec::buffer_list bl;
    EXPECT_EQ(bl.get_num_buffers(), 0);

    // expect a flush in ~contiguous_appender
    auto ap = bl.get_contiguous_appender(100);
    EXPECT_EQ(bl.get_num_buffers(), 1);

    // append buffer_list with single ptr inside. This should
    // commit changes to bl::_len and the underlying bp::len.
    {
    buffer_list bl_foo;
    bl_foo.append("foo", 3);
    EXPECT_EQ(bl_foo.length(), 3);
    EXPECT_EQ(bl_foo.get_num_buffers(), 1);

    ap.append(bl_foo);
    // ap::append(const bl&) splits the bp with free space.
    EXPECT_EQ(bl.get_num_buffers(), 3);
    }

    EXPECT_EQ(bl.get_num_buffers(), 3);
}

TEST(BufferList, TestPtrAppend) {
    buffer_list bl;
    char correct[MAX_TEST];
    int curpos = 0;
    int length = random() % 5 > 0 ? random() % 1000 : 0;
    while (curpos + length < MAX_TEST) {
        if (!length) {
            buffer_ptr ptr;
            bl.push_back(ptr);
        } else {
            char *current = correct + curpos;
            for (int i = 0; i < length; ++i) {
                char next = random() % 255;
                correct[curpos++] = next;
            }
            buffer_ptr ptr(current, length);
            bl.append(ptr);
        }
        length = random() % 5 > 0 ? random() % 1000 : 0;
    }
    ASSERT_EQ(memcmp(bl.c_str(), correct, curpos), 0);
}

TEST(BufferList, TestDirectAppend) {
    buffer_list bl;
    char correct[MAX_TEST];
    int curpos = 0;
    int length = random() % 5 > 0 ? random() % 1000 : 0;
    while (curpos + length < MAX_TEST) {
        char *current = correct + curpos;
        for (int i = 0; i < length; ++i) {
            char next = random() % 255;
            correct[curpos++] = next;
        }
        bl.append(current, length);
        length = random() % 5 > 0 ? random() % 1000 : 0;
    }
    ASSERT_EQ(memcmp(bl.c_str(), correct, curpos), 0);
}

TEST(BufferList, TestCopyAll) {
    const static size_t buffer_size = 10737414;
    std::shared_ptr <unsigned char> big(
      (unsigned char*)malloc(buffer_size), free);
    unsigned char c = 0;
    for (size_t i = 0; i < buffer_size; ++i) {
        big.get()[0] = c++;
    }

    buffer_list bl;
    bl.append((const char*)big.get(), buffer_size);

    buffer_list::iterator it = bl.begin();
    buffer_list bl2;
    it.copy_all(bl2);
    ASSERT_EQ(bl2.length(), buffer_size);
    std::shared_ptr <unsigned char> big2(
      (unsigned char*)malloc(buffer_size), free);
    bl2.begin().copy(buffer_size, (char*)big2.get());
    ASSERT_EQ(memcmp(big.get(), big2.get(), buffer_size), 0);
}

TEST(BufferList, InvalidateCrc) {
    const static size_t buffer_size = 262144;
    std::shared_ptr <unsigned char> big(
        (unsigned char*)malloc(buffer_size), free);
    unsigned char c = 0;
    for (size_t i = 0; i < buffer_size; ++i) {
        big.get()[i] = c++;
    }
    buffer_list bl;

    // test for crashes (shouldn't crash)
    bl.invalidate_crc();

    // put data into buffer_list
    bl.append((const char*)big.get(), buffer_size);

    // get its crc
    uint32_t crc = bl.crc32c(0);

    // modify data in bl without notification
    char* inptr = (char*) bl.c_str();
    c = 0;
    for (size_t i = 0; i < buffer_size; ++i) {
        inptr[i] = c--;
    }

    // make sure data in bl are now different
    EXPECT_NE(memcmp((void*)(big.get()), (void*)inptr, buffer_size), 0);

    // crc should remain the same because of the cached crc
    uint32_t new_crc = bl.crc32c(0);
    EXPECT_EQ(crc, new_crc);

    // force crc invalidate, check if it is updated
    bl.invalidate_crc();
    EXPECT_NE(crc, bl.crc32c(0));
}

TEST(BufferList, TestIsProvidedBuffer) {
    char buff[100];
    buffer_list bl;
    bl.push_back(buffer::create_static(100, buff));
    ASSERT_TRUE(bl.is_provided_buffer(buff));
    bl.append_zero(100);
    ASSERT_FALSE(bl.is_provided_buffer(buff));
}

TEST(BufferList, DanglingLastP) {
    buffer_list bl;
    {
    buffer_ptr bp(buffer::create(10));
    bp.copy_in(0, 3, "XXX");
    bl.push_back(std::move(bp));
    EXPECT_EQ(0, ::memcmp("XXX", bl.c_str(), 3));

    bl.begin().copy_in(2, "AB");
    EXPECT_EQ(0, ::memcmp("ABX", bl.c_str(), 3));
    }

    buffer_list empty;
    bl = const_cast<const buffer_list&>(empty);
    bl.append("123");

    bl.begin(2).copy_in(1, "C");
    EXPECT_EQ(0, ::memcmp("12C", bl.c_str(), 3));
}

TEST(BufferHash, all) {
    {
    buffer_list bl;
    bl.append("A");
    buffer_hash hash;
    EXPECT_EQ(0, hash.digest());
    hash.update(bl);
    EXPECT_EQ(0xB3109EBF, hash.digest());
    hash.update(bl);
    EXPECT_EQ(0x5FA5C0CC, hash.digest());
    }

    {
    buffer_list bl;
    bl.append("A");
    buffer_hash hash;
    EXPECT_EQ(0, hash.digest());
    buffer_hash& returned_hash =  hash << bl;
    EXPECT_EQ(&returned_hash, &hash);
    EXPECT_EQ(0xB3109EBF, hash.digest());
    }
}
