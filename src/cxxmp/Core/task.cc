#include "cxxmp/Core/task.h"

#include "cxxmp/Common/log.h"

#include <exception>
#include <memory>

namespace cxxmp::core {

// move
Task::Task(Task&& other) noexcept
    : m_fn(std::move(other.m_fn)), m_state(std::move(other.m_state)) {
    other.m_state = State::Created;
    other.m_fn    = nullptr;
}

Task::Task(const Task& other)
    : m_fn(std::make_unique< Fn >(*other.m_fn)), m_state(other.m_state) {}

// when the task could be executed, execute the task
void Task::execute() {
    if (!this->executable()) {
        return;
    }
    switch (this->m_state) {
        case State::Created: {
            m_state = State::Executing;
        }
        case State::Executing: {
            if (this->executable()) {
                try {
                    (*m_fn)();
                    m_state = State::Completed;
                } catch (...) {
                    std::rethrow_exception(std::current_exception());
                }
            }
            break;
        }
        case State::Completed: {
            break;
        }
    }
}

} // namespace cxxmp::core
