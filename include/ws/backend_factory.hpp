#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "ws/backend.hpp"

namespace ws {

struct BackendOptions {
    std::size_t workers = 1;
    std::size_t abp_capacity = 4096;
    std::size_t chase_lev_initial_log_capacity = 4;
    std::size_t steal_attempts_per_poll = 0;
};

std::string normalize_scheduler_name(std::string name);

std::unique_ptr<ITaskBackend> make_backend(const std::string& scheduler, const BackendOptions& options);

}  // namespace ws
