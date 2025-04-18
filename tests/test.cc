#include "cxxmp/Common/log.h"
#include "cxxmp/Core/local.h"
#include "cxxmp/Core/lru.h"
#include "cxxmp/Core/task.h"
#include "cxxmp/Utily/getsys.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <deque>
#include <iostream>
#include <thread>

using namespace cxxmp;

void test_lru() {
    LRU< int, int > lru(3);
    lru.put(1, 1);
    lru.put(2, 2);
    lru.put(3, 3);
    auto [ok, res] = lru.get(2);
    log::info("Get 2: {} {}", ok, res);
    lru.put(4, 4);
    log::info("Put 4: {} {}", 4, 4);
    auto [ok2, res2] = lru.get(1);
    log::info("Get 1: {} {}", ok2, res2);
    auto [ok3, res3] = lru.get(3);
    log::info("Get 3: {} {}", ok3, res3);
}

void test_task(bool mtxOrNot = false) {
    int ret = 0;
    // Shared counter without synchronization
    int shared_counter = 0;

    // Vector to record intermediate values - will show evidence of race
    // condition
    std::vector< int > recorded_values;
    std::mutex vec_mutex; // Only protect the vector, not the counter
    std::mutex mtx;

    constexpr size_t tasksNumber = 1000;

    std::array< LocalTaskQueue, 8 > ltqs{LocalTaskQueue(1000),
      LocalTaskQueue(tasksNumber), LocalTaskQueue(1000), LocalTaskQueue(1000),
      LocalTaskQueue(tasksNumber), LocalTaskQueue(1000), LocalTaskQueue(1000),
      LocalTaskQueue(tasksNumber)};

    for (auto& ltq : ltqs) {
        ltq.run();
        log::info("LocalTaskQueue {} started", ltq.getHid());
    }
    // Submit a large number of tasks to each queue
    for (size_t q = 0; q < ltqs.size(); q++) {
        for (size_t i = 0; i < ltqs[q].capacity(); i++) {
            ltqs[q].submit(Task::build([&shared_counter, &recorded_values,
                                         &vec_mutex, &mtx, q, i, mtxOrNot]() {
                if (mtxOrNot) {
                    std::lock_guard< std::mutex > lock(mtx);
                    // Read current value
                    int current = shared_counter;
                    // Deliberately create a situation where race conditions
                    // can occur by introducing a small sleep between read and
                    // write
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    // write back incremented value
                    shared_counter = current + 1;
                }
                else {
                    int current = shared_counter;
                    // Deliberately create a situation where race conditions
                    // can occur by introducing a small sleep between read and
                    // write
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    // write back incremented value
                    shared_counter = current + 1;
                }

                // Periodically record the value (only capture every 100th
                // update)
                if (i % 100 == 0) {
                    std::lock_guard< std::mutex > lock(vec_mutex);
                    recorded_values.push_back(shared_counter);

                    // The following will log evidence of out-of-order
                    // execution
                    if (recorded_values.size() > 1 &&
                        recorded_values[recorded_values.size() - 1] <=
                          recorded_values[recorded_values.size() - 2])
                    {
                        log::info("Race detected: Queue {} saw value {} <= "
                                  "previous {}",
                          q, shared_counter,
                          recorded_values[recorded_values.size() - 2]);
                    }
                }
            }));
        }
        ltqs[q % 2].pause();
    }

    // Wait for completion
    for (auto& ltq : ltqs) {
        ltq.waitForCompletion();
        log::info("ltq: {}", ltq.getHid());
    }
    for (auto& ltq : ltqs) {
        ltq.shutdown();
    }
    // Final check
    log::info("Expected final count: 8000, Actual count: {}, Matched?: {}",
      shared_counter, shared_counter == 8000);
    log::info("If these numbers don't match, a race condition occurred!");

    // Check for non-monotonic values in the recorded sequence
    bool found_race = false;
    for (size_t i = 1; i < recorded_values.size(); i++) {
        if (recorded_values[i] <= recorded_values[i - 1]) {
            found_race = true;
            log::info("Race evidence: {} <= {} at positions {} and {}",
              recorded_values[i], recorded_values[i - 1], i, i - 1);
        }
    }

    if (found_race) {
        log::info("Race conditions confirmed - execution is parallel!");
    }
    else {
        log::info("No clear race condition evidence found. Try increasing "
                  "task count or sleep duration.");
    }

    for (auto& ltq : ltqs) {
        log::info("LocalTaskQueue size: {}", ltq.size());
    }
}

