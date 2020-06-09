/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef BUFFER_EXCEPTION_H
#define BUFFER_EXCEPTION_H

#include <stdio.h>

#include <string>
#include <exception>

#include "../error_info/errno.h"

namespace spec {

namespace buffer {

class error: public std::exception {
public:
    const char *what() const noexcept override {
        return "buffer::exception";
    }
};

class bad_alloc: public error {
public:
    const char *what() const noexcept override {
        return "buffer::bad_alloc";
    }
};

class end_of_buffer: public error {
public:
    const char *what() const noexcept override {
        return "buffer::end_of_buffer";
    }
};

class malformed_input: public error {
private:
    char error_info[256];
public:
    explicit
    malformed_input(const std::string& msg) {
       snprintf(error_info, sizeof(error_info), "buffer::malformed_input: %s", msg.c_str());
    }

    const char *what() const noexcept override {
        return error_info;
    }
};

class error_code: public malformed_input {
public:
    explicit
    error_code(int64_t error)
        :malformed_input(cpp_strerror(error).c_str()), code(error) {
    }
    int64_t code;
};

extern std::ostream& operator<<(std::ostream& out, const error& berror);

} //namespace: buffer
} //namespace: spec
#endif //BUFFER_EXCEPTION_H
