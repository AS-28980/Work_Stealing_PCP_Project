#include "ws/global_queue.hpp"

#include <utility>

namespace ws {

GlobalQueueBackend::GlobalQueueBackend(std::size_t workers)
    : workers_(workers == 0 ? 1 : workers) {}

std::string GlobalQueueBackend::name() const {
    return "global";
}

std::size_t GlobalQueueBackend::worker_count() const {
    return workers_;
}

EnqueueStatus GlobalQueueBackend::enqueue(std::size_t, Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(task));
    return EnqueueStatus::ok;
}

DequeueResult GlobalQueueBackend::dequeue(std::size_t, std::mt19937_64&) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return {};
    }

    DequeueResult result;
    result.task = std::move(queue_.front());
    result.kind = DequeueKind::global;
    queue_.pop_front();
    return result;
}

BackendMetrics GlobalQueueBackend::metrics() const {
    return {};
}

}  // namespace ws
