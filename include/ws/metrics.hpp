#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include "ws/task.hpp"

namespace ws {

enum class EnqueueStatus {
    ok,
    overflow
};

enum class DequeueKind {
    none,
    global,
    local,
    stolen
};

struct DequeueResult {
    Task task;
    DequeueKind kind = DequeueKind::none;
    std::uint64_t steal_attempts = 0;
    std::uint64_t failed_steal_attempts = 0;
    std::uint64_t steal_aborts = 0;
};

struct BackendMetrics {
    std::uint64_t overflow_count = 0;
    std::uint64_t resize_count = 0;
};

struct WorkerStats {
    std::uint64_t scheduled_tasks_executed = 0;
    std::uint64_t inline_tasks_executed = 0;
    std::uint64_t global_pops = 0;
    std::uint64_t local_pops = 0;
    std::uint64_t stolen_pops = 0;
    std::uint64_t steal_attempts = 0;
    std::uint64_t failed_steal_attempts = 0;
    std::uint64_t steal_aborts = 0;
    std::uint64_t empty_polls = 0;
    double idle_seconds = 0.0;
};

struct RunMetrics {
    std::string scheduler;
    int n = 0;
    int split_depth = 0;
    std::uint64_t solutions = 0;
    std::uint64_t known_solutions = 0;
    bool has_known_solutions = false;
    bool correct = false;
    std::size_t workers = 0;

    double elapsed_seconds = 0.0;
    double sequential_seconds = 0.0;
    double speedup = 0.0;

    std::uint64_t tasks_created = 0;
    std::uint64_t tasks_scheduled = 0;
    std::uint64_t tasks_completed = 0;
    std::uint64_t scheduled_tasks_completed = 0;
    std::uint64_t inline_overflow_tasks = 0;
    double tasks_per_second = 0.0;

    std::uint64_t successful_steals = 0;
    std::uint64_t failed_steal_attempts = 0;
    std::uint64_t steal_attempts = 0;
    std::uint64_t steal_aborts = 0;
    double total_idle_seconds = 0.0;

    std::uint64_t min_tasks_per_worker = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t max_tasks_per_worker = 0;
    double mean_tasks_per_worker = 0.0;
    double stddev_tasks_per_worker = 0.0;

    std::uint64_t abp_overflows = 0;
    std::uint64_t chase_lev_resizes = 0;
};

}  // namespace ws
