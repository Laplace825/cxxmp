#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The global task queue is a shared resource that is used to manage the
 * execution of tasks.
 *
 * Unlike local task queues, the global task queue is shared by all threads
 */

#include "cxxmp/Core/task.h"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace cxxmp {

// Global task queue
class GlobalTaskQueue {
  private:
    // Mutex for thread safety
    ::std::mutex m_mx;

    // Queue of tasks, could recive user submitted tasks
    ::std::deque< Task > m_queue;

    // Condition variable for notifying threads (When user submitted tasks,
    // activate the global queue)
    ::std::condition_variable m_cv;

  public:
    // Constructor
    GlobalTaskQueue() {
        // Initialize the queue
        m_queue.clear();
    };

    ~GlobalTaskQueue();

    // submit a task to run
    bool submit(const Task&);
};

} // namespace cxxmp
