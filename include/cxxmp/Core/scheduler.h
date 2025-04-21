#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The scheduler system is responsible for managing the local tasks queue and
 * global tasks queue. User should directly submit tasks to the scheduler
 * and scheduler will allocate task to local or global queue.
 *
 * The scheduler is also a state machine
 */

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/typing.h"
#include "cxxmp/Core/taskQueue.h"

namespace cxxmp {

class Scheduler {
  public:
    // Constructor
    [[nodiscard("Should not be called directly. If you want to create a "
                "scheduler, use the build() method.")]]
    Scheduler();

    Scheduler(const Scheduler&)            = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&& other) noexcept
        : m_numCPUs(other.m_numCPUs),
          m_localQueuesMap(std::move(other.m_localQueuesMap)),
          m_globalQueue(std::move(other.m_globalQueue)) {}

    ~Scheduler() {
        for (auto& queue : m_localQueuesMap) {
            queue->shutdown();
        }
    };

    static typing::Box< Scheduler > build() {
        return std::make_unique< Scheduler >();
    }

    consteval size_t numCPUs() const { return m_numCPUs; }

  private:
    static size_t objCnt;
    const size_t m_numCPUs{sys::CXXMP_PROC_COUNT};
    mutable ::std::array< typing::Box< core::LocalTaskQueue >,
      sys::CXXMP_PROC_COUNT >
      m_localQueuesMap; // capacity equals to m_numCPUs
    mutable typing::Box< core::GlobalTaskQueue > m_globalQueue;

    /**
     * @brief: we allocate a task to the local queue like a clock allocator
     * aiming at making the task more balanced and more parallel,
     * because just the front tasks are executed parallelly
     */
    size_t m_clockAllocatorTaskQueueId = 0;
};

} // namespace cxxmp
