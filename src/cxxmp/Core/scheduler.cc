#include <cxxmp/Core/scheduler.h>
#include <ranges>

namespace cxxmp {

size_t Scheduler::objCnt = 0;

Scheduler::Scheduler() {
    ++objCnt;
    if (objCnt > 1) {
        throw std::runtime_error("Only one scheduler can be created");
    }
    for (size_t i : std::views::iota(0u, m_numCPUs)) {
        m_localQueuesMap[i] = std::make_unique< core::LocalTaskQueue >();
        m_localQueuesMap[i]->run();
    }
}

} // namespace cxxmp