void test_pause() {
    using namespace std::chrono_literals;
    log::info("Starting pause functionality test");

    // Create a local task queue
    LocalTaskQueue ltq(20);
    ltq.run();

    // A shared counter to verify tasks are executed
    std::atomic< int > counter             = 0;
    std::atomic< bool > long_task_started  = false;
    std::atomic< bool > long_task_finished = false;

    // Add some initial quick tasks
    for (int i = 0; i < 5; i++) {
        ltq.submit(Task::build([&counter, i]() {
            log::info("Quick task {} executing", i);
            ++counter;
            std::this_thread::sleep_for(50ms);
            log::info(
              "Quick task {} completed, Counter: {}", i, counter.load());
        }));
    }

    // Wait a bit to let some tasks start executing
    std::this_thread::sleep_for(100ms);

    // Add a long-running task
    ltq.submit(
      Task::build([&counter, &long_task_started, &long_task_finished]() {
          log::info("Long task starting");
          long_task_started = true;

          // Simulate long work
          for (int i = 0; i < 5; i++) {
              std::this_thread::sleep_for(200ms);
              log::info("Long task still running...");
          }

          ++counter;
          long_task_finished = true;
          log::info("Long task completed, Counter: {}", counter.load());
      }));

    // Wait for the long task to start
    while (!long_task_started) {
        std::this_thread::sleep_for(10ms);
    }

    // Pause the queue - this should let the current task finish but pause new
    // ones
    log::info("Pausing the queue...");
    ltq.pause();

    // Add more tasks that should be paused
    for (int i = 0; i < 5; i++) {
        ltq.submit(Task::build([&counter, i]() {
            log::info(
              "Post-pause task {} executing, Counter: {}", i, counter.load());
            ++counter;
            std::this_thread::sleep_for(50ms);
            log::info(
              "Post-pause task {} completed, Counter: {}", i, counter.load());
        }));
    }

    // Wait for the long-running task to finish (it should continue despite the
    // pause)
    while (!long_task_finished) {
        log::info("Waiting for long task to finish...");
        std::this_thread::sleep_for(10ms);
    }

    // Check counter value after pause
    int counter_after_pause = counter.load();
    log::info("Counter after pause: {}", counter_after_pause);

    // The counter should be around 6 (5 quick tasks + 1 long task)
    // but could be less if some quick tasks were still in queue when paused

    // Verify queue is actually paused by waiting a bit and checking if counter
    // changes
    std::this_thread::sleep_for(500ms);
    int counter_check = counter.load();

    if (counter_check == counter_after_pause) {
        log::info("Queue is correctly paused - counter didn't change");
    }
    else {
        log::error("Queue might not be paused! Counter changed from {} to {}",
          counter_after_pause, counter_check);
    }

    // Now unpause the queue
    log::info("Unpausing the queue...");
    ltq.unpause();

    // Wait for all remaining tasks to complete
    std::this_thread::sleep_for(500ms);

    // Final counter check - should be 11 (5 quick + 1 long + 5 post-pause)
    log::info("Final counter: {}", counter.load());

    // Verify all tasks have been processed
    ltq.waitForCompletion();

    if (counter.load() == 11) {
        log::info("Pause/unpause test PASSED - all tasks executed");
    }
    else {
        log::error("Pause/unpause test FAILED - expected 11 tasks, got {}",
          counter.load());
    }

    log::info("Pause functionality test completed");
}

int main(int argc, char* argv[]) {
    // log::cfg(log::level::info);
    // test_lru();
    test_task(false);
    test_pause();
    log::info("SUCCESSFULLY RUN");
    return 0;
}
