#pragma once

#include <cstddef>
#include <memory>
#include <random>
#include <string>

#include "ws/metrics.hpp"
#include "ws/task.hpp"

namespace ws {

class ITaskBackend {
public:
    virtual ~ITaskBackend() = default;

    virtual std::string name() const = 0;
    virtual std::size_t worker_count() const = 0;

    virtual EnqueueStatus enqueue(std::size_t worker_id, Task task) = 0;
    virtual DequeueResult dequeue(std::size_t worker_id, std::mt19937_64& rng) = 0;
    virtual BackendMetrics metrics() const = 0;
};

}  // namespace ws
