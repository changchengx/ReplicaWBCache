/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2020 Liu, Changcheng <changcheng.liu@aliyun.com>
 */

#ifndef CTX_H
#define CTX_H

#include <memory>
#include <cassert>

#include "Shared_Ptr.h"

class Context: public std::enable_shared_from_this<Context> {
private:
    Context(const Context& other) = delete;
    const Context& operator=(const Context& other) = delete;

private:
    std::shared_ptr<Context> m_self_reference = nullptr;
    void ref_self() {
        assert(!m_self_reference);
        m_self_reference = shared_from_this();
    }

protected:
    virtual void finish(int rst) = 0;

    /* retrun true if the variant of finish confirm it's safe
     * to be called synchronously.
     */
    virtual bool sync_finish(int rst) {
        return false;
    }

public:
    Context() = default;

    virtual ~Context() = default;

    virtual void complete(int rst) {
        finish(rst);
        if (m_self_reference != nullptr) {
            m_self_reference = nullptr;
        } else {
            delete this;
        }
    }

    virtual bool sync_complete(int rst) {
        if (sync_finish(rst)) {
            if (m_self_reference != nullptr) {
                m_self_reference = nullptr;
            } else {
                delete this;
            }
            return true;
        }
        return false;
    }

public:
    template <typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args) {
        std::shared_ptr<T> sp_ctx = std::make_shared<T>(std::forward<Args>(args)...);
        sp_ctx->ref_self();
        return sp_ctx;
    }
};

#endif //CTX_H
