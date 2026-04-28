#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <random>
#include <string>

#include "ws/backend.hpp"

namespace ws {

class GlobalQueueBackend final : public ITaskBackend {
public:
    explicit GlobalQueueBackend(std::size_t workers);

    std::string name() const override;
    std::size_t worker_count() const override;
    EnqueueStatus enqueue(std::size_t worker_id, Task task) override;
    DequeueResult dequeue(std::size_t worker_id, std::mt19937_64& rng) override;
    BackendMetrics metrics() const override;

private:
    std::size_t workers_;
    std::deque<Task> queue_;
    mutable std::mutex mutex_;
};

}  // namespace ws
