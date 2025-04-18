#include "cxxmp/Common/log.h"

#include "config.h"
#include "cxxmp/Common/typing.h"

namespace cxxmp::log {

void cfg(typing::Option< level > level) noexcept {
    set_pattern("[P %P] [T %t] [%Y %T] [%^%l%$] %v");
    auto SPDLOG_LEVEL = level::debug;
    if (level == ::std::nullopt) {
        if (::cxxmp::versions::BUILD_TYPE == "Release") {
            SPDLOG_LEVEL = level::warn;
        }
        ::spdlog::set_level(SPDLOG_LEVEL);
    }
    else {
        ::spdlog::set_level(level.value());
    }
}

} // namespace cxxmp::log
