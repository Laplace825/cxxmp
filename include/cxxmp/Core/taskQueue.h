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
 *
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#include "cxxmp/Common/log.h"
#include "cxxmp/config.h"
#include "cxxmp/Core/queueObserver.h"
#include "cxxmp/Core/task.h"

namespace cxxmp::core {

class TaskQueue {
  protected:
    ::std::deque< RcTaskPtr > m_queue;
    mutable ::std::mutex m_mx;
    size_t m_capacity;

  protected:
    RcTaskPtr pop(bool front) {
        if (this->m_queue.empty()) {
            return nullptr;
        }
        auto task =
          std::move(front ? this->m_queue.front() : this->m_queue.back());
        if (front) {
            this->m_queue.pop_front();
        }
        else {
            this->m_queue.pop_back();
        }
        return task;
    }

  public:
    TaskQueue(const TaskQueue&)            = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    TaskQueue()                            = default;

    TaskQueue(TaskQueue&& other)
        : m_queue(std::move(other.m_queue)), m_capacity(other.m_capacity) {
        other.m_queue.clear();
    }

    TaskQueue(size_t capacity) : m_capacity(capacity) { m_queue.clear(); }

    ~TaskQueue() {
        log::debug("TaskQueue destroyed");
        m_queue.clear();
    }

    bool empty() const noexcept { return m_queue.empty(); }

    constexpr size_t getSize() const noexcept { return m_queue.size(); }

    constexpr size_t getCapacity() const noexcept { return m_capacity; }

    constexpr bool full() const noexcept { return getSize() == getCapacity(); }
};

/**
 * @brief: the queue is a thread-local queue that is used to manage the
 * local tasks for a thread.
 *
 * This is a state machine
 */
class LocalTaskQueue : public TaskQueue {
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

    void stateTransfer2(State state) noexcept;

    void signalStealing() noexcept;

    void waitForTask() noexcept;

    bool tryStealTask() noexcept;

    // when one self get a task and now has 2 more
    // notify peers they can steal task
    void notifyPeersGetTask() noexcept;

  public:
    LocalTaskQueue(const LocalTaskQueue&)            = delete;
    LocalTaskQueue& operator=(const LocalTaskQueue&) = delete;

    LocalTaskQueue(LocalTaskQueue&& other) noexcept
        : m_hid(other.m_hid), m_tid(other.m_tid), m_state(other.m_state.load()),
          m_exception_ptr(other.m_exception_ptr), TaskQueue(std::move(other)) {
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

    explicit LocalTaskQueue(size_t capacity) : TaskQueue(capacity) {
        log::debug("LocalTaskQueue created");
        // Initialize the queue
        m_state            = State::Idle;
        m_exception_ptr    = nullptr;
        m_capacity         = capacity;
        m_lastStealAttempt = std::chrono::steady_clock::now();
    }

    LocalTaskQueue() : TaskQueue(sys::CXXMP_PROC_COUNT * 32) {
        log::debug("LocalTaskQueue created");
        // Initialize the queue
        m_state            = State::Idle;
        m_exception_ptr    = nullptr;
        m_lastStealAttempt = std::chrono::steady_clock::now();
    }

    ~LocalTaskQueue() {
        log::debug("LocalTaskQueue destroyed");
        this->shutdown();
        if (m_peers) {
            log::trace("LocalTaskQueue[{}] peers shared_ptr use count: {}",
              getHid(), m_peers.use_count());
        }
    }

    // submit a task to run, always push to the back
    template < typename TaskType >
        requires ::std::is_same_v< std::decay_t< TaskType >, Task > ||
                 ::std::is_same_v< std::decay_t< TaskType >, RcTaskPtr >
    bool submit(TaskType&& task) {
        if (m_state == State::Shutdown) {
            return false;
        }
        {
            std::lock_guard< std::mutex > lock(m_mx);
            if (m_queue.size() >= m_capacity) {
                return false;
            }
            if constexpr (::std::is_same_v< ::std::decay_t< TaskType >, Task >)
            {
                return submit(
                  std::make_shared< Task >(std::forward< Task >(task)));
            }
            else if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                                 RcTaskPtr >)
            {
                // In submit method
                log::debug("Submitting task: ptr={:p}, valid={}",
                  (void*)task.get(), task != nullptr);
                m_queue.push_back(std::move(task));
                log::debug("LocalTaskQueue Tasks: {}", m_queue.size());
            }
            m_cv.notify_one();
        }
        notifyPeersGetTask();
        // After pushing to queue
        log::debug("Queue now has {} tasks", m_queue.size());
        return true;
    }

