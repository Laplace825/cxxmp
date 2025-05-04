#pragma once

#include <cstddef>
#include <numeric>
#include <thread>

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/timer.h"
#include "cxxmp/Core/scheduler.h"
#include "cxxmp/Core/task.h"

namespace test::scheduler {

/**
 * @brief: Check if our Scheduler could allocate tasks to different queues
 * to run in parallel
 */
static void testParallel() {
    using namespace std::chrono_literals;
    using namespace cxxmp;
    fmt::println("======== Testing Parallel =========");

    auto scheduler = Scheduler<>::build();
    scheduler->pause();

    const int tasksPerCore = 10; // Reasonable number for testing
    const int numCores     = scheduler->numCPUs();
    const int totalTasks   = numCores * tasksPerCore;

    std::atomic< int > completedTasks{0};

    fmt::println(
      "Starting parallel test with {} tasks on {} cores with local capacity {}",
      totalTasks, numCores, scheduler->getLocalCapcity(0));

    // Submit tasks that wait for the signal, then do work
    for (int i = 0; i < totalTasks; ++i) {
        scheduler->submit(core::Task::build([&completedTasks, i]() {
            // Do some actual work (sleeping simulates work)
            std::this_thread::sleep_for(50ms);

            // Mark task as complete
            completedTasks.fetch_add(1);

            fmt::println("Task {} completed", i);
        }));
    }

    // Make sure tasks are distributed to queues
    std::this_thread::sleep_for(100ms);

    // Check distribution
    for (size_t i = 0; i < scheduler->numCPUs(); ++i) {
        fmt::println("Local queue {} size: {}", i, scheduler->getLocalSize(i));
    }

    fmt::println("Starting execution...");

    int duration = 0;

    {
        common::RAIITimer t{};
        // Wait for all tasks to complete
        scheduler->waitForAllCompletion();
        duration = t.elapsed();
    }
    size_t sum = 0;
    for (size_t i = 0; i < scheduler->numCPUs(); ++i) {
        sum += scheduler->getLocalSize(i);
    }
    sum += scheduler->getGlobalSize();

    fmt::println("Starting execution...");

    fmt::println(R"(All {} tasks completed in {}ms
Theoretical sequential time: {}ms
Theoretical perfect parallel time: {}ms
Actual speedup: {:.2f}x
Actually Finished? {}
)",
      totalTasks, duration, 50 * totalTasks, 50 * tasksPerCore,
      (50.0 * totalTasks) / duration, sum == 0);
}

/**
 * @brief: A simple test to check if our scheduler has task loss
 * If the result is as expected, no task loss
 */
static void testSumming() {
    using namespace std::chrono_literals;
    using namespace cxxmp;
    fmt::println("====== Parallel Summing Test ======");

    auto scheduler = Scheduler<>::build();
    scheduler->pause();

    const int numCores = scheduler->numCPUs();

    std::atomic< int > summing{0};

    size_t capacity  = scheduler->getLocalCapcity(0);
    size_t totalTask = numCores * capacity * 2;

    fmt::println(R"(
With {} tasks
With {} cores
)",
      totalTask, numCores);

    // Submit tasks that wait for the signal, then do work
    for (int i = 0; i < totalTask; ++i) {
        scheduler->submit(core::Task::build([&summing, i]() { summing += 1; }));
    }

    {
        common::RAIITimer t{common::RAIITimer::Unit::Microseconds};
        scheduler->waitForAllCompletion();
    }

    fmt::println(
      "Result: {} Valid: {}\n", summing.load(), totalTask == summing.load());
}

/**
 * @brief: A test to check if our the local task queue could steal tasks
 * from other queues
 *
 * We deliberately submit all the tasks to the 0 local task queue
 */
static void testSteal() {
    using namespace std::chrono_literals;
    using namespace cxxmp;
    fmt::println("====== Parallel Steal Test ======");

    auto scheduler = Scheduler< 32 >::build();
    scheduler->pause();
    const size_t localCapacity0 = scheduler->getLocalCapcity(0);
    scheduler->setStealIntervalFor(4, 5000ms);
    scheduler->setStealIntervalFor(1, 5000ms);
    scheduler->setStealIntervalFor(2, 5000ms);
    const size_t totalTime = localCapacity0 * 100;

    std::atomic< int > cnt{0};

    // submit tasks to just one task queue
    // if sequential, the time will near `totalTime`
    // if task steal by other, the time must be less than `totalTime`

    for (size_t i = 0; i < localCapacity0; ++i) {
        scheduler->submit(0, core::Task::build([i, &cnt] {
            std::this_thread::sleep_for(100ms);
            log::info("Runned");
            cnt.fetch_add(1);
        }));
    }

    int duration = 0;

    scheduler->unpause();
    {
        common::RAIITimer t{};
        scheduler->waitForAllCompletion();
        duration = t.elapsed();
    }

    fmt::println("The result should be less than {}ms", totalTime);
    fmt::println("Result Valid: {}", totalTime > duration);
    fmt::println("Speed up: {:.2f}x\n", totalTime * 1.0 / duration);
    fmt::println("Total tasks: {} Completed tasks: {} Valid: {}",
      localCapacity0, cnt.load(), cnt.load() % localCapacity0 == 0);
}

