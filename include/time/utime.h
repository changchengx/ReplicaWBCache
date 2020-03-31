/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef SPEC_UTIME_H
#define SPEC_UTIME_H

#include <math.h>
#include <limits>
#include <time.h>
#include <iostream>
#include <iomanip>

inline uint32_t cap_to_u32_max(uint64_t t) {
    return std::min(t, (uint64_t)std::numeric_limits<uint32_t>::max());
}

class utime_t {
public:
    struct {
        uint32_t tv_sec, tv_nsec;
    } tv;

public:
    bool is_zero() const {
        return (tv.tv_sec == 0) && (tv.tv_nsec == 0);
    }

    void normalize() {
        if (tv.tv_nsec > 1000000000ul) {
            tv.tv_sec = cap_to_u32_max(tv.tv_sec + tv.tv_nsec / (1000000000ul));
            tv.tv_nsec %= 1000000000ul;
        }
    }

    void set_from_double(double d) {
        tv.tv_sec = (uint32_t)trunc(d);
        tv.tv_nsec = (uint32_t)((d - (double)tv.tv_sec) * 1000000000.0);
    }

    utime_t() {
        tv.tv_sec = 0;
        tv.tv_nsec = 0;
    }
    utime_t(time_t s, int n) {
        tv.tv_sec = s;
        tv.tv_nsec = n;
        normalize();
    }
    utime_t(const struct timespec v) {
        tv.tv_sec = v.tv_sec;
        tv.tv_nsec = v.tv_nsec;
    }
    utime_t(const struct timeval &v) {
        set_from_timeval(&v);
    }
    utime_t(const struct timeval *v) {
        set_from_timeval(v);
    }
    void to_timespec(struct timespec *ts) const {
        ts->tv_sec = tv.tv_sec;
        ts->tv_nsec = tv.tv_nsec;
    }

    time_t sec() const {
        return tv.tv_sec;
    }
    uint32_t& sec_ref() {
        return tv.tv_sec;
    }

    uint32_t usec() const {
        return tv.tv_nsec/1000;
    }

    uint32_t nsec() const {
        return tv.tv_nsec;
    }
    uint32_t& nsec_ref() {
        return tv.tv_nsec;
    }

    uint64_t to_nsec() const {
        return (uint64_t)tv.tv_nsec + (uint64_t)tv.tv_sec * 1000000000ull;
    }
    uint64_t to_msec() const {
        return (uint64_t)tv.tv_nsec / 1000000ull + (uint64_t)tv.tv_sec * 1000ull;
    }

    void copy_to_timeval(struct timeval *v) const {
        v->tv_sec = tv.tv_sec;
        v->tv_usec = tv.tv_nsec/1000;
    }

    void set_from_timeval(const struct timeval *v) {
        tv.tv_sec = v->tv_sec;
        tv.tv_nsec = v->tv_usec*1000;
    }

    // cast to double
    operator double() const {
        return (double)sec() + ((double)nsec() / 1000000000.0L);
    }

    friend std::ostream& operator<<(std::ostream& out, const utime_t& val) {
        out.setf(std::ios::right);
        char oldfill = out.fill();
        out.fill('0');

        if (val.sec() < ((time_t)(60 * 60 * 24 * 365 * 10))) {
            out << (long)(val.sec()) << "." << std::setw(6) << (val.usec());
        } else {
            struct tm bdt;
            time_t tt = val.sec();
            localtime_r(&tt, &bdt);

            out << std::setw(4) << (bdt.tm_year + 1900)
                << '-' << std::setw(2) << (bdt.tm_mon + 1)
                << '-' << std::setw(2) << bdt.tm_mday;
            out << 'T';

            out << std::setw(2) << bdt.tm_hour
                << ':' << std::setw(2) << bdt.tm_min
                << ':' << std::setw(2) << bdt.tm_sec;
            out << "." << std::setw(6) << val.usec();

            char buf[32] = { 0 };
            strftime(buf, sizeof(buf), "%z", &bdt);
            out << buf;
        }

        out.fill(oldfill);
        out.unsetf(std::ios::right);
        return out;
   }
};

inline utime_t operator+(const utime_t& lhs, const utime_t& rhs) {
    uint64_t sec = (uint64_t)lhs.sec() + rhs.sec();
    return utime_t(cap_to_u32_max(sec), lhs.nsec() + rhs.nsec());
}

inline utime_t& operator+=(utime_t& lhs, const utime_t& rhs) {
    lhs.sec_ref() = cap_to_u32_max((uint64_t)lhs.sec() + rhs.sec());
    lhs.nsec_ref() += rhs.nsec();
    lhs.normalize();

    return lhs;
}

inline utime_t& operator+=(utime_t& lhs, double val) {
    double fs = trunc(val);
    double ns = (val - fs) * 1000000000.0;

    lhs.sec_ref() = cap_to_u32_max(lhs.sec() + (uint64_t)fs);
    lhs.nsec_ref() += (long)ns;
    lhs.normalize();

    return lhs;
}

inline utime_t operator-(const utime_t& lhs, const utime_t& rhs) {
    return utime_t(lhs.sec() - rhs.sec() - (lhs.nsec() < rhs.nsec() ? 1 : 0),
                  lhs.nsec() - rhs.nsec() + (lhs.nsec() < rhs.nsec() ? 1000000000 : 0));
}

inline utime_t& operator-=(utime_t& lhs, const utime_t& rhs) {
    lhs.sec_ref() -= rhs.sec();

    if (lhs.nsec() >= rhs.nsec())
      lhs.nsec_ref() -= rhs.nsec();
    else {
      lhs.nsec_ref() += 1000000000L - rhs.nsec();
      lhs.sec_ref()--;
    }

    return lhs;
}

inline utime_t& operator-=(utime_t& lhs, double val) {
    double fs = trunc(val);
    double ns = (val - fs) * 1000000000.0;

    lhs.sec_ref() -= (long)fs;

    long nsl = (long)ns;
    if (nsl) {
      lhs.sec_ref()--;
      lhs.nsec_ref() = 1000000000L + lhs.nsec_ref() - nsl;
    }

    lhs.normalize();

    return lhs;
}

inline bool operator>(const utime_t& a, const utime_t& b) {
    return (a.sec() > b.sec()) || (a.sec() == b.sec() && a.nsec() > b.nsec());
}
inline bool operator<=(const utime_t& a, const utime_t& b) {
    return !(a > b);
}
inline bool operator<(const utime_t& a, const utime_t& b) {
    return (a.sec() < b.sec()) || (a.sec() == b.sec() && a.nsec() < b.nsec());
}
inline bool operator>=(const utime_t& a, const utime_t& b) {
  return !(a < b);
}
inline bool operator==(const utime_t& a, const utime_t& b) {
  return a.sec() == b.sec() && a.nsec() == b.nsec();
}
inline bool operator!=(const utime_t& a, const utime_t& b) {
  return !(a == b);
}

#endif //SPEC_UTIME_H
