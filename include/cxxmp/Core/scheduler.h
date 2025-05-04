#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The scheduler system is responsible for managing the local tasks queue and
 * global tasks queue. User should directly submit tasks to the scheduler
 * and scheduler will allocate task to local or global queue.
 *
 * The scheduler always makes the local tasks queue capacity equals to
 * `32 * sys::CXXMP_PROC_COUNT`
 *
 */

#include <bitset>
#include <chrono>
#include <cstddef>
#include <thread>
#include <unordered_map>
#include <utility>

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/typing.h"
#include "cxxmp/Core/queueObserver.h"
#include "cxxmp/Core/taskQueue.h"

namespace cxxmp {

template < size_t NumLocalQueues = sys::CXXMP_PROC_COUNT >
class Scheduler : public TaskQueueObserver {
  public:
    using Tid = ::std::thread::id;
    using Hid = size_t;

  private:
    // get the current id and go to the next one
    // mod by the number of CPUs to make it round-robin
    size_t nextTaskQueueId() noexcept {
        return (m_clockAllocatorTaskQueueId++) % NumLocalQueues;
    }

    // get the previous id and go to the previous one
    // mod by the number of CPUs to make it round-robin
    size_t prevTaskQueueId() noexcept {
        return (m_clockAllocatorTaskQueueId + NumLocalQueues - 1) %
               NumLocalQueues;
    }

    template < core::isValidTask TaskType >
    bool submitToLocal(TaskType&& task, size_t idx) {
        if (m_localQueuesMap[idx]->submit(std::forward< TaskType >(task))) {
            m_fullLocalQueue.set(idx, m_localQueuesMap[idx]->full());
            return true;
        }
        return false;
    }

    // push a task to the local queue
    template < core::isValidTask TaskType >
    bool push2Local(
      TaskType&& task, typing::Option< size_t > indx = typing::None) noexcept {
        if (m_fullLocalQueue.all()) {
            return false;
        }
        size_t id = indx.has_value() ? indx.value() : nextTaskQueueId();
        if (indx.has_value()) {
            if (id > numCPUs()) {
                log::error("Scheduler Submitting a bad id: {}, RollBack to use "
                           "round-robin",
                  id);
                id = nextTaskQueueId();
            }
            else if (m_fullLocalQueue.test(id)) {
                log::warn("Scheduler LocalTaskQueue[{}] is full, try to "
                          "allocate to others",
                  id);
            }
        }
        // find the next available local queue
        while (m_fullLocalQueue.test(id)) {
            id = nextTaskQueueId();
        }
        log::trace("Pushing task to local queue {}", id);
        return submitToLocal(std::forward< TaskType >(task), id);
    }

    // move a task from the global queue front a specified local queue with
    // index
    bool moveGlobalToLocal(size_t idx) noexcept {
        if (m_globalQueue->empty() || m_fullLocalQueue.test(idx)) {
            // nothing to move or local queue is full
            return false;
        }

        auto task       = m_globalQueue->popFront();
        auto copiedTask = task;
        // copy the task
        if (!task) {
            return false; // Global queue is empty
        }
        if (submitToLocal(std::move(task), idx)) {
            log::trace("Scheduler Move Global Task to LocalTaskQueue[{}]", idx);
            m_fullLocalQueue.set(idx, m_localQueuesMap[idx]->full());
            return true;
        }
        else {
            log::trace("Scheduler Move Global Task to LocalTaskQueue[{}] "
                       "Failed, move to back",
              idx);
            m_globalQueue->store(std::move(copiedTask));
            return false;
        }
    }

    typing::Option< size_t > indexFromHid(Hid hid) noexcept {
        const auto it = m_localQueueMapIndex.find(hid);
        if (it != m_localQueueMapIndex.end()) {
            return it->second;
        }
        return typing::None;
    }

  public:
    // Constructor
    [[nodiscard("Should not be called directly. If you want to create a "
                "scheduler, use the build() method.")]]
    Scheduler(bool enableTaskStealing = true) {
        for (size_t i = 0ul; i < NumLocalQueues; ++i) {
            m_localQueuesMap[i] =
              std::make_unique< core::LocalTaskQueue >(32 * NumLocalQueues);
            m_localQueuesMap[i]->enableStealing(enableTaskStealing);
        }

        auto allTaskQueues =
          std::make_shared< std::vector< core::LocalTaskQueue* > >(
            std::vector< core::LocalTaskQueue* >(NumLocalQueues));
        for (const auto& queue : m_localQueuesMap) {
            allTaskQueues->push_back(queue.get());
        }
        for (auto& queue : m_localQueuesMap) {
            queue->setPear(allTaskQueues);
        }
        for (size_t i = 0ul; i < NumLocalQueues; ++i) {
            m_localQueuesMap[i]->run(this);
            // this must be called before accessing the queue(because it
            // initializes the queue tid and hid)
            m_localQueuesMapIds[i] = {
              m_localQueuesMap[i]->getTid(), m_localQueuesMap[i]->getHid()};
            m_localQueueMapIndex.insert({m_localQueuesMap[i]->getHid(), i});
        }
        m_globalQueue = std::make_unique< core::GlobalTaskQueue >();
    }