/**
 * @brief: A mixed complex testing. Contains a vector normalization and work
 * steal.
 */
template < size_t NumLocalQueues = cxxmp::sys::CXXMP_PROC_COUNT >
static void mixed(
  cxxmp::typing::Rc< cxxmp::Scheduler< NumLocalQueues > > s = nullptr) {
    using namespace std::chrono_literals;
    using namespace cxxmp;
    fmt::println("====== Mixed complex testing ====== ");
    auto scheduler = (s != nullptr) ? s : Scheduler< NumLocalQueues >::build();
    scheduler->pause();
    const size_t localCapacity0 = scheduler->getLocalCapcity(0);
    const size_t numCPUs        = scheduler->numLocalQueues();
    const size_t totalTasks     = localCapacity0 * 2;

    const size_t vecSize = totalTasks * 10 + 3;

    // generate random numbers
    std::vector< double > v(vecSize, 0);
    std::generate(v.begin(), v.end(), [] { return rand() % 10; });

    using WorkArg = struct {
        size_t begin;
        size_t end;
    };

    size_t sizePerTask = vecSize / totalTasks;

    std::vector< WorkArg > workArgs(totalTasks);
    for (size_t i = 0; i < totalTasks; ++i) {
        workArgs[i].begin = i * sizePerTask;
        workArgs[i].end =
          (i != totalTasks - 1) ? (i + 1) * sizePerTask : vecSize;
    }

    double trueSum = std::accumulate(v.begin(), v.end(), 0,
      [](double sum, double value) { return sum + value * value; });

    std::vector< double > summing(totalTasks, 0);
    std::atomic< int > cnt{0};

    // submit tasks to just one task queue
    // if sequential, the time will near `totalTime`
    // if task steal by other, the time must be less than `totalTime`

    for (size_t i = 0; i < totalTasks; ++i) {
        scheduler->submit(
          0, core::Task::build([i, &cnt, &workArgs, &v, &summing] {
              std::this_thread::sleep_for(100ms);
              log::info("Runned");
              cnt.fetch_add(1);
              for (size_t j = workArgs[i].begin; j < workArgs[i].end; ++j) {
                  double data = v[j];
                  summing[i] += data * data;
              }

              if (i % 8 == 0) {
                  throw std::runtime_error(
                    fmt::format("Simulate Task[{}] failed", i));
              }
          }));
    }

    int duration{0};
    std::atomic< double > sum{0};

    {
        common::RAIITimer t{false};
        scheduler->waitForAllCompletion();
        for (size_t i = 0; i < totalTasks; ++i) {
            scheduler->submit(core::Task::build([i, &cnt, &summing, &sum] {
                cnt.fetch_add(1);
                sum += summing[i];
            }));
        }
        scheduler->waitForAllCompletion();
        const double sqrtSum = std::sqrt(sum.load());
        for (size_t i = 0; i < totalTasks; ++i) {
            scheduler->submit(
              numCPUs + 2, core::Task::build([i, &cnt, &workArgs, &v, sqrtSum] {
                  cnt.fetch_add(1);
                  log::info("Task[{}] started sqrt sum: {}", i, sqrtSum);
                  for (size_t j = workArgs[i].begin; j < workArgs[i].end; ++j) {
                      v[j] /= sqrtSum;
                  }
              }));
        }
        scheduler->waitForAllCompletion();
        duration = t.elapsed();
    }

    const size_t totalTime = totalTasks * 100;
    fmt::println("The result should be less than {}ms", totalTime);
    fmt::println("Run Time Valid: {}", totalTime > duration);
    fmt::println("Speed up: {:.2f}x", totalTime * 1.0 / duration);
    fmt::println("Total tasks: {} Completed tasks: {} Valid: {}",
      3 * totalTasks, cnt.load(), cnt == 3 * totalTasks);
    // get the square sum up of v
    double shouldBeOne = std::accumulate(v.cbegin(), v.cend(), 0.0,
      [](double sum, double x) { return sum + x * x; });
    fmt::println("Norm Vector Square Sum {} (should be ~1), Valid: {}",
      shouldBeOne, std::abs(shouldBeOne - 1.0) < 1e-6);
}

} // namespace test::scheduler
