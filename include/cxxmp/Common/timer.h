#pragma once

#include <fmt/core.h>

#include <chrono>
#include <string_view>

namespace cxxmp::common {

class RAIITimer {
  private:
    constexpr std::string_view unitToString() {
        switch (m_unit) {
            case Unit::Seconds:
                return "s";
            case Unit::Milliseconds:
                return "ms";
            case Unit::Microseconds:
                return "us";
            case Unit::Nanoseconds:
                return "ns";
        }
    }

  public:
    enum class Unit { Seconds, Milliseconds, Microseconds, Nanoseconds };

    /**
     * @param: `unit`: the time record unit to use
     *
     * coule be found in the enum class `RAIITimer::Unit`
     *
     * @param: `enablePrint`: should the `~RAIITimer` print the duration
     */
    [[nodiscard("RAIITimer should not be ignore because the rvalue will "
                "immediately destroy")]]
    RAIITimer(Unit unit = Unit::Milliseconds, bool enablePrint = true)
        : m_enablePrint(enablePrint), m_unit(unit),
          m_start_time(std::chrono::steady_clock::now()) {}

    [[nodiscard("RAIITimer should not be ignore because the rvalue will "
                "immediately destroy")]]
    RAIITimer(bool enablePrint)
        : m_enablePrint(enablePrint), m_unit(Unit::Milliseconds),
          m_start_time(std::chrono::steady_clock::now()) {}

    auto elapsed() const {
        auto end_time = std::chrono::steady_clock::now();
        switch (m_unit) {
            case Unit::Seconds:
                return std::chrono::duration_cast< std::chrono::seconds >(
                  end_time - m_start_time)
                  .count();
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
        if (m_enablePrint) {
            fmt::println("RAIITimer Destroy. Total Duration: {}{}", elapsed(),
              unitToString());
        }
    }

  private:
    std::chrono::steady_clock::time_point m_start_time;
    Unit m_unit;
    bool m_enablePrint{true};
};

} // namespace cxxmp::common
