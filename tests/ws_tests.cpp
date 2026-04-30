#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ws/backend_factory.hpp"
#include "ws/executor.hpp"
#include "ws/nqueens.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ws::RunMetrics run_case(const std::string& scheduler, int n, int split_depth,
                        std::size_t workers,
                        std::size_t abp_capacity = 64,
                        std::size_t chase_log_capacity = 2) {
    ws::BackendOptions backend_options;
    backend_options.workers = workers;
    backend_options.abp_capacity = abp_capacity;
    backend_options.chase_lev_initial_log_capacity = chase_log_capacity;

    ws::NQueensBenchmark benchmark(n, split_depth);
    ws::Executor executor(ws::make_backend(scheduler, backend_options), 7);
    ws::RunDescriptor descriptor;
    descriptor.scheduler = scheduler;
    descriptor.n = n;
    descriptor.split_depth = split_depth;

    ws::RunMetrics metrics = executor.run(benchmark.initial_task(), descriptor);
    metrics.solutions = benchmark.solution_count();
    return metrics;
}

void test_sequential_counts() {
    for (int n = 1; n <= 12; ++n) {
        const auto known = ws::NQueensBenchmark::known_solution_count(n);
        require(known.has_value(), "missing known count");
        const std::uint64_t got = ws::NQueensBenchmark::solve_sequential(n);
        require(got == *known, "sequential N-Queens count mismatch for n=" + std::to_string(n));
    }
}

void test_scheduler_counts() {
    const std::string schedulers[] = {"global", "abp", "chaselev"};
    for (const std::string& scheduler : schedulers) {
        for (int n = 1; n <= 10; ++n) {
            const auto known = ws::NQueensBenchmark::known_solution_count(n);
            ws::RunMetrics metrics = run_case(scheduler, n, std::min(n, 4), 4);
            require(metrics.solutions == *known,
                    scheduler + " count mismatch for n=" + std::to_string(n));
            require(metrics.tasks_completed == metrics.tasks_created,
                    scheduler + " did not complete all created tasks for n=" + std::to_string(n));
        }
    }
}

void test_abp_overflow_fallback() {
    ws::RunMetrics metrics = run_case("abp", 9, 6, 4, 2, 2);
    const auto known = ws::NQueensBenchmark::known_solution_count(9);
    require(metrics.solutions == *known, "ABP overflow fallback changed the answer");
    require(metrics.abp_overflows > 0, "ABP overflow test did not overflow");
    require(metrics.inline_overflow_tasks > 0, "ABP overflow test did not execute inline fallback");
}

void test_chase_lev_resize() {
    ws::RunMetrics metrics = run_case("chaselev", 9, 6, 4, 64, 1);
    const auto known = ws::NQueensBenchmark::known_solution_count(9);
    require(metrics.solutions == *known, "Chase-Lev resize changed the answer");
    require(metrics.chase_lev_resizes > 0, "Chase-Lev resize test did not resize");
}

}  // namespace

int main() {
    try {
        test_sequential_counts();
        test_scheduler_counts();
        test_abp_overflow_fallback();
        test_chase_lev_resize();
        std::cout << "all tests passed\n";
    } catch (const std::exception& ex) {
        std::cerr << "test failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
