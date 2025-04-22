#pragma once

#include <cstddef>

namespace cxxmp {

class TaskQueueObserver {
  public:
    virtual ~TaskQueueObserver()                 = default;
    virtual void notifyQueueHasSpace(size_t hid) = 0;
};

} // namespace cxxmp
