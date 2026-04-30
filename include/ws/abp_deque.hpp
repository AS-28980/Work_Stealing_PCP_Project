#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "ws/backend.hpp"

namespace ws {

enum class StealStatus { empty, abort, success };

struct StealResult {
    StealStatus status = StealStatus::empty;
    Task task;
};

class AbpDeque {
public:
    explicit AbpDeque(std::size_t capacity);

    bool push_bottom(Task task);
    Task pop_bottom();
    StealResult pop_top();

private:
    std::size_t index_for(std::int64_t logical_index) const;
    Task get(std::int64_t logical_index);
    void put(std::int64_t logical_index, Task task);

    std::size_t capacity_;
    std::vector<Task> slots_;
    std::atomic<std::int64_t> top_;
    std::atomic<std::int64_t> bottom_;
};

class AbpBackend final : public ITaskBackend {
public:
    AbpBackend(std::size_t workers, std::size_t capacity, std::size_t steal_attempts_per_poll);

    std::string name() const override;
    std::size_t worker_count() const override;
    EnqueueStatus enqueue(std::size_t worker_id, Task task) override;
    DequeueResult dequeue(std::size_t worker_id, std::mt19937_64& rng) override;
    BackendMetrics metrics() const override;

private:
    std::size_t workers_;
    std::size_t steal_attempts_per_poll_;
    std::vector<std::unique_ptr<AbpDeque>> deques_;
    std::atomic<std::uint64_t> overflows_;
};

}  // namespace ws
