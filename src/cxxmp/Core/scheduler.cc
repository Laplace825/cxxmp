#include <cxxmp/Core/scheduler.h>

namespace cxxmp {

size_t Scheduler::objCnt = 0;

Scheduler::Scheduler(size_t num_cpus) {
    ++objCnt;
    if (objCnt > 1) {
        throw std::runtime_error("Only one scheduler can be created");
    }
    m_numCPUs = num_cpus;
    m_localQueues.resize(m_numCPUs);
    for (size_t i : std::views::iota(0) | std::views::take(m_numCPUs)) {
        m_localQueues[i] = std::make_unique< core::LocalTaskQueue >();
        log::debug("Scheduler LocalTaskQueue capacity: {}",
          m_localQueues[i]->capacity());
        m_localQueues[i]->run();
    }

    // try to allocate some task to test
    for (size_t i : std::views::iota(0) | std::views::take(m_numCPUs)) {
        m_localQueues[i]->submit(core::Task::build(
          [i]() { log::debug("Task executed on CPU {}", i); }));
    }
}

} // namespace cxxmp
