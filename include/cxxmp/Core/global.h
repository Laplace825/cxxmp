#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The global task queue is a shared resource that is used to
 * store the tasks that exceed the local task queue capacity. The global task
 * queue is allowed to dynamically grow and shrink based on the number of tasks
 * submitted.
 *
 * Unlike local task queues, the global task queue should be shared by all
 * threads And the global task queue won't execute any tasks directly.
 */

#include "cxxmp/Common/typing.h"
#include "cxxmp/Core/task.h"

#include <deque>
#include <mutex>
#include <type_traits>

namespace cxxmp::core {

// Global task queue
class GlobalTaskQueue {
  public:
    // Constructor
    GlobalTaskQueue() { m_queue.clear(); };

    GlobalTaskQueue(GlobalTaskQueue&& other) noexcept
        : m_queue(std::move(other.m_queue)) {}

    ~GlobalTaskQueue() = default;

    GlobalTaskQueue(const GlobalTaskQueue&)            = delete;
    GlobalTaskQueue& operator=(const GlobalTaskQueue&) = delete;

    // store a task to the back
    template < typename TaskType >
        requires std::is_same_v< TaskType, Task > ||
                 std::is_same_v< TaskPtr, Task >
    void store(TaskType&& task) {
        std::lock_guard< std::mutex > lock(m_mx);
        if constexpr (::std::is_same_v< ::std::decay_t< TaskType >, Task >) {
            m_queue.push_back(
              ::std::make_shared< Task >(std::forward< TaskType >(task)));
        }
        else if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                             TaskPtr >)
        {
            m_queue.push_back(std::move(task));
        }
    }

    RcTaskPtr pop() {
        std::lock_guard< std::mutex > lock(m_mx);
        if (!m_queue.empty()) {
            auto task = std::move(m_queue.front());
            m_queue.pop_front();
            return task;
        }
        return nullptr;
    }

  private:
    ::std::mutex m_mx;

    ::std::deque< RcTaskPtr > m_queue;
};

} // namespace cxxmp::core
