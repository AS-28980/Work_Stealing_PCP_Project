#include "ws/nqueens.hpp"

#include <stdexcept>
#include <vector>

namespace ws {

NQueensBenchmark::NQueensBenchmark(int n, int split_depth)
    : n_(n),
      split_depth_(split_depth < 0 ? 0 : split_depth),
      full_mask_(mask_for(n)),
      solutions_(0) {
    if (n_ < 1 || n_ > 63) {
        throw std::invalid_argument("N must be in the range [1, 63]");
    }
    if (split_depth_ > n_) {
        split_depth_ = n_;
    }
}

Task NQueensBenchmark::initial_task() {
    return make_node(0, 0, 0, 0);
}

std::uint64_t NQueensBenchmark::solution_count() const {
    return solutions_.load(std::memory_order_relaxed);
}

std::uint64_t NQueensBenchmark::solve_sequential(int n) {
    if (n < 1 || n > 63) {
        throw std::invalid_argument("N must be in the range [1, 63]");
    }
    return count_from(n, mask_for(n), 0, 0, 0, 0);
}

std::optional<std::uint64_t> NQueensBenchmark::known_solution_count(int n) {
    static const std::vector<std::uint64_t> counts = {
        1ULL,        // 0, unused by the CLI
        1ULL,
        0ULL,
        0ULL,
        2ULL,
        10ULL,
        4ULL,
        40ULL,
        92ULL,
        352ULL,
        724ULL,
        2680ULL,
        14200ULL,
        73712ULL,
        365596ULL,
        2279184ULL,
        14772512ULL,
        95815104ULL,
        666090624ULL
    };

    if (n < 0 || static_cast<std::size_t>(n) >= counts.size()) {
        return std::nullopt;
    }
    return counts[static_cast<std::size_t>(n)];
}

Task NQueensBenchmark::make_node(int row,
                                 std::uint64_t columns,
                                 std::uint64_t left_diagonals,
                                 std::uint64_t right_diagonals) {
    return make_task([this, row, columns, left_diagonals, right_diagonals](TaskContext& context) {
        run_node(context, row, columns, left_diagonals, right_diagonals);
    });
}

void NQueensBenchmark::run_node(TaskContext& context,
                                int row,
                                std::uint64_t columns,
                                std::uint64_t left_diagonals,
                                std::uint64_t right_diagonals) {
    if (row == n_) {
        solutions_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    std::uint64_t local_solutions = 0;
    std::uint64_t available = full_mask_ & ~(columns | left_diagonals | right_diagonals);
    while (available != 0) {
        const std::uint64_t bit = available & (~available + 1);
        available ^= bit;

        const std::uint64_t next_columns = columns | bit;
        const std::uint64_t next_left = (left_diagonals | bit) << 1;
        const std::uint64_t next_right = (right_diagonals | bit) >> 1;

        if (row < split_depth_) {
            context.spawn(make_node(row + 1, next_columns, next_left, next_right));
        } else {
            local_solutions += count_from(n_,
                                          full_mask_,
                                          row + 1,
                                          next_columns,
                                          next_left,
                                          next_right);
        }
    }

    if (local_solutions != 0) {
        solutions_.fetch_add(local_solutions, std::memory_order_relaxed);
    }
}

std::uint64_t NQueensBenchmark::count_from(int n,
                                           std::uint64_t full_mask,
                                           int row,
                                           std::uint64_t columns,
                                           std::uint64_t left_diagonals,
                                           std::uint64_t right_diagonals) {
    if (row == n) {
        return 1;
    }

    std::uint64_t count = 0;
    std::uint64_t available = full_mask & ~(columns | left_diagonals | right_diagonals);
    while (available != 0) {
        const std::uint64_t bit = available & (~available + 1);
        available ^= bit;
        count += count_from(n,
                            full_mask,
                            row + 1,
                            columns | bit,
                            (left_diagonals | bit) << 1,
                            (right_diagonals | bit) >> 1);
    }
    return count;
}

std::uint64_t NQueensBenchmark::mask_for(int n) {
    if (n == 64) {
        return ~0ULL;
    }
    return (1ULL << n) - 1ULL;
}

}  // namespace ws
