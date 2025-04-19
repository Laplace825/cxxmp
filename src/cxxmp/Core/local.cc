#include "cxxmp/Core/local.h"

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/typing.h"
#include "cxxmp/Core/task.h"

#include <exception>
#include <mutex>
#include <thread>

namespace cxxmp::core {

#define LOCK_GUARD std::lock_guard< std::mutex > lock(this->m_mx)

void LocalTaskQueue::stateTransfer2(State state) {
    log::trace("LocalTaskQueue[{}] from {} to {}", this->getHid(),
      state2String(this->m_state), state2String(state));
    // std::atomic
    this->m_state = state;

    // Notify one waiting thread, this may notify the thread
    // that is waiting for a task
    // or a waiting thread that is paused
    m_cv.notify_all();
}

void LocalTaskQueue::waitForTask() {
    std::unique_lock< std::mutex > lock(this->m_mx);
    this->m_cv.wait(lock, [this]() {
        return hasTask() || this->m_state == State::Paused ||
               this->m_state == State::Shutdown;
    });
}

void LocalTaskQueue::handleError() {
    log::debug("LocalTaskQueue[{}] handleError()", this->getHid());
    // will handle the error and change the state to idle
    if (this->m_exception_ptr) {
        try {
            throw this->m_exception_ptr;
        } catch (...) {
            log::error("LocalTaskQueue[{}] Get Error", this->getHid());
        }
        this->m_exception_ptr = nullptr;
    }
}

typing::Rc< Task > LocalTaskQueue::getFrontDirect() {
    LOCK_GUARD;
    if (this->m_queue.empty()) {
        log::debug(
          "LocalTaskQueue[{}] frontDirect(): Queue is empty", this->getHid());
        return nullptr;
    }

    auto task = this->m_queue.front();
    // Debug the shared_ptr before conversion
    log::debug("LocalTaskQueue[{}] frontDirect(): Got task with use_count = {}",
      this->getHid(), task.use_count());
    return task;
}

typing::Rc< Task > LocalTaskQueue::getBackDirect() {
    LOCK_GUARD;
    if (this->m_queue.empty()) {
        log::debug(
          "LocalTaskQueue[{}] frontDirect(): Queue is empty", this->getHid());
        return nullptr;
    }

    auto task = this->m_queue.back();
    // Debug the shared_ptr before conversion
    log::debug("LocalTaskQueue[{}] frontDirect(): Got task with use_count = {}",
      this->getHid(), task.use_count());
    return task;
}

void LocalTaskQueue::popBack() {
    if (this->m_queue.empty()) {
        return;
    }
    LOCK_GUARD;
    this->m_queue.pop_back();
}

void LocalTaskQueue::popFront() {
    if (this->m_queue.empty()) {
        return;
    }
    LOCK_GUARD;
    this->m_queue.pop_front();
}

bool LocalTaskQueue::hasTask() const noexcept { return !this->m_queue.empty(); }

void LocalTaskQueue::pause() noexcept {
    if (this->m_state == State::Shutdown || this->m_state == State::Paused ||
        this->m_state == State::ToCompleteAll)
    {
        return;
    }
    log::debug("LocalTaskQueue[{}] Pause", this->getHid());
    this->stateTransfer2(State::Paused);
}

void LocalTaskQueue::unpause() noexcept {
    if (this->m_state == State::Shutdown ||
        this->m_state == State::ToCompleteAll)
    {
        return;
    }
    else if (this->m_state == State::Paused) {
        log::debug("LocalTaskQueue[{}] unpause", this->getHid());
        this->stateTransfer2(State::Idle);
    }
}

void LocalTaskQueue::clear() {
    LOCK_GUARD;
    this->m_queue.clear();
}

void LocalTaskQueue::shutdown() {
    if (this->m_state == State::Shutdown) {
        return;
    }
    log::debug("LocalTaskQueue[{}] Shutdown", this->getHid());
    this->stateTransfer2(State::Shutdown);

    // Ensure the worker thread has time to notice the shutdown
    std::this_thread::yield();

    if (this->hasTask()) {
        this->clear();
    }
}

void LocalTaskQueue::waitForCompletion() {
    log::debug("LocalTaskQueue[{}] WaitForCompletion", this->getHid());
    // to state busy working and doing all the jobs, when the queue is
    // empty the completion condition is met
    this->stateTransfer2(State::ToCompleteAll);
    while (this->hasTask()) {
        std::this_thread::yield();
    }
    log::debug("LocalTaskQueue[{}] WaitForCompletion Done", this->getHid());
}

// just run one front task each time
bool LocalTaskQueue::step() {
    bool runned = false;
    if (hasTask()) {
        log::debug("Working LocalTaskQueue[{}] Tasks: {}", this->getHid(),
          this->m_queue.size());
        auto task = this->getFrontDirect();
        this->popFront();
        if (task) {
            try {
                task->execute();
                runned = true;
            } catch (const std::exception& e) {
                // Handle the exception
                this->m_exception_ptr = std::make_exception_ptr(e);
                this->stateTransfer2(State::SubTaskErrorHappend);
            }
        }
    }
    log::debug("After Working LocalTaskQueue[{}] Tasks: {}", this->getHid(),
      this->m_queue.size());
    return runned;
}

void LocalTaskQueue::run() {
    m_worker    = std::jthread([this]() {
        // the state machine
        while (m_state != State::Shutdown) {
            switch (m_state) {
                case State::Shutdown: {
                    // `Shutdown` state could be transferred just
                    // when the life cycle is over or the user requested to
                    // shutdown
                    return;
                }
                case State::Idle: {
                    // `Busy` -> `Idle`: when done one task
                    // `Paused` -> `Idle` when user requested to unpause
                    // `SubTaskErrorHappend` -> `Idle` when subtask error
                    // was handled
                    //
                    // `ToCompleteAll` -> `Idle` only when all tasks are
                    // completed

                    // this will be activated when user submit a task
                    waitForTask();

                    // when get a task, check if we should transfer
                    // to pause ourself, else we transfer to busy
                    if (m_state == State::Paused) {
                        stateTransfer2(State::Paused);
                    }
                    else if (hasTask()) {
                        stateTransfer2(State::Busy);
                    }
                    break;
                }
                case State::ToCompleteAll: {
                    // Any State could be transfered to ToCompleteAll
                    // when user requested to complete all tasks
                    try {
                        if (!step()) {
                            log::debug("LocalTaskQueue[{}] CompleteAll "
                                             "finished, Transfer to Idle",
                                 this->getHid());
                            // no task to done
                            stateTransfer2(State::Idle);
                        };
                    } catch (...) {
                        log::error("LocalTaskQueue[{}] CompleteAll Error",
                             this->getHid());
                    }
                    break;
                }
                case State::Busy: {
                    // `Idle` -> `Busy` when get one work to do
                    try {
                        step();
                        // a task is done without exception
                        if (m_state == State::Busy) {
                            // when we still working but user already change
                            // the state (may pause or shutdown or complete
                            // all)
                            stateTransfer2(State::Idle);
                        }
                    } catch (...) {
                        std::rethrow_exception(std::current_exception());
                        stateTransfer2(State::SubTaskErrorHappend);
                    }
                    break;
                }
                case State::Paused: {
                    // `Busy` -> `Paused` when user requested to pause
                    // `Idle` -> `Paused` when user requested to pause

                    {
                        std::unique_lock< std::mutex > lock(m_mx);
                        // this will be released when the state is not
                        // paused `unpause` will change the state to idle
                        m_cv.wait(lock, [this] {
                            log::trace(
                              "Waiting for state change from Paused to Idle");
                            return m_state != State::Paused;
                        });
                    }
                    break;
                }
                case State::SubTaskErrorHappend: {
                    // `Busy` -> `SubTaskErrorHappend` when task error
                    // happend
                    handleError();
                    stateTransfer2(State::Idle);
                    break;
                }
            } // end switch
        } // end while loop
    });
    this->m_tid = this->m_worker.get_id();
    this->m_hid = std::hash< std::thread::id >{}(this->m_tid);
    log::debug("LocalTaskQueue[{}] Run", this->getHid());
}
} // namespace cxxmp
