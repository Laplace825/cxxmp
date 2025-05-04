#include "cxxmp/Core/task.h"

#include <memory>

#include "cxxmp/Common/log.h"

namespace cxxmp::core {

// move
Task::Task(Task&& other) noexcept : m_fn(std::move(other.m_fn)) {
    other.m_fn = nullptr;
}

Task::Task(const Task& other) : m_fn(std::make_unique< Fn >(*other.m_fn)) {}

// when the task could be executed, execute the task
void Task::execute() {
    if (!this->executable()) {
        return;
    }

    try {
        (*m_fn)();
    } catch (...) {
        std::rethrow_exception(std::current_exception());
    }
}

} // namespace cxxmp::core
