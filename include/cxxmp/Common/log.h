#pragma once

#include "cxxmp/Common/typing.h"

#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

namespace cxxmp::log {

using namespace ::spdlog;
using level = ::spdlog::level::level_enum;

/**
 * @brief: cfg() is used to configure the ::spdlog
 * it sets the pattern and level of the ::spdlog
 * if call this function, the env variable to set level will be ignore
 */
void cfg(typing::Option< level > level = ::std::nullopt) noexcept;

static auto _ = [] {
    ::spdlog::cfg::load_env_levels();
    return true;
}();

} // namespace cxxmp::log
