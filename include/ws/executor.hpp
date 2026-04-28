#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
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
    TaskContext(Executor& executor, std::size_t worker_id);

    std::size_t worker_id() const;
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
    explicit Executor(std::unique_ptr<ITaskBackend> backend, std::uint64_t seed = 1);

    RunMetrics run(Task initial_task, const RunDescriptor& descriptor);
    void spawn(std::size_t worker_id, Task task);

private:
    using Clock = std::chrono::steady_clock;

    void reset_counters();
    void worker_loop(std::size_t worker_id);
    void execute_task(std::size_t worker_id, DequeueKind kind, Task task);
    void remember_exception(std::exception_ptr exception);
    RunMetrics collect_metrics() const;
    static double seconds_between(Clock::time_point start, Clock::time_point finish);

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

}  // namespace ws
