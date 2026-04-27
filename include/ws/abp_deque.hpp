#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "ws/backend.hpp"

namespace ws {

enum class StealStatus {
    empty,
    abort,
    success
};

struct StealResult {
    StealStatus status = StealStatus::empty;
    Task task;
};

class AbpDeque {
public:
    explicit AbpDeque(std::size_t capacity)
        : capacity_(capacity < 2 ? 2 : capacity),
          slots_(capacity_),
          top_(0),
          bottom_(0) {}

    bool push_bottom(Task task) {
        // Fixed capacity is the ABP trade-off: overflow is reported to the
        // scheduler instead of silently overwriting a task.
        const auto bottom = bottom_.load(std::memory_order_relaxed);
        const auto top = top_.load(std::memory_order_acquire);
        if (bottom - top >= static_cast<std::int64_t>(capacity_)) {
            return false;
        }

        put(bottom, std::move(task));
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(bottom + 1, std::memory_order_release);
        return true;
    }

    Task pop_bottom() {
        // Only the owning worker calls pop_bottom(). The common multi-item path
        // avoids CAS; CAS is reserved for the last-item race against thieves.
        auto bottom = bottom_.load(std::memory_order_relaxed) - 1;
        bottom_.store(bottom, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);

        auto top = top_.load(std::memory_order_acquire);
        const auto size = bottom - top;
        if (size < 0) {
            bottom_.store(top, std::memory_order_relaxed);
            return {};
        }

        Task task = get(bottom);
        if (size > 0) {
            return task;
        }

        auto expected_top = top;
        if (top_.compare_exchange_strong(expected_top, top + 1,
                                         std::memory_order_seq_cst,
                                         std::memory_order_relaxed)) {
            bottom_.store(top + 1, std::memory_order_relaxed);
            return task;
        }

        bottom_.store(expected_top, std::memory_order_relaxed);
        return {};
    }

    StealResult pop_top() {
        // Thieves compete by advancing top. A failed CAS is an abort, which the
        // benchmark records as a failed steal attempt.
        const auto top = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        const auto bottom = bottom_.load(std::memory_order_acquire);
        if (bottom <= top) {
            return {StealStatus::empty, {}};
        }

        Task task = get(top);
        auto expected_top = top;
        if (top_.compare_exchange_strong(expected_top, top + 1,
                                         std::memory_order_seq_cst,
                                         std::memory_order_relaxed)) {
            return {StealStatus::success, task};
        }
        return {StealStatus::abort, {}};
    }

private:
    std::size_t index_for(std::int64_t logical_index) const {
        return static_cast<std::size_t>(logical_index) % capacity_;
    }

    Task get(std::int64_t logical_index) {
        return std::atomic_load_explicit(&slots_[index_for(logical_index)],
                                         std::memory_order_acquire);
    }

    void put(std::int64_t logical_index, Task task) {
        std::atomic_store_explicit(&slots_[index_for(logical_index)],
                                   std::move(task),
                                   std::memory_order_release);
    }

    std::size_t capacity_;
    std::vector<Task> slots_;
    std::atomic<std::int64_t> top_;
    std::atomic<std::int64_t> bottom_;
};

class AbpBackend final : public ITaskBackend {
public:
    AbpBackend(std::size_t workers, std::size_t capacity, std::size_t steal_attempts_per_poll)
        : workers_(workers == 0 ? 1 : workers),
          steal_attempts_per_poll_(steal_attempts_per_poll == 0 ? workers_ : steal_attempts_per_poll),
          overflows_(0) {
        deques_.reserve(workers_);
        for (std::size_t i = 0; i < workers_; ++i) {
            deques_.push_back(std::make_unique<AbpDeque>(capacity));
        }
    }

    std::string name() const override {
        return "abp";
    }

    std::size_t worker_count() const override {
        return workers_;
    }

    EnqueueStatus enqueue(std::size_t worker_id, Task task) override {
        const std::size_t owner = worker_id % workers_;
        if (!deques_[owner]->push_bottom(std::move(task))) {
            overflows_.fetch_add(1, std::memory_order_relaxed);
            return EnqueueStatus::overflow;
        }
        return EnqueueStatus::ok;
    }

    DequeueResult dequeue(std::size_t worker_id, std::mt19937_64& rng) override {
        DequeueResult result;
        if (Task local = deques_[worker_id]->pop_bottom()) {
            result.task = std::move(local);
            result.kind = DequeueKind::local;
            return result;
        }

        if (workers_ == 1) {
            return result;
        }

        std::uniform_int_distribution<std::size_t> dist(0, workers_ - 1);
        for (std::size_t attempt = 0; attempt < steal_attempts_per_poll_; ++attempt) {
            std::size_t victim = dist(rng);
            if (victim == worker_id) {
                victim = (victim + 1) % workers_;
            }

            ++result.steal_attempts;
            StealResult stolen = deques_[victim]->pop_top();
            if (stolen.status == StealStatus::success) {
                result.task = std::move(stolen.task);
                result.kind = DequeueKind::stolen;
                return result;
            }

            ++result.failed_steal_attempts;
            if (stolen.status == StealStatus::abort) {
                ++result.steal_aborts;
            }
        }

        return result;
    }

    BackendMetrics metrics() const override {
        BackendMetrics metrics;
        metrics.overflow_count = overflows_.load(std::memory_order_relaxed);
        return metrics;
    }

private:
    std::size_t workers_;
    std::size_t steal_attempts_per_poll_;
    std::vector<std::unique_ptr<AbpDeque>> deques_;
    std::atomic<std::uint64_t> overflows_;
};

}  // namespace ws
