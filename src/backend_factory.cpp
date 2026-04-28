#include "ws/backend_factory.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "ws/abp_deque.hpp"
#include "ws/chase_lev_deque.hpp"
#include "ws/global_queue.hpp"

namespace ws {

std::string normalize_scheduler_name(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    name.erase(std::remove(name.begin(), name.end(), '-'), name.end());
    name.erase(std::remove(name.begin(), name.end(), '_'), name.end());
    return name;
}

std::unique_ptr<ITaskBackend> make_backend(const std::string& scheduler,
                                           const BackendOptions& options) {
    const std::string normalized = normalize_scheduler_name(scheduler);
    if (normalized == "global" || normalized == "globalqueue" || normalized == "baseline") {
        return std::make_unique<GlobalQueueBackend>(options.workers);
    }
    if (normalized == "abp" || normalized == "bounded") {
        return std::make_unique<AbpBackend>(options.workers,
                                            options.abp_capacity,
                                            options.steal_attempts_per_poll);
    }
    if (normalized == "chaselev" || normalized == "unbounded") {
        return std::make_unique<ChaseLevBackend>(options.workers,
                                                 options.chase_lev_initial_log_capacity,
                                                 options.steal_attempts_per_poll);
    }
    throw std::invalid_argument("unknown scheduler: " + scheduler);
}

}  // namespace ws