    Scheduler(const Scheduler&)            = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&& other) noexcept
        : m_fullLocalQueue(std::move_if_noexcept(other.m_fullLocalQueue)),
          m_localQueuesMap(std::move_if_noexcept(other.m_localQueuesMap)),
          m_localQueuesMapIds(std::move_if_noexcept(other.m_localQueuesMapIds)),
          m_localQueueMapIndex(
            std::move_if_noexcept(other.m_localQueueMapIndex)),
          m_globalQueue(std::move_if_noexcept(other.m_globalQueue)),
          m_clockAllocatorTaskQueueId(other.m_clockAllocatorTaskQueueId) {}

    ~Scheduler() {
        waitForAllCompletion();
        log::debug("Scheduler destroyed");
        for (const auto& queue : m_localQueuesMap) {
            queue->shutdown();
        }
        if (!m_globalQueue->empty()) {
            log::error("Still task uncomplete");
        }
    }

    /**
     * @brief: A static factory function to build a object
     *
     * Pass true if you want the local task queue to steal tasks
     * from each other, default we enable it
     */
    static typing::Rc< Scheduler< NumLocalQueues > > build(
      bool enableTaskStealing = true) noexcept {
        return std::make_shared< Scheduler< NumLocalQueues > >(
          enableTaskStealing);
    }

    static constexpr size_t numCPUs() noexcept { return sys::CXXMP_PROC_COUNT; }

    constexpr size_t numLocalQueues() const noexcept { return NumLocalQueues; }

    // get the local task queue's hashed id
    constexpr Hid getHid(size_t idx) const noexcept {
        return m_localQueuesMapIds[idx].second;
    }

    Tid getTid(size_t idx) const noexcept {
        return m_localQueuesMapIds[idx].first;
    }

    // submit a task to run, always push to the back
    template < core::isValidTask TaskType >
    bool submit(TaskType&& task) {
        if (m_fullLocalQueue.all()) {
            if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                            core::Task >)
            {
                m_globalQueue->store(std::forward< core::Task >(task));
            }
            else {
                m_globalQueue->store(std::move_if_noexcept(task));
            }
            return true;
        }
        return push2Local(std::forward< TaskType >(task));
    }

    // submit a task to run, always push to the back
    template < core::isValidTask TaskType >
    bool submit(size_t indx, TaskType&& task) {
        if (indx >= NumLocalQueues) {
            log::warn(
              "Scheduler Invalid Submitting index, should be less then {}, "
              "rollback to round robin",
              NumLocalQueues);
            return submit(std::forward< TaskType >(task));
        }
        if (m_fullLocalQueue.all()) {
            if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                            core::Task >)
            {
                m_globalQueue->store(std::forward< core::Task >(task));
            }
            else {
                m_globalQueue->store(std::move_if_noexcept(task));
            }
            return true;
        }
        return push2Local(std::forward< TaskType >(task), indx);
    }

    constexpr size_t getLocalSize(size_t idx) const {
        return m_localQueuesMap.at(idx)->getSize();
    }

    constexpr size_t getLocalCapcity(size_t idx) const {
        return m_localQueuesMap.at(idx)->getCapacity();
    }

    constexpr size_t getGlobalSize() const noexcept {
        return m_globalQueue->getSize();
    }

    void pause() noexcept {
        for (auto& queue : m_localQueuesMap) {
            queue->pause();
        }
    }

    void unpause() noexcept {
        for (auto& queue : m_localQueuesMap) {
            queue->unpause();
        }
    }

    /**
     * @brief: Waits for all tasks to complete.
     * This function blocks until all tasks in the scheduler have completed.
     * The tasks to complete are runned in parallel.
     */
    void waitForAllCompletion() noexcept {
        bool allDone = false;
        for (auto& queue : m_localQueuesMap) {
            queue->startCompletion();
        }
        while (!allDone) {
            allDone = true;
            for (size_t i = 0; i < NumLocalQueues; ++i) {
                if (m_localQueuesMap[i]->hasTask() ||
                    m_localQueuesMap[i]->isBusyWorking())
                {
                    allDone = false;
                    break;
                }
            }
            std::this_thread::yield();
        }
    }

    size_t fullLocalQueueCount() const noexcept {
        return m_fullLocalQueue.count();
    }

    bool allLocalQueueFull() const noexcept { return m_fullLocalQueue.all(); }

    void setStealIntervalFor(
      size_t indx, std::chrono::milliseconds interval) noexcept {
        m_localQueuesMap[indx % NumLocalQueues]->setStealInterval(interval);
    }

    void notifyQueueHasSpace(size_t hid) {
        auto indexOp = indexFromHid(hid);
        if (indexOp.has_value()) {
            size_t index = indexOp.value();
            m_fullLocalQueue.set(index, false);
            log::trace("Global Task moved to local queue {}", index);
            moveGlobalToLocal(index);
        }
    }

  private:
    ::std::array< typing::Box< core::LocalTaskQueue >,
      NumLocalQueues >
      m_localQueuesMap; // capacity equals to m_numCPUs

    typing::Box< core::GlobalTaskQueue > m_globalQueue;

    ::std::bitset< NumLocalQueues > m_fullLocalQueue;

    /**
     * @brief: we allocate a task to the local queue like a clock allocator
     * aiming at making the task more balanced and more parallel,
     * because just the front tasks are executed parallelly
     */
    size_t m_clockAllocatorTaskQueueId = 0;

    // this tells the hid's index in the m_localQueuesMap
    // hid -> index
    ::std::unordered_map< Hid, size_t > m_localQueueMapIndex;

    // this stores all the local queue's hashed id and thread id
    // index -> (tid, hid)
    ::std::array< ::std::pair< Tid, Hid >, NumLocalQueues > m_localQueuesMapIds;
};

} // namespace cxxmp
