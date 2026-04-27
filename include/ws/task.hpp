#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace ws {

class TaskContext;

using TaskFunction = std::function<void(TaskContext&)>;
using Task = std::shared_ptr<const TaskFunction>;

template <typename Fn>
Task make_task(Fn&& fn) {
    return std::make_shared<TaskFunction>(std::forward<Fn>(fn));
}

}  // namespace ws