    bool isBusyWorking() const noexcept {
        switch (m_state) {
            case State::Busy:
            case State::ToCompleteAll:
            case State::SubTaskErrorHappend:
                return true;
            default:
                return false;
        }
    }

    State getState() const noexcept { return m_state; }

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

    /**
     * @brief: start completion of all tasks in the queue
     *
     * This method will start completion of all tasks in the queue.
     * Unlike `waitForCompletion`, this method will not block until all tasks
     * have completed. It will start completion of all tasks in the queue and
     * return immediately.
     *
     * After this method returns, state change to `State::Idle`.
     */
    void startCompletion();

    // pop back one task
    //
    // has a mutex lock to protect the queue
    RcTaskPtr popBack();

    // pop front
    //
    // has a mutex lock to protect the queue
    RcTaskPtr popFront();

    // run front one task each time
    // return `false` if no task to run
    bool step(TaskQueueObserver* = nullptr);

    // run all task (always from the front)
    void run(TaskQueueObserver* = nullptr);

    void clear();

    // returns the task that is stolen from other
    size_t stealFrom(
      LocalTaskQueue* victim = nullptr, size_t maxStealItem = 1) noexcept;

    void setPear(typing::Rc< ::std::vector< LocalTaskQueue* > > peer) noexcept;

    void enableStealing(bool enabled = true) noexcept;

  private:
    size_t m_hid;            // a hashed thread id
    ::std::thread::id m_tid; // the thread id
    ::std::condition_variable m_cv;
    mutable ::std::atomic< State > m_state = State::Idle;
    ::std::exception_ptr m_exception_ptr;
    ::std::jthread m_worker;
    typing::Rc< ::std::vector< LocalTaskQueue* > > m_peers{nullptr};
    bool m_stealEnabled{true};

    ::std::chrono::steady_clock::time_point m_lastStealAttempt;
    ::std::atomic< bool > m_shouldCheckStealing{false};
};

// Global task queue
class GlobalTaskQueue : public TaskQueue {
  public:
    GlobalTaskQueue()  = default;
    ~GlobalTaskQueue() = default;

    GlobalTaskQueue(GlobalTaskQueue&& other) noexcept
        : TaskQueue(::std::move(other)) {}

    // store a task to the back
    template < typename TaskType >
        requires ::std::is_same_v< ::std::decay_t< TaskType >, Task > ||
                 ::std::is_same_v< ::std::decay_t< TaskType >, RcTaskPtr >
    void store(TaskType&& task) {
        ::std::lock_guard< std::mutex > lock(m_mx);
        if constexpr (::std::is_same_v< ::std::decay_t< TaskType >, Task >) {
            log::trace("GlobalTaskQueue Storing task");
            m_queue.push_back(
              ::std::make_shared< Task >(std::forward< Task >(task)));
        }
        else if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                             RcTaskPtr >)
        {
            log::trace("GlobalTaskQueue Storing Ptr task: ptr={:p}, valid={}",
              (void*)task.get(), task != nullptr);
            m_queue.push_back(std::move(task));
        }
    }

    RcTaskPtr popFront() {
        std::lock_guard< std::mutex > lock(m_mx);
        return pop(true);
    }
};

} // namespace cxxmp::core
