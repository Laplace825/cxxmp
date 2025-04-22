#include "cxxmp/Common/log.h"
#include "test/localTaskQueue.h"
#include "test/scheduler.h"

using namespace cxxmp;

int main(int argc, char* argv[]) {
    log::cfg(log::level::info);
    // test_lru();
    // test::ltq::mutipleTask();
    // test::ltq::rightPause();
    // test::ltq::runAllPerformanceTests();
    // test::ltq::moveConstruct();
    // test::scheduler::build();
    test::scheduler::testParallel();
    test::scheduler::testSumming();
    log::info("SUCCESSFULLY RUN");
    return 0;
}
