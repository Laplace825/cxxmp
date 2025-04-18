#pragma once

#include "cxxmp/Common/typing.h"

namespace cxxmp::sys {

// get the system logical cpus, equivalent to the hardware_concurrency
typing::u32 getSysCPUs() noexcept;

} // namespace cxxmp::sys
