#pragma once

#include "cxxmp/Common/log.h"
#include "cxxmp/Common/timer.h"
#include "cxxmp/Core/scheduler.h"
#include "cxxmp/Core/task.h"

#include <thread>

namespace test::scheduler {

using namespace cxxmp;

static void build() {
    log::cfg(log::level::trace);
    auto scheduler = Scheduler::build();

    // auto scheduler2 = Scheduler();
    // log::info("Scheduler2 built with {} CPUs", scheduler2.numCPUs());
}

static void testParallel() {
    using namespace std::chrono_literals;
    fmt::println("======== Testing Parallel =========");

    auto scheduler = Scheduler::build();
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

static void testSumming() {
    using namespace std::chrono_literals;
    fmt::println("====== Parallel Summing Test ======");

    auto scheduler = Scheduler::build();
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

static void testSteal() {
    using namespace std::chrono_literals;
    fmt::println("====== Parallel Steal Test ======");

    auto scheduler = Scheduler::build();
    scheduler->pause();
    const size_t localCapacity0 = scheduler->getLocalCapcity(0);
    const size_t numCPUs        = scheduler->numCPUs();
    const size_t totalTime      = localCapacity0 * 100;

    // submit tasks to just one task queue
    // if sequential, the time will near `totalTime`
    // if task steal by other, the time must be less than `totalTime`

    for (size_t i = 0; i < localCapacity0; ++i) {
        scheduler->submit(core::Task::build([i] {
            std::this_thread::sleep_for(100ms);
            log::info("Runned");
        }),
          0);
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
    fmt::println("Speed up: {:.2f}\n", totalTime * 1.0 / duration);
}

} // namespace test::scheduler
