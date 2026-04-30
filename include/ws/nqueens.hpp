#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "ws/executor.hpp"
#include "ws/task.hpp"

namespace ws {

class NQueensBenchmark {
public:
    NQueensBenchmark(int n, int split_depth);

    Task initial_task();
    std::uint64_t solution_count() const;

    static std::uint64_t solve_sequential(int n);
    static std::optional<std::uint64_t> known_solution_count(int n);

private:
    Task make_node(int row, std::uint64_t columns, std::uint64_t left_diagonals,
                   std::uint64_t right_diagonals);
    void run_node(TaskContext& context, int row, std::uint64_t columns,
                  std::uint64_t left_diagonals, std::uint64_t right_diagonals);
    static std::uint64_t count_from(int n, std::uint64_t full_mask, int row,
                                    std::uint64_t columns, std::uint64_t left_diagonals,
                                    std::uint64_t right_diagonals);
    static std::uint64_t mask_for(int n);

    int n_;
    int split_depth_;
    std::uint64_t full_mask_;
    std::atomic<std::uint64_t> solutions_;
};

}  // namespace ws
