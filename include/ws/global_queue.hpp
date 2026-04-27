#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <utility>

#include "ws/backend.hpp"

namespace ws {

class GlobalQueueBackend final : public ITaskBackend {
public:
    explicit GlobalQueueBackend(std::size_t workers)
        : workers_(workers == 0 ? 1 : workers) {}

    std::string name() const override {
        return "global";
    }

    std::size_t worker_count() const override {
        return workers_;
    }

    EnqueueStatus enqueue(std::size_t, Task task) override {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(task));
        return EnqueueStatus::ok;
    }

    DequeueResult dequeue(std::size_t, std::mt19937_64&) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return {};
        }

        DequeueResult result;
        result.task = std::move(queue_.front());
        result.kind = DequeueKind::global;
        queue_.pop_front();
        return result;
    }

    BackendMetrics metrics() const override {
        return {};
    }

private:
    std::size_t workers_;
    std::deque<Task> queue_;
    mutable std::mutex mutex_;
};

}  // namespace ws
