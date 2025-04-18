#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The local task queue is a thread-local queue that is used to manage the
 * local tasks for a thread.
 *
 * We create threads equals to the number of the logical cores
 * Each thread has its own task queue
 * but the task queue could be shared with other threads, for any other could
 * steal tasks from others local queue
 */

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/typing.h"
#include "cxxmp/Core/task.h"
#include "cxxmp/Utily/getsys.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <string_view>
#include <thread>
#include <type_traits>

namespace cxxmp {

/**
 * @brief: the queue is a thread-local queue that is used to manage the
 * local tasks for a thread.
 *
 * This is a state machine
 */
class LocalTaskQueue {
  public:
    enum class State {
        Idle, // the start state
        Busy,
        Paused,
        SubTaskErrorHappend,
        ToCompleteAll, // when in this state, will do all tasks then idle
        Shutdown,
    };

    static constexpr std::string_view state2String(State state) {
#define Fn(stateName)      \
    case State::stateName: \
        return "LocalTaskQueue::State::" #stateName;

        switch (state) {
            Fn(Idle);
            Fn(Busy);
            Fn(Paused);
            Fn(SubTaskErrorHappend);
            Fn(ToCompleteAll);
            Fn(Shutdown);
            default:
                return "Unknown";
        }
#undef Fn
    }

  private:
    // will handle the error
    void handleError();

    void stateTransfer2(State state);

    void waitForTask();

  public:
    static constexpr size_t getCapacity() { return 32 * sys::getSysCPUs(); }

    LocalTaskQueue(const LocalTaskQueue&)            = delete;
    LocalTaskQueue& operator=(const LocalTaskQueue&) = delete;

    LocalTaskQueue(LocalTaskQueue&& other) noexcept
        : m_capacity(other.m_capacity), m_hid(other.m_hid), m_tid(other.m_tid),
          m_queue(std::move(other.m_queue)), m_state(other.m_state.load()),
          m_exception_ptr(other.m_exception_ptr) {
        other.m_state = State::Idle;
        if (other.m_worker.joinable()) {
            other.shutdown();
        }
        run();
    }

    LocalTaskQueue& operator=(LocalTaskQueue&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            shutdown();

            // Move resources from other
            m_capacity = other.m_capacity;
            m_hid      = other.m_hid;
            m_tid      = other.m_tid;
            m_queue    = std::move(other.m_queue);
            m_state.store(other.m_state.load());
            m_exception_ptr = std::move(other.m_exception_ptr);

            // Reset other's state
            other.m_state = State::Shutdown;

            // Handle the worker thread
            if (other.m_worker.joinable()) {
                other.shutdown();
            }

            // Start a new worker thread for this instance
            run();
        }
        return *this;
    }

    explicit LocalTaskQueue(size_t capacity) {
        log::debug("LocalTaskQueue created");
        // Initialize the queue
        m_state         = State::Idle;
        m_exception_ptr = nullptr;
        m_capacity      = capacity;
    }

    LocalTaskQueue() {
        log::debug("LocalTaskQueue created");
        // Initialize the queue
        m_state         = State::Idle;
        m_exception_ptr = nullptr;
        m_capacity      = getCapacity();
    }

    ~LocalTaskQueue() {
        log::debug("LocalTaskQueue destroyed");
        this->shutdown();
    }

    // submit a task to run, always push to the back
    template < typename TaskType >
        requires ::std::is_same_v< TaskType, Task > ||
                 ::std::is_same_v< TaskType, TaskPtr >
    bool submit(TaskType&& task) {
        if (m_state == State::Shutdown) {
            return false;
        }
        if (m_queue.size() >= m_capacity) {
            return false;
        }
        if constexpr (::std::is_same_v< ::std::decay_t< TaskType >, Task >) {
            std::lock_guard< std::mutex > lock(m_mx);
            m_queue.push_back(
              ::std::make_shared< Task >(std::forward< TaskType >(task)));
        }
        else if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                             TaskPtr >)
        {

            // In submit method
            log::debug("Submitting task: ptr={:p}, valid={}", (void*)task.get(),
              task != nullptr);
            std::lock_guard< std::mutex > lock(m_mx);
            m_queue.push_back(std::move(task));
            log::debug("LocalTaskQueue Tasks: {}", m_queue.size());
        }
        m_cv.notify_one();
        // After pushing to queue
        log::debug("Queue now has {} tasks, front valid={}", m_queue.size(),
          m_queue.front() != nullptr);
        return true;
    }

    constexpr size_t size() const noexcept { return m_queue.size(); }

    constexpr size_t capacity() const noexcept { return m_capacity; }

    // get the `thread` id
    ::std::thread::id getTid() const noexcept { return m_tid; }

    // get the hashed `thread` id
    size_t getHid() const noexcept { return m_hid; }

    // just tell is there any jobs still in the queue
    // this won't change anything
    bool hasTask() const noexcept;

    /**
     * @brief: pause the queue running.
     * If is already in running one task, complete it and then pause.
     * If there are no tasks in the queue, do nothing.
     */
    void pause() noexcept;

    // If is already in paused state, unpause it and continue running.
    void unpause() noexcept;

    /**
     * @brief: shutdown the queue
     *
     * This method will shutdown the queue and finished all tasks
     *
     * called in `~LocalTaskQueue` to clean up resources.
     */
    void shutdown();

    /**
     * @brief: wait for completion of all tasks in the queue
     *
     * This method will block until all tasks in the queue have completed.
     * may be used like a barrier to this threads.
     * When called this method will block until all tasks in the queue have
     * completed.
     *
     * After this method returns, state change to `State::Idle`.
     */
    void waitForCompletion();

    // pop back one task
    //
    // has a mutex lock to protect the queue
    void popBack();

    // pop front
    //
    // has a mutex lock to protect the queue
    void popFront();

    // // it will move the task to the back of the queue
    // void push_back(TaskPtr task);

    typing::Rc< Task > getFrontDirect();
    typing::Rc< Task > getBackDirect();

    // run front one task each time
    // return `false` if no task to run
    bool step();

    // run all task (always from the front)
    void run();

    void clear();

  private:
    size_t m_capacity;
    size_t m_hid;            // a hashed thread id
    ::std::thread::id m_tid; // the thread id
    ::std::deque< RcTaskPtr > m_queue;
    mutable ::std::mutex m_mx;
    ::std::condition_variable m_cv;
    mutable ::std::atomic< State > m_state = State::Idle;
    ::std::exception_ptr m_exception_ptr;
    ::std::jthread m_worker;
};

} // namespace cxxmp
