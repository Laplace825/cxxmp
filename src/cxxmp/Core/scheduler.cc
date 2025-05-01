#include "cxxmp/Core/scheduler.h"

#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "cxxmp/Common/typing.h"
#include "cxxmp/config.h"
#include "cxxmp/Core/taskQueue.h"

namespace cxxmp {

typing::Rc< Scheduler > Scheduler::build(bool enableTaskStealing) noexcept {
    return std::make_shared< Scheduler >(enableTaskStealing);
}

Scheduler::Scheduler(Scheduler&& other) noexcept
    : m_fullLocalQueue(std::move_if_noexcept(other.m_fullLocalQueue)),
      m_localQueuesMap(std::move_if_noexcept(other.m_localQueuesMap)),
      m_localQueuesMapIds(std::move_if_noexcept(other.m_localQueuesMapIds)),
      m_localQueueMapIndex(std::move_if_noexcept(other.m_localQueueMapIndex)),
      m_globalQueue(std::move_if_noexcept(other.m_globalQueue)),
      m_clockAllocatorTaskQueueId(other.m_clockAllocatorTaskQueueId) {}

Scheduler::Scheduler(bool enableTaskStealing) : m_fullLocalQueue(0) {
    for (size_t i = 0ul; i < sys::CXXMP_PROC_COUNT; ++i) {
        m_localQueuesMap[i] =
          std::make_unique< core::LocalTaskQueue >(32 * sys::CXXMP_PROC_COUNT);
        m_localQueuesMap[i]->enableStealing(enableTaskStealing);
    }

    auto allTaskQueues =
      std::make_shared< std::vector< core::LocalTaskQueue* > >(
        std::vector< core::LocalTaskQueue* >(sys::CXXMP_PROC_COUNT));
    for (const auto& queue : m_localQueuesMap) {
        allTaskQueues->push_back(queue.get());
    }
    for (auto& queue : m_localQueuesMap) {
        queue->setPear(allTaskQueues);
    }
    for (size_t i = 0ul; i < sys::CXXMP_PROC_COUNT; ++i) {
        m_localQueuesMap[i]->run(this);
        // this must be called before accessing the queue(because it
        // initializes the queue tid and hid)
        m_localQueuesMapIds[i] = {
          m_localQueuesMap[i]->getTid(), m_localQueuesMap[i]->getHid()};
        m_localQueueMapIndex.insert({m_localQueuesMap[i]->getHid(), i});
    }
    m_globalQueue = std::make_unique< core::GlobalTaskQueue >();
}

Scheduler::~Scheduler() {
    waitForAllCompletion();
    log::debug("Scheduler destroyed");
    for (const auto& queue : m_localQueuesMap) {
        queue->shutdown();
    }
    if (!m_globalQueue->empty()) {
        log::error("Still task uncomplete");
    }
};

// move a task from the global queue front a specified local queue with
// index
bool Scheduler::moveGlobalToLocal(size_t idx) noexcept {
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

typing::Option< size_t > Scheduler::indexFromHid(Hid hid) noexcept {
    const auto it = m_localQueueMapIndex.find(hid);
    if (it != m_localQueueMapIndex.end()) {
        return it->second;
    }
    return typing::None;
}

void Scheduler::pause() noexcept {
    for (auto& queue : m_localQueuesMap) {
        queue->pause();
    }
}

void Scheduler::unpause() noexcept {
    for (auto& queue : m_localQueuesMap) {
        queue->unpause();
    }
}

void Scheduler::waitForAllCompletion() noexcept {
    bool allDone = false;
    for (auto& queue : m_localQueuesMap) {
        queue->startCompletion();
    }
    while (!allDone) {
        allDone = true;
        for (size_t i = 0; i < sys::CXXMP_PROC_COUNT; ++i) {
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

size_t Scheduler::fullLocalQueueCount() const noexcept {
    return m_fullLocalQueue.count();
}

void Scheduler::notifyQueueHasSpace(size_t hid) {
    auto indexOp = indexFromHid(hid);
    if (indexOp.has_value()) {
        size_t index = indexOp.value();
        m_fullLocalQueue.set(index, false);
        log::trace("Global Task moved to local queue {}", index);
        moveGlobalToLocal(index);
    }
}

} // namespace cxxmp
