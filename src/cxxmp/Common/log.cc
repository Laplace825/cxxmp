#include "cxxmp/Common/log.h"

#include "cxxmp/Common/typing.h"
#include "cxxmp/config.h"

namespace cxxmp::log {

void cfg(typing::Option< level > level) noexcept {
    set_pattern("[%^%l%$] [P %P] [T %t] [%Y %T] %v");
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
    ::spdlog::cfg::load_env_levels();
}

} // namespace cxxmp::log
