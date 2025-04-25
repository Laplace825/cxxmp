#include "cxxmp/Common/log.h"
#include "test/localTaskQueue.h"
#include "test/scheduler.h"

using namespace cxxmp;

int main(int argc, char* argv[]) {
    log::cfg(log::level::info);
    // test::ltq::mutipleTask();
    // test::ltq::rightPause();
    // test::ltq::runAllPerformanceTests();
    // test::ltq::moveConstruct();
    test::scheduler::testParallel();
    test::scheduler::testSumming();
    test::scheduler::testSteal();
    log::info("SUCCESSFULLY RUN");
    return 0;
}
