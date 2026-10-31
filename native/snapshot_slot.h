// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
namespace vcop2 {
// Android libc++ does not implement atomic<shared_ptr> yet. Keep the bounded
// latest-frame contract with a short lock; release old payloads after unlocking.
template<class T> class SnapshotSlot {
    mutable std::mutex mutex_;
    std::shared_ptr<T> value_;
public:
    void store(std::shared_ptr<T> next,std::memory_order = std::memory_order_seq_cst) {
        {std::lock_guard lock(mutex_);value_.swap(next);}
    }
    std::shared_ptr<T> load(std::memory_order = std::memory_order_seq_cst) const {
        std::lock_guard lock(mutex_);return value_;
    }
};
}
