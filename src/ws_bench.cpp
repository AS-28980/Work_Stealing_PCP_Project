#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ws/backend_factory.hpp"
#include "ws/executor.hpp"
#include "ws/nqueens.hpp"

namespace {

struct CliOptions {
    std::string scheduler = "chaselev";
    int n = 12;
    int split_depth = 5;
    std::size_t workers = std::max(1u, std::thread::hardware_concurrency());
    std::size_t abp_capacity = 4096;
    std::size_t chase_log_capacity = 4;
    std::size_t steal_attempts = 0;
    std::uint64_t seed = 1;
    bool csv = false;
    bool skip_sequential = false;
};

void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --scheduler global|abp|chaselev|all\n"
        << "  --n N\n"
        << "  --workers P\n"
        << "  --split-depth D\n"
        << "  --abp-capacity C\n"
        << "  --chase-log-capacity L\n"
        << "  --steal-attempts K\n"
        << "  --seed S\n"
        << "  --csv\n"
        << "  --skip-sequential\n"
        << "  --help\n";
}

std::string require_value(int& i, int argc, char** argv) {
    if (i + 1 >= argc) {
        throw std::invalid_argument(std::string("missing value for ") + argv[i]);
    }
    return argv[++i];
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--scheduler") {
            options.scheduler = require_value(i, argc, argv);
        } else if (arg == "--n") {
            options.n = std::stoi(require_value(i, argc, argv));
        } else if (arg == "--workers") {
            options.workers = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--split-depth") {
            options.split_depth = std::stoi(require_value(i, argc, argv));
        } else if (arg == "--abp-capacity") {
            options.abp_capacity = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--chase-log-capacity") {
            options.chase_log_capacity = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--steal-attempts") {
            options.steal_attempts = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--seed") {
            options.seed = static_cast<std::uint64_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--csv") {
            options.csv = true;
        } else if (arg == "--skip-sequential") {
            options.skip_sequential = true;
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    if (options.workers == 0) {
        options.workers = 1;
    }
    return options;
}

std::pair<std::uint64_t, double> run_sequential(int n) {
    const auto start = std::chrono::steady_clock::now();
    const std::uint64_t solutions = ws::NQueensBenchmark::solve_sequential(n);
    const auto finish = std::chrono::steady_clock::now();
    return {solutions, std::chrono::duration<double>(finish - start).count()};
}

ws::RunMetrics run_scheduler(const CliOptions& options, const std::string& scheduler,
                             double sequential_seconds) {
    ws::BackendOptions backend_options;
    backend_options.workers = options.workers;
    backend_options.abp_capacity = options.abp_capacity;
    backend_options.chase_lev_initial_log_capacity = options.chase_log_capacity;
    backend_options.steal_attempts_per_poll = options.steal_attempts;

    ws::NQueensBenchmark benchmark(options.n, options.split_depth);
    ws::Executor executor(ws::make_backend(scheduler, backend_options), options.seed);
    ws::RunDescriptor descriptor;
    descriptor.scheduler = scheduler;
    descriptor.n = options.n;
    descriptor.split_depth = options.split_depth;
    descriptor.sequential_seconds = sequential_seconds;

    ws::RunMetrics metrics = executor.run(benchmark.initial_task(), descriptor);
    metrics.solutions = benchmark.solution_count();
    if (auto known = ws::NQueensBenchmark::known_solution_count(options.n)) {
        metrics.has_known_solutions = true;
        metrics.known_solutions = *known;
        metrics.correct = metrics.solutions == *known;
    }
    return metrics;
}

void print_csv_header() {
    std::cout
        << "scheduler,n,workers,split_depth,solutions,known_solutions,correct,"
        << "elapsed_seconds,sequential_seconds,speedup,tasks_created,tasks_scheduled,"
        << "tasks_completed,tasks_per_second,successful_steals,failed_steal_attempts,"
        << "steal_attempts,steal_aborts,idle_seconds,load_min,load_max,load_mean,"
        << "load_stddev,abp_overflows,chase_lev_resizes,inline_overflow_tasks\n";
}

