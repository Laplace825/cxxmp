#pragma once

#include "cxxmp/config.h"

namespace cxxmp::sys {

// get the system logical cpus, equivalent to the hardware_concurrency
consteval size_t getSysCPUs() noexcept { return CXXMP_PROC_COUNT; }

} // namespace cxxmp::sys
