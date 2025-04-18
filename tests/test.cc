#include "cxxmp/Common/log.h"
#include "test/localTaskQueue.h"


using namespace cxxmp;

int main(int argc, char* argv[]) {
    // log::cfg(log::level::info);
    // test_lru();
    // test::ltq::mutipleTask();
    // test::ltq::rightPause();
    test::ltq::run_all_performance_tests();
    log::info("SUCCESSFULLY RUN");
    return 0;
}
