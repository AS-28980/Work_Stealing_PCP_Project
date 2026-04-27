#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "ws/abp_deque.hpp"
#include "ws/backend.hpp"

namespace ws {

class CircularArray {
public:
    explicit CircularArray(std::size_t log_capacity)
        : log_capacity_(log_capacity < 1 ? 1 : log_capacity),
          capacity_(std::size_t{1} << log_capacity_),
          slots_(capacity_) {}

    std::size_t capacity() const {
        return capacity_;
    }

    Task get(std::int64_t logical_index) const {
        return std::atomic_load_explicit(&slots_[index_for(logical_index)],
                                         std::memory_order_acquire);
    }

    void put(std::int64_t logical_index, Task task) {
        std::atomic_store_explicit(&slots_[index_for(logical_index)],
                                   std::move(task),
                                   std::memory_order_release);
    }

    std::shared_ptr<CircularArray> grow(std::int64_t top, std::int64_t bottom) const {
        // Logical indexes are unchanged across resize, so thieves can keep
        // using top while the owner publishes a larger circular array.
        auto larger = std::make_shared<CircularArray>(log_capacity_ + 1);
        for (std::int64_t i = top; i < bottom; ++i) {
            larger->put(i, get(i));
        }
        return larger;
    }

private:
    std::size_t index_for(std::int64_t logical_index) const {
        return static_cast<std::size_t>(logical_index) & (capacity_ - 1);
    }

    std::size_t log_capacity_;
    std::size_t capacity_;
    mutable std::vector<Task> slots_;
};

class ChaseLevDeque {
public:
    explicit ChaseLevDeque(std::size_t initial_log_capacity)
        : array_(std::make_shared<CircularArray>(initial_log_capacity)),
          top_(0),
          bottom_(0),
          resizes_(0) {}

    void push_bottom(Task task) {
        // Chase-Lev removes the fixed capacity limit by letting the owner grow
        // the backing array while old arrays stay alive through shared_ptrs.
        const auto bottom = bottom_.load(std::memory_order_relaxed);
        const auto top = top_.load(std::memory_order_acquire);
        auto array = std::atomic_load_explicit(&array_, std::memory_order_acquire);

        const auto size = bottom - top;
        if (size >= static_cast<std::int64_t>(array->capacity()) - 1) {
            array = array->grow(top, bottom);
            std::atomic_store_explicit(&array_, array, std::memory_order_release);
            resizes_.fetch_add(1, std::memory_order_relaxed);
        }

        array->put(bottom, std::move(task));
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(bottom + 1, std::memory_order_release);
    }

    Task pop_bottom() {
        auto bottom = bottom_.load(std::memory_order_relaxed) - 1;
        bottom_.store(bottom, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);

        const auto top = top_.load(std::memory_order_acquire);
        const auto size = bottom - top;
        if (size < 0) {
            bottom_.store(top, std::memory_order_relaxed);
            return {};
        }

        auto array = std::atomic_load_explicit(&array_, std::memory_order_acquire);
        Task task = array->get(bottom);
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

        bottom_.store(top + 1, std::memory_order_relaxed);
        return {};
    }

    StealResult pop_top() {
        const auto top = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        const auto bottom = bottom_.load(std::memory_order_acquire);
        auto array = std::atomic_load_explicit(&array_, std::memory_order_acquire);

        if (bottom <= top) {
            return {StealStatus::empty, {}};
        }

        Task task = array->get(top);
        auto expected_top = top;
        if (top_.compare_exchange_strong(expected_top, top + 1,
                                         std::memory_order_seq_cst,
                                         std::memory_order_relaxed)) {
            return {StealStatus::success, task};
        }
        return {StealStatus::abort, {}};
    }

    std::uint64_t resize_count() const {
        return resizes_.load(std::memory_order_relaxed);
    }

private:
    std::shared_ptr<CircularArray> array_;
    std::atomic<std::int64_t> top_;
    std::atomic<std::int64_t> bottom_;
    std::atomic<std::uint64_t> resizes_;
};

class ChaseLevBackend final : public ITaskBackend {
public:
    ChaseLevBackend(std::size_t workers,
                    std::size_t initial_log_capacity,
                    std::size_t steal_attempts_per_poll)
        : workers_(workers == 0 ? 1 : workers),
          steal_attempts_per_poll_(steal_attempts_per_poll == 0 ? workers_ : steal_attempts_per_poll) {
        deques_.reserve(workers_);
        for (std::size_t i = 0; i < workers_; ++i) {
            deques_.push_back(std::make_unique<ChaseLevDeque>(initial_log_capacity));
        }
    }

    std::string name() const override {
        return "chaselev";
    }

    std::size_t worker_count() const override {
        return workers_;
    }

    EnqueueStatus enqueue(std::size_t worker_id, Task task) override {
        deques_[worker_id % workers_]->push_bottom(std::move(task));
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
        for (const auto& deque : deques_) {
            metrics.resize_count += deque->resize_count();
        }
        return metrics;
    }

private:
    std::size_t workers_;
    std::size_t steal_attempts_per_poll_;
    std::vector<std::unique_ptr<ChaseLevDeque>> deques_;
};

}  // namespace ws
