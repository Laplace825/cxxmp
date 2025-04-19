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
#include "cxxmp/Core/local.h"
#include "cxxmp/Utily/getsys.h"

#include <ranges>

namespace cxxmp {

class Scheduler {
  private:
    static size_t objCnt;

  public:
    // Constructor
    [[nodiscard("Should not be called directly. If you want to create a "
                "scheduler, use the build() method.")]]
    Scheduler(size_t = sys::getSysCPUs());

    Scheduler(const Scheduler&)            = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&& other) noexcept
        : m_numCPUs(other.m_numCPUs),
          m_localQueues(std::move(other.m_localQueues)) {
        other.m_numCPUs = 0;
    }

    ~Scheduler() {
        for (auto& queue : m_localQueues) {
            queue->shutdown();
        }
    };

    static typing::Box< Scheduler > build() {
        return std::make_unique< Scheduler >(sys::getSysCPUs());
    }

    size_t numCPUs() const { return m_numCPUs; }

  private:
    size_t m_numCPUs;
    ::std::vector< typing::Box< core::LocalTaskQueue > >
      m_localQueues; // capacity equals to m_numCPUs

    /**
     * @brief: we allocate a task to the local queue like a clock allocator
     * aiming at making the task more balanced and more parallel,
     * because just the front tasks are executed parallelly
     */
    size_t m_clockAllocatorTaskQueueId = 0;
};

} // namespace cxxmp
