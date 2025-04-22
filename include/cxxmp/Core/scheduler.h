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
 */

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/typing.h"
#include "cxxmp/Core/queueObserver.h"
#include "cxxmp/Core/taskQueue.h"

#include <bitset>
#include <thread>
#include <unordered_map>

namespace cxxmp {

class Scheduler : public TaskQueueObserver {

  public:
    using Tid = ::std::thread::id;
    using Hid = size_t;

  private:
    // get the current id and go to the next one
    // mod by the number of CPUs to make it round-robin
    size_t nextTaskQueueId() noexcept {
        return (m_clockAllocatorTaskQueueId++) % sys::CXXMP_PROC_COUNT;
    }

    // get the previous id and go to the previous one
    // mod by the number of CPUs to make it round-robin
    size_t prevTaskQueueId() noexcept {
        return (m_clockAllocatorTaskQueueId + sys::CXXMP_PROC_COUNT - 1) %
               sys::CXXMP_PROC_COUNT;
    }

    template < typename TaskType >
        requires ::std::is_same_v< std::decay_t< TaskType >, core::Task > ||
                 ::std::is_same_v< std::decay_t< TaskType >, core::RcTaskPtr >
    bool submitToLocal(TaskType&& task, size_t idx) {
        if (m_localQueuesMap[idx]->submit(std::forward< TaskType >(task))) {
            m_fullLocalQueue.set(idx, m_localQueuesMap[idx]->full());
            return true;
        }
        return false;
    }

    // push a task to the local queue
    template < typename TaskType >
        requires ::std::is_same_v< TaskType, core::Task > ||
                 ::std::is_same_v< TaskType, core::RcTaskPtr >
    bool push2Local(TaskType&& task) noexcept {
        if (m_fullLocalQueue.all()) {
            return false;
        }
        size_t id = nextTaskQueueId();
        // find the next available local queue
        while (m_fullLocalQueue.test(id)) {
            id = nextTaskQueueId();
        }
        log::trace("Pushing task to local queue {}", id);
        return submitToLocal(std::forward< TaskType >(task), id);
    }

    // move a task from the global queue front a specified local queue with
    // index
    bool moveGlobalToLocal(size_t idx) noexcept;
    typing::Option< size_t > indexFromHid(Hid hid) noexcept;

  public:
    // Constructor
    [[nodiscard("Should not be called directly. If you want to create a "
                "scheduler, use the build() method.")]]
    Scheduler();

    Scheduler(const Scheduler&)            = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&& other) noexcept;

    ~Scheduler();

    static typing::Box< Scheduler > build() noexcept;

    constexpr size_t numCPUs() const noexcept { return sys::CXXMP_PROC_COUNT; }

    // get the local task queue's hashed id
    constexpr Hid getHid(size_t idx) const noexcept {
        return m_localQueuesMapIds[idx].second;
    }

    constexpr Tid getTid(size_t idx) const noexcept {
        return m_localQueuesMapIds[idx].first;
    }

    // submit a task to run, always push to the back
    template < typename TaskType >
        requires ::std::is_same_v< TaskType, core::Task > ||
                 ::std::is_same_v< TaskType, core::RcTaskPtr >
    bool submit(TaskType&& task) {
        if (m_fullLocalQueue.all()) {
            if constexpr (::std::is_same_v< ::std::decay_t< TaskType >,
                            core::Task >)
            {
                m_globalQueue->store(std::forward< core::Task >(task));
            }
            else {
                m_globalQueue->store(std::move(task));
            }
            return true;
        }
        return push2Local(std::forward< TaskType >(task));
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

    void pause() noexcept;
    void unpause() noexcept;

    /**
     * @brief: Waits for all tasks to complete.
     * This function blocks until all tasks in the scheduler have completed.
     * The tasks to complete are runned in parallel.
     */
    void waitForAllCompletion() noexcept;

    size_t fullLocalQueueCount() const noexcept;

    bool allLocalQueueFull() const noexcept { return m_fullLocalQueue.all(); }

    void notifyQueueHasSpace(size_t hid) override;

  private:
    static size_t objCnt;
    ::std::array< typing::Box< core::LocalTaskQueue >,
      sys::CXXMP_PROC_COUNT >
      m_localQueuesMap; // capacity equals to m_numCPUs

    // this stores all the local queue's hashed id and thread id
    // index -> (tid, hid)
    ::std::array< ::std::pair< Tid, Hid >, sys::CXXMP_PROC_COUNT >
      m_localQueuesMapIds;

    // this tells the hid's index in the m_localQueuesMap
    // hid -> index
    ::std::unordered_map< Hid, size_t > m_localQueueMapIndex;
    typing::Box< core::GlobalTaskQueue > m_globalQueue;
    ::std::bitset< sys::CXXMP_PROC_COUNT > m_fullLocalQueue;

    /**
     * @brief: we allocate a task to the local queue like a clock allocator
     * aiming at making the task more balanced and more parallel,
     * because just the front tasks are executed parallelly
     */
    size_t m_clockAllocatorTaskQueueId = 0;
};

} // namespace cxxmp
