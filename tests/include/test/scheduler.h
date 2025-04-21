#pragma once

#include "cxxmp/Common/log.h"
#include "cxxmp/Core/scheduler.h"

namespace test::scheduler {

using namespace cxxmp;

static void build() {
    log::cfg(log::level::trace);
    auto scheduler = Scheduler::build();
    log::info("Scheduler built with {} CPUs", scheduler->numCPUs());

    // auto scheduler2 = Scheduler();
    // log::info("Scheduler2 built with {} CPUs", scheduler2.numCPUs());
}

} // namespace test::scheduler
