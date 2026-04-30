#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "ws/abp_deque.hpp"
#include "ws/backend.hpp"

namespace ws {

class CircularArray {
public:
    explicit CircularArray(std::size_t log_capacity);

    std::size_t capacity() const;
    Task get(std::int64_t logical_index) const;
    void put(std::int64_t logical_index, Task task);
    std::shared_ptr<CircularArray> grow(std::int64_t top, std::int64_t bottom) const;

private:
    std::size_t index_for(std::int64_t logical_index) const;

    std::size_t log_capacity_;
    std::size_t capacity_;
    mutable std::vector<Task> slots_;
};

class ChaseLevDeque {
public:
    explicit ChaseLevDeque(std::size_t initial_log_capacity);

    void push_bottom(Task task);
    Task pop_bottom();
    StealResult pop_top();
    std::uint64_t resize_count() const;

private:
    std::shared_ptr<CircularArray> array_;
    std::atomic<std::int64_t> top_;
    std::atomic<std::int64_t> bottom_;
    std::atomic<std::uint64_t> resizes_;
};

class ChaseLevBackend final : public ITaskBackend {
public:
    ChaseLevBackend(std::size_t workers, std::size_t initial_log_capacity,
                    std::size_t steal_attempts_per_poll);

    std::string name() const override;
    std::size_t worker_count() const override;
    EnqueueStatus enqueue(std::size_t worker_id, Task task) override;
    DequeueResult dequeue(std::size_t worker_id, std::mt19937_64& rng) override;
    BackendMetrics metrics() const override;

private:
    std::size_t workers_;
    std::size_t steal_attempts_per_poll_;
    std::vector<std::unique_ptr<ChaseLevDeque>> deques_;
};

}  // namespace ws
