#include "cxxmp/Utily/getsys.h"

#include "cxxmp/Common/typing.h"

#include <thread>

namespace cxxmp::sys {

// get the system logical cpus, equivalent to the hardware_concurrency
typing::u32 getSysCPUs() noexcept {
    return std::thread::hardware_concurrency();
}

} // namespace cxxmp::sys
