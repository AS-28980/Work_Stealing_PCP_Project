#pragma once

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ws/backend.hpp"
#include "ws/metrics.hpp"
#include "ws/task.hpp"

namespace ws {

struct RunDescriptor {
    std::string scheduler;
    int n = 0;
    int split_depth = 0;
    double sequential_seconds = 0.0;
};

class Executor;

class TaskContext {
public:
    TaskContext(Executor& executor, std::size_t worker_id)
        : executor_(executor), worker_id_(worker_id) {}

    std::size_t worker_id() const {
        return worker_id_;
    }

    void spawn(Task task);

    template <typename Fn>
    void spawn_fn(Fn&& fn) {
        spawn(make_task(std::forward<Fn>(fn)));
    }

private:
    Executor& executor_;
    std::size_t worker_id_;
};

class Executor {
public:
    explicit Executor(std::unique_ptr<ITaskBackend> backend, std::uint64_t seed = 1)
        : backend_(std::move(backend)),
          seed_(seed) {
        if (!backend_) {
            throw std::invalid_argument("backend must not be null");
        }
        stats_.resize(backend_->worker_count());
    }

    RunMetrics run(Task initial_task, const RunDescriptor& descriptor) {
        reset_counters();

        outstanding_.store(1, std::memory_order_release);
        tasks_created_.store(1, std::memory_order_relaxed);
        tasks_scheduled_.store(1, std::memory_order_relaxed);
        const EnqueueStatus status = backend_->enqueue(0, std::move(initial_task));
        if (status != EnqueueStatus::ok) {
            throw std::runtime_error("initial task could not be scheduled");
        }

        const auto start = Clock::now();
        std::vector<std::thread> threads;
        threads.reserve(stats_.size());
        for (std::size_t id = 0; id < stats_.size(); ++id) {
            threads.emplace_back([this, id] {
                worker_loop(id);
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        const auto finish = Clock::now();

        if (captured_exception_) {
            std::rethrow_exception(captured_exception_);
        }

        RunMetrics metrics = collect_metrics();
        metrics.scheduler = descriptor.scheduler.empty() ? backend_->name() : descriptor.scheduler;
        metrics.n = descriptor.n;
        metrics.split_depth = descriptor.split_depth;
        metrics.sequential_seconds = descriptor.sequential_seconds;
        metrics.elapsed_seconds = seconds_between(start, finish);
        metrics.speedup = metrics.sequential_seconds > 0.0
                              ? metrics.sequential_seconds / metrics.elapsed_seconds
                              : 0.0;
        metrics.tasks_per_second = metrics.elapsed_seconds > 0.0
                                       ? static_cast<double>(metrics.tasks_completed) / metrics.elapsed_seconds
                                       : 0.0;
        return metrics;
    }

    void spawn(std::size_t worker_id, Task task) {
        if (!task) {
            return;
        }

        tasks_created_.fetch_add(1, std::memory_order_relaxed);
        outstanding_.fetch_add(1, std::memory_order_release);

        const EnqueueStatus status = backend_->enqueue(worker_id, task);
        if (status == EnqueueStatus::ok) {
            tasks_scheduled_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        outstanding_.fetch_sub(1, std::memory_order_acq_rel);
        inline_overflow_tasks_.fetch_add(1, std::memory_order_relaxed);
        stats_[worker_id].inline_tasks_executed += 1;

        TaskContext nested_context(*this, worker_id);
        (*task)(nested_context);
    }

private:
    using Clock = std::chrono::steady_clock;

    void reset_counters() {
        stop_.store(false, std::memory_order_release);
        outstanding_.store(0, std::memory_order_release);
        tasks_created_.store(0, std::memory_order_relaxed);
        tasks_scheduled_.store(0, std::memory_order_relaxed);
        inline_overflow_tasks_.store(0, std::memory_order_relaxed);
        captured_exception_ = nullptr;
        for (auto& stat : stats_) {
            stat = WorkerStats{};
        }
    }

    void worker_loop(std::size_t worker_id) {
        std::mt19937_64 rng(seed_ + 0x9e3779b97f4a7c15ULL * (worker_id + 1));

        while (!stop_.load(std::memory_order_acquire)) {
            DequeueResult result = backend_->dequeue(worker_id, rng);
            WorkerStats& stat = stats_[worker_id];
            stat.steal_attempts += result.steal_attempts;
            stat.failed_steal_attempts += result.failed_steal_attempts;
            stat.steal_aborts += result.steal_aborts;

            if (result.task) {
                execute_task(worker_id, result.kind, std::move(result.task));
                continue;
            }

            if (outstanding_.load(std::memory_order_acquire) == 0) {
                break;
            }

            ++stat.empty_polls;
            const auto idle_start = Clock::now();
            std::this_thread::yield();
            const auto idle_finish = Clock::now();
            stat.idle_seconds += seconds_between(idle_start, idle_finish);
        }
    }

    void execute_task(std::size_t worker_id, DequeueKind kind, Task task) {
        WorkerStats& stat = stats_[worker_id];
        switch (kind) {
            case DequeueKind::global:
                ++stat.global_pops;
                break;
            case DequeueKind::local:
                ++stat.local_pops;
                break;
            case DequeueKind::stolen:
                ++stat.stolen_pops;
                break;
            case DequeueKind::none:
                break;
        }

        bool failed = false;
        try {
            TaskContext context(*this, worker_id);
            (*task)(context);
        } catch (...) {
            remember_exception(std::current_exception());
            stop_.store(true, std::memory_order_release);
            failed = true;
        }

        ++stat.scheduled_tasks_executed;
        if (outstanding_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            stop_.store(true, std::memory_order_release);
        }

        if (failed) {
            return;
        }
    }

    void remember_exception(std::exception_ptr exception) {
        std::lock_guard<std::mutex> lock(exception_mutex_);
        if (!captured_exception_) {
            captured_exception_ = exception;
        }
    }

    RunMetrics collect_metrics() const {
        RunMetrics metrics;
        metrics.workers = stats_.size();
        metrics.tasks_created = tasks_created_.load(std::memory_order_relaxed);
        metrics.tasks_scheduled = tasks_scheduled_.load(std::memory_order_relaxed);
        metrics.inline_overflow_tasks = inline_overflow_tasks_.load(std::memory_order_relaxed);

        double sum = 0.0;
        double sum_squares = 0.0;
        for (const WorkerStats& stat : stats_) {
            const std::uint64_t completed = stat.scheduled_tasks_executed + stat.inline_tasks_executed;
            metrics.tasks_completed += completed;
            metrics.scheduled_tasks_completed += stat.scheduled_tasks_executed;
            metrics.successful_steals += stat.stolen_pops;
            metrics.failed_steal_attempts += stat.failed_steal_attempts;
            metrics.steal_attempts += stat.steal_attempts;
            metrics.steal_aborts += stat.steal_aborts;
            metrics.total_idle_seconds += stat.idle_seconds;
            metrics.min_tasks_per_worker = std::min(metrics.min_tasks_per_worker, completed);
            metrics.max_tasks_per_worker = std::max(metrics.max_tasks_per_worker, completed);
            sum += static_cast<double>(completed);
            sum_squares += static_cast<double>(completed) * static_cast<double>(completed);
        }

        if (stats_.empty()) {
            metrics.min_tasks_per_worker = 0;
        } else {
            metrics.mean_tasks_per_worker = sum / static_cast<double>(stats_.size());
            const double mean_square = sum_squares / static_cast<double>(stats_.size());
            const double variance = std::max(0.0, mean_square - metrics.mean_tasks_per_worker * metrics.mean_tasks_per_worker);
            metrics.stddev_tasks_per_worker = std::sqrt(variance);
        }

        const BackendMetrics backend_metrics = backend_->metrics();
        metrics.abp_overflows = backend_metrics.overflow_count;
        metrics.chase_lev_resizes = backend_metrics.resize_count;
        return metrics;
    }

    static double seconds_between(Clock::time_point start, Clock::time_point finish) {
        return std::chrono::duration<double>(finish - start).count();
    }

    std::unique_ptr<ITaskBackend> backend_;
    std::uint64_t seed_;
    std::vector<WorkerStats> stats_;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> outstanding_{0};
    std::atomic<std::uint64_t> tasks_created_{0};
    std::atomic<std::uint64_t> tasks_scheduled_{0};
    std::atomic<std::uint64_t> inline_overflow_tasks_{0};
    mutable std::mutex exception_mutex_;
    std::exception_ptr captured_exception_;
};

inline void TaskContext::spawn(Task task) {
    executor_.spawn(worker_id_, std::move(task));
}

}  // namespace ws