void print_csv_row(const ws::RunMetrics& m) {
    std::cout << std::fixed << std::setprecision(6)
              << m.scheduler << ','
              << m.n << ','
              << m.workers << ','
              << m.split_depth << ','
              << m.solutions << ',';
    if (m.has_known_solutions) {
        std::cout << m.known_solutions << ',' << (m.correct ? 1 : 0) << ',';
    } else {
        std::cout << ",,";
    }
    std::cout << m.elapsed_seconds << ','
              << m.sequential_seconds << ','
              << m.speedup << ','
              << m.tasks_created << ','
              << m.tasks_scheduled << ','
              << m.tasks_completed << ','
              << m.tasks_per_second << ','
              << m.successful_steals << ','
              << m.failed_steal_attempts << ','
              << m.steal_attempts << ','
              << m.steal_aborts << ','
              << m.total_idle_seconds << ','
              << m.min_tasks_per_worker << ','
              << m.max_tasks_per_worker << ','
              << m.mean_tasks_per_worker << ','
              << m.stddev_tasks_per_worker << ','
              << m.abp_overflows << ','
              << m.chase_lev_resizes << ','
              << m.inline_overflow_tasks
              << '\n';
}

void print_human(const ws::RunMetrics& m) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Scheduler: " << m.scheduler << '\n'
              << "N: " << m.n << ", workers: " << m.workers
              << ", split depth: " << m.split_depth << '\n'
              << "Solutions: " << m.solutions;
    if (m.has_known_solutions) {
        std::cout << " (known " << m.known_solutions << ", "
                  << (m.correct ? "correct" : "incorrect") << ')';
    }
    std::cout << '\n'
              << "Elapsed: " << m.elapsed_seconds << " s\n";
    if (m.sequential_seconds > 0.0) {
        std::cout << "Sequential: " << m.sequential_seconds << " s, speedup: "
                  << m.speedup << "x\n";
    }
    std::cout << "Tasks: created " << m.tasks_created
              << ", scheduled " << m.tasks_scheduled
              << ", completed " << m.tasks_completed
              << ", throughput " << m.tasks_per_second << " tasks/s\n"
              << "Steals: successful " << m.successful_steals
              << ", failed " << m.failed_steal_attempts
              << ", attempts " << m.steal_attempts
              << ", aborts " << m.steal_aborts << '\n'
              << "Idle time total: " << m.total_idle_seconds << " s\n"
              << "Load per worker: min " << m.min_tasks_per_worker
              << ", max " << m.max_tasks_per_worker
              << ", mean " << m.mean_tasks_per_worker
              << ", stddev " << m.stddev_tasks_per_worker << '\n'
              << "ABP overflows: " << m.abp_overflows
              << ", Chase-Lev resizes: " << m.chase_lev_resizes
              << ", inline overflow tasks: " << m.inline_overflow_tasks << "\n";
}

std::vector<std::string> scheduler_list(const std::string& scheduler) {
    if (ws::normalize_scheduler_name(scheduler) == "all") {
        return {"global", "abp", "chaselev"};
    }
    return {scheduler};
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_args(argc, argv);

        double sequential_seconds = 0.0;
        if (!options.skip_sequential) {
            const auto [solutions, seconds] = run_sequential(options.n);
            sequential_seconds = seconds;
            if (const auto known = ws::NQueensBenchmark::known_solution_count(options.n);
                known && solutions != *known) {
                std::cerr << "Sequential solver produced " << solutions
                          << ", expected " << *known << '\n';
                return 2;
            }
        }

        if (options.csv) {
            print_csv_header();
        }

        for (const std::string& scheduler : scheduler_list(options.scheduler)) {
            ws::RunMetrics metrics = run_scheduler(options, scheduler, sequential_seconds);
            if (options.csv) {
                print_csv_row(metrics);
            } else {
                print_human(metrics);
                std::cout << '\n';
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
