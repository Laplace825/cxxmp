#include "cxxmp/Core/taskQueue.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include "cxxmp/Common/log.h"
#include "cxxmp/Core/queueObserver.h"
#include "cxxmp/Core/task.h"

namespace cxxmp::core {

#define LOCK_GUARD std::lock_guard< std::mutex > lock(this->m_mx)

void LocalTaskQueue::stateTransfer2(State state) noexcept {
    log::trace("LocalTaskQueue[{}] from {} to {}", this->getHid(),
      state2String(this->m_state), state2String(state));
    // std::atomic
    this->m_state.store(state);

    // Notify one waiting thread, this may notify the thread
    // that is waiting for a task
    // or a waiting thread that is paused
    m_cv.notify_all();
}

void LocalTaskQueue::handleError() {
    log::debug("LocalTaskQueue[{}] handleError()", this->getHid());
    // will handle the error and change the state to idle
    if (this->m_exception_ptr) {
        try {
            throw this->m_exception_ptr;
        } catch (const std::exception& e) {
            log::error(
              "LocalTaskQueue[{}] Get Error: {}", this->getHid(), e.what());
        }
        this->m_exception_ptr = nullptr;
    }
}

RcTaskPtr LocalTaskQueue::popBack() {
    LOCK_GUARD;
    return this->pop(false);
}

RcTaskPtr LocalTaskQueue::popFront() {
    LOCK_GUARD;
    return this->pop(true);
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

void LocalTaskQueue::setPear(
  typing::Rc< ::std::vector< LocalTaskQueue* > > peer) noexcept {
    m_peers = peer;
}

void LocalTaskQueue::signalStealing() noexcept {
    if (m_stealEnabled && m_state == State::Idle) {
        log::debug("LocalTaskQueue[{}] being Signal Steal", getHid());
        m_shouldCheckStealing.store(true);
        m_cv.notify_one();
    }
}

void LocalTaskQueue::enableStealing(bool enabled) noexcept {
    m_stealEnabled = enabled;
}

size_t LocalTaskQueue::stealFrom(
  LocalTaskQueue* victim, size_t maxItems) noexcept {
    if (!victim || !victim->hasTask() || victim == this || full() ||
        m_state == State::Shutdown)
    {
        return 0;
    }
    if (victim->getState() == State::Shutdown) {
        return 0;
    }
    victim->pause();
    this->pause();
    log::trace("LocalTaskQueue[{}] Steal a Task From LocalTaskQueue[{}]",
      getHid(), victim->getHid());
    size_t stolenCnt{0};
    size_t toSteal = std::min(victim->getSize(), maxItems);

    // can't over the capacity
    toSteal = std::min(toSteal, getCapacity() - getSize());

    for (size_t i = 0; i < toSteal; ++i) {
        auto task = std::move(victim->popBack());
        submit(task);
        ++stolenCnt;
    }

    if (stolenCnt > 0) {
        // if we actually steal something
        m_cv.notify_one();
    }

    this->unpause();
    victim->unpause();
    return stolenCnt;
}

bool LocalTaskQueue::tryStealTask() noexcept {
    if (!m_peers || m_peers->empty()) {
        log::debug("LocalTaskQueue[{}] could not steal Task", getHid());
        return false;
    }
    log::debug("LocalTaskQueue[{}] Try to steal task", getHid());

    size_t totalPeers = m_peers->size();
    // try to steal from a random peer
    size_t startIndx = rand() % totalPeers;

    // find someone who can be stolen
    for (size_t i = 0; i < totalPeers; ++i) {
        size_t indx = (startIndx + i) % totalPeers;
        auto victim = (*m_peers)[indx];
        log::trace("LocalTaskQueue[{}] Get victim null: {}, not this: {}",
          getHid(), victim == nullptr, victim != this);
        // do not steal from who just have one task
        if (victim && victim != this && victim->getSize() > 1) {
            if (stealFrom(victim, 1) > 0) {
                // actually we steal something, so return
                return true;
            }
        }
    }
    return false;
}

void LocalTaskQueue::waitForTask() noexcept {
    using namespace std::chrono_literals;
    std::unique_lock< std::mutex > lock(this->m_mx);
    if (m_stealEnabled) {
        if (!this->m_cv.wait_for(lock, 50ms, [this]() {
                return hasTask() || this->m_state == State::Paused ||
                       this->m_state == State::Shutdown ||
                       m_shouldCheckStealing;
            }))
        {
            // wait for up to 10ms, check if we should steal
            m_shouldCheckStealing = true;
        }
    }
    else {
        this->m_cv.wait(lock, [this]() {
            return hasTask() || this->m_state == State::Paused ||
                   this->m_state == State::Shutdown;
        });
    }
    m_shouldCheckStealing = false;
}

void LocalTaskQueue::shutdown() {
    if (this->m_state == State::Shutdown) {
        return;
    }
    log::debug("LocalTaskQueue[{}] Shutdown", this->getHid());
    m_shouldCheckStealing = false;
    m_stealEnabled        = false;
    // seems shared_ptr is not required to clear by myself
    // this won't cause memory leak because the shared_ptr has no recursive
    // reference count
    //
    // if (m_peers) {
    // }
    this->stateTransfer2(State::Shutdown);

    // Ensure the worker thread has time to notice the shutdown
    std::this_thread::yield();

    if (this->hasTask()) {
        this->clear();
    }
}

void LocalTaskQueue::waitForCompletion() {
    if (m_state == State::Shutdown || m_state == State::ToCompleteAll) {
        return;
    }
    log::debug("LocalTaskQueue[{}] WaitForCompletion", this->getHid());
    // to state busy working and doing all the jobs, when the queue is
    // empty the completion condition is met
    this->stateTransfer2(State::ToCompleteAll);
    while (this->hasTask()) {
        std::this_thread::yield();
    }
    log::debug("LocalTaskQueue[{}] WaitForCompletion Done", this->getHid());
}

void LocalTaskQueue::startCompletion() {
    if (m_state == State::Shutdown || m_state == State::ToCompleteAll) {
        return;
    }
    if (hasTask()) {
        log::debug("LocalTaskQueue[{}] startCompletion", this->getHid());
        this->stateTransfer2(State::ToCompleteAll);
    }
}

// just run one front task each time
bool LocalTaskQueue::step(TaskQueueObserver* observer) {
    bool runned = false;
    if (hasTask()) {
        log::debug("Working LocalTaskQueue[{}] Has Tasks: {}", this->getHid(),
          this->m_queue.size());
        auto task = this->popFront();
        if (task) {
            try {
                task->execute();
                runned = true;
                if (observer && m_state != State::Shutdown) {
                    observer->notifyQueueHasSpace(getHid());
                    log::trace("LocalTaskQueue[{}] notify Has Space", getHid());
                }
            } catch (...) {
                this->m_exception_ptr = std::current_exception();
                this->stateTransfer2(State::SubTaskErrorHappend);
            }
        }
    }
    log::debug("After Working LocalTaskQueue[{}] Tasks: {}", this->getHid(),
      this->m_queue.size());
    return runned;
}

void LocalTaskQueue::notifyPeersGetTask() noexcept {
    if (m_peers && m_stealEnabled && getSize() > 1) {
        log::debug("LocalTaskQueue[{}] notify peers get task", this->getHid());
        for (auto peer : *m_peers) {
            if (peer && peer != this && peer->getState() == State::Idle) {
                peer->signalStealing();
            }
        }
    }
}

void LocalTaskQueue::run(TaskQueueObserver* observer) {
    using namespace std::chrono_literals;
    m_worker = std::jthread([this, observer]() {
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

                    bool shouldTrySteal =
                      m_stealEnabled &&
                      (m_shouldCheckStealing ||
                        std::chrono::steady_clock::now() - m_lastStealAttempt >
                          50ms);
                    if (shouldTrySteal) {
                        m_lastStealAttempt = std::chrono::steady_clock::now();
                        m_shouldCheckStealing = false;
                        if (tryStealTask()) {
                            stateTransfer2(State::Busy);
                            // we get task, so just finish it
                            break;
                        }
                    }

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
                        if (!step(observer)) {
                            log::debug("LocalTaskQueue[{}] CompleteAll "
                                       "finished, Transfer to Idle",
                              this->getHid());
                            // no task to done
                            stateTransfer2(State::Idle);
                        }
                    } catch (const std::exception& e) {
                        log::error("LocalTaskQueue[{}] CompleteAll "
                                   "Got Error: {}",
                          this->getHid(), e.what());
                    }
                    break;
                }
                case State::Busy: {
                    // `Idle` -> `Busy` when get one work to do
                    try {
                        step(observer);
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
                              "Waiting for state change from Paused to Any");
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
    m_tid    = m_worker.get_id();
    m_hid    = std::hash< std::thread::id >{}(m_tid);
    log::debug("LocalTaskQueue[{}] Run", this->getHid());
}
} // namespace cxxmp::core
