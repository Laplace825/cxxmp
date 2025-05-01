#include "cxxmp/Core/taskQueue.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "cxxmp/Common/log.h"
#include "cxxmp/Core/queueObserver.h"
#include "cxxmp/Core/task.h"

namespace cxxmp::core {

#define LOCK_GUARD std::lock_guard< std::mutex > lock(this->m_mx)
#define LOG_ERROR \
    log::error("LocalTaskQueue[{}] Got Error: {}", this->getHid(), e.what())

void LocalTaskQueue::spwanCtl(size_t s) noexcept { m_descriptor.spwan += s; }

void LocalTaskQueue::doneCtl(size_t d) noexcept { m_descriptor.done += d; }

bool LocalTaskQueue::isFinishAll() noexcept {
    bool ret = (m_descriptor.spwan == m_descriptor.done) && !hasTask();
    log::trace("LocalTaskQueue[{}] isFinishAll {}", this->getHid(), ret);
    return ret;
}

void LocalTaskQueue::stateTransfer2(State state) noexcept {
    log::trace("LocalTaskQueue[{}] from {} to {}", this->getHid(),
      state2String(this->m_state), state2String(state));
    this->m_state = state;

    // Notify one waiting thread, this may notify the thread
    // that is waiting for a task
    // or a waiting thread that is paused
    m_cv.notify_all();
}

RcTaskPtr LocalTaskQueue::popBack() {
    // @NOTE: the lock here seems not necessary
    // steal just happens when somebody has 1 more task (> 1)
    // `this` just run the front task, and others just steal from back
    // so there is no race condition, but I don't know whether a complex
    // situation will cause race condition or not.

    // LOCK_GUARD;
    return this->pop(false);
}

RcTaskPtr LocalTaskQueue::popFront() {
    // LOCK_GUARD;
    return this->pop(true);
}

bool LocalTaskQueue::hasTask() const noexcept { return !this->m_queue.empty(); }

void LocalTaskQueue::pause() noexcept {
    if (this->isShutdown() || this->isPaused() || this->isToCompleteAll()) {
        return;
    }
    log::debug("LocalTaskQueue[{}] Pause", this->getHid());
    this->stateTransfer2(State::Paused);
}

void LocalTaskQueue::unpause() noexcept {
    if (this->isShutdown() || this->isToCompleteAll()) {
        return;
    }
    else if (this->isPaused()) {
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
        isShutdown())
    {
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
        victim->spwanCtl(-1);
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
                return hasTask() || this->isPaused() || this->isShutdown() ||
                       m_shouldCheckStealing;
            }))
        {
            // wait for up to 10ms, check if we should steal
            m_shouldCheckStealing = true;
        }
    }
    else {
        this->m_cv.wait(lock, [this]() {
            return hasTask() || this->isPaused() || this->isShutdown();
        });
    }
    m_shouldCheckStealing = false;
}

void LocalTaskQueue::shutdown() {
    if (isShutdown()) {
        return;
    }
    m_stealEnabled = false;

    this->waitForCompletion();

    // Ensure the worker thread has time to notice the shutdown
    std::this_thread::yield();
    // seems shared_ptr is not required to clear by myself
    // this won't cause memory leak because the shared_ptr has no recursive
    // reference count
    //
    // if (m_peers) {
    // }

    this->stateTransfer2(State::Shutdown);

    log::debug("LocalTaskQueue[{}] Shutdown", this->getHid());
}

void LocalTaskQueue::waitForCompletion() {
    if (isShutdown() || isToCompleteAll()) {
        return;
    }
    log::debug("LocalTaskQueue[{}] WaitForCompletion", this->getHid());
    // to state busy working and doing all the jobs, when the queue is
    // empty the completion condition is met
    this->stateTransfer2(State::ToCompleteAll);
    while (!this->isFinishAll()) {
        std::this_thread::yield();
    }
    log::debug("LocalTaskQueue[{}] WaitForCompletion Done", this->getHid());
}

void LocalTaskQueue::startCompletion() {
    if (isShutdown() || isToCompleteAll()) {
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
            } catch (const std::exception& e) {
                LOG_ERROR;
            }
            runned = true;
            doneCtl(1);
            log::debug("LocalTaskQueue[{}] Task Executed: {}", this->getHid(),
              this->m_descriptor.done);
            if (observer && !isShutdown()) {
                observer->notifyQueueHasSpace(getHid());
                log::trace("LocalTaskQueue[{}] notify Has Space", getHid());
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
        while (!isShutdown()) {
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
                    log::trace("LocakTaskQueue[{}] Should Try Steal: {}",
                      getHid(), shouldTrySteal);
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
                    if (isPaused()) {
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
                    if (!step(observer)) {
                        log::debug("LocalTaskQueue[{}] CompleteAll "
                                   "finished, Transfer to Idle",
                          this->getHid());
                        // no task to done
                        stateTransfer2(State::Idle);
                    }
                    break;
                }
                case State::Busy: {
                    // `Idle` -> `Busy` when get one work to do
                    step(observer);

                    // a task is done without exception
                    if (m_state == State::Busy) {
                        // when we still working but user already change
                        // the state (may pause or shutdown or complete
                        // all)
                        stateTransfer2(State::Idle);
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
                            return !isPaused();
                        });
                    }
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
