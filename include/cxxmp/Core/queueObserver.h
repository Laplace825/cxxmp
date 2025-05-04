#pragma once

#include <cstddef>

namespace cxxmp {

class TaskQueueObserver {
  public:
    ~TaskQueueObserver() = default;
    void notifyQueueHasSpace(size_t hid) {};
};

} // namespace cxxmp
