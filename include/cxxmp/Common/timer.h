#pragma once

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/typing.h"

#include <chrono>

namespace cxxmp::common {

class RAIITimer {
  public:
    enum class Unit { Milliseconds, Microseconds, Nanoseconds };

    RAIITimer(Unit unit = Unit::Milliseconds) {
        m_start_time = std::chrono::steady_clock::now();
        m_unit       = unit;
    }

    auto elapsed() const noexcept {
        auto end_time = std::chrono::steady_clock::now();
        switch (m_unit) {
            case Unit::Milliseconds:
                return std::chrono::duration_cast< std::chrono::milliseconds >(
                  end_time - m_start_time)
                  .count();
            case Unit::Microseconds:
                return std::chrono::duration_cast< std::chrono::microseconds >(
                  end_time - m_start_time)
                  .count();
            case Unit::Nanoseconds:
                return std::chrono::duration_cast< std::chrono::nanoseconds >(
                  end_time - m_start_time)
                  .count();
        }
    }

    ~RAIITimer() {
        auto end_time = std::chrono::steady_clock::now();
        switch (m_unit) {
            case Unit::Milliseconds:
                fmt::println("RAIITimer Destroy. Total duration: {} ms",
                  std::chrono::duration_cast< std::chrono::milliseconds >(
                    end_time - m_start_time)
                    .count());
                break;
            case Unit::Microseconds:
                fmt::println("RAIITimer Destroy. Total duration: {} us",
                  std::chrono::duration_cast< std::chrono::microseconds >(
                    end_time - m_start_time)
                    .count());
                break;
            case Unit::Nanoseconds:
                fmt::println("RAIITimer Destroy. Total duration: {} ns",
                  std::chrono::duration_cast< std::chrono::nanoseconds >(
                    end_time - m_start_time)
                    .count());
                break;
        }
    }

  private:
    std::chrono::steady_clock::time_point m_start_time;
    Unit m_unit;
};

} // namespace cxxmp::common
