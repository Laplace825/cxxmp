#pragma once

#include "cxxmp/Core/task.h"
#include "cxxmp/Core/taskQueue.h"

#include <algorithm>
#include <random>
#include <thread>
#include <vector>

namespace test::ltq {

using namespace cxxmp;
using namespace cxxmp::core;

static void mutipleTask(bool mtxOrNot = false) {
    int ret = 0;
    // Shared counter without synchronization
    int shared_counter = 0;
    // Vector to record intermediate values - will show evidence of race
    // condition
    std::vector< int > recorded_values;
    std::mutex vec_mutex; // Only protect the vector, not the counter
    std::mutex mtx;

    constexpr size_t tasksNumber = 1000;

    std::array< LocalTaskQueue, 8 > ltqs{
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
      LocalTaskQueue(tasksNumber),
    };

    for (auto& ltq : ltqs) {
        ltq.run();
        log::info("LocalTaskQueue {} started", ltq.getHid());
    }
    // Submit a large number of tasks to each queue
    for (size_t q = 0; q < ltqs.size(); q++) {
        for (size_t i = 0; i < ltqs[q].getCapacity(); i++) {
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
        log::info("LocalTaskQueue size: {}", ltq.getSize());
    }
}

static bool rightPause() {
    bool success = false;
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
        bool success = true;
        log::info("Queue is correctly paused - counter didn't change");
    }
    else {
        bool success = false;
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
    return success;
}

namespace chr = std::chrono;

// Structure to hold performance metrics
struct PerformanceMetrics {
    std::vector< int > results;
    double total_time_ms;    // Total execution time in milliseconds
    double tasks_per_second; // Throughput
    double avg_task_time_ms; // Average time per task
    double speedup;          // Parallel speedup vs sequential
    double efficiency;       // Speedup / number of threads
};

static bool judgeResults(
  const PerformanceMetrics& lhs, const PerformanceMetrics& rhs) {
    for (size_t i = 0; i < lhs.results.size(); ++i) {
        if (lhs.results[i] != rhs.results[i]) {
            return false;
        }
    }
    return true;
}

// Function to print metrics in a nice format
static void printMetrics(
  const std::string& test_name, const PerformanceMetrics& metrics) {
    log::fmt_lib::println("==== {} ====", test_name);
    log::fmt_lib::println(
      "Total execution time: {:.2f} ms", metrics.total_time_ms);
    log::fmt_lib::println("Tasks per second: {:.2f}", metrics.tasks_per_second);
    log::fmt_lib::println(
      "Average task time: {:.2f} ms", metrics.avg_task_time_ms);
    log::fmt_lib::println("Speedup: {:.2f}x", metrics.speedup);
    log::fmt_lib::println(
      "Parallel efficiency: {:.2f}%", metrics.efficiency * 100);
}

// Define different workload types for testing
enum class WorkloadType {
    CPU_BOUND,    // Heavy computation (e.g., prime checking)
    MEMORY_BOUND, // Memory intensive (e.g., vector operations)
    IO_BOUND,     // Simulated I/O (e.g., sleep)
    MIXED         // Mixture of the above
};

// Create a task function based on workload type and duration
static std::function< void() > createTaskFn(
  WorkloadType type, int complexity, int& ret) {
    switch (type) {
        case WorkloadType::CPU_BOUND: {
            // CPU bound - prime number checking
            return [complexity, &ret]() {
                int count = 0;
                for (int n = 2; n < complexity * 1000; ++n) {
                    bool is_prime = true;
                    for (int i = 2; i <= std::sqrt(n); ++i) {
                        if (n % i == 0) {
                            is_prime = false;
                            break;
                        }
                    }
                    if (is_prime) count++;
                }
                ret = count;
                // Use the result to prevent compiler optimization
                if (count < 0) {
                    log::error("Unexpected count");
                }
            };
        }

        case WorkloadType::MEMORY_BOUND: {
            // Memory bound - vector operations
            return [complexity]() {
                std::vector< int > data(complexity * 10000);
                std::iota(
                  data.begin(), data.end(), 0); // Fill with incrementing values

                // Shuffle the vector
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(data.begin(), data.end(), g);

                // Sort it
                std::sort(data.begin(), data.end());

                // Use result to prevent compiler optimization
                if (data[0] > data[data.size() - 1])
                    log::error("Unexpected sort");
            };
        }

        case WorkloadType::IO_BOUND: {
            // I/O bound - simulated with sleep
            return [complexity]() {
                std::this_thread::sleep_for(
                  std::chrono::milliseconds(complexity));
            };
        }

        case WorkloadType::MIXED: {
            // Mixed workload
            return [complexity, &ret]() {
                // Some CPU work
                int sum = 0;
                for (int i = 0; i < complexity * 5000; ++i) {
                    sum += i;
                }
                ret = sum;

                // Some memory work
                std::vector< int > data(complexity * 1000);
                for (int i = 0; i < data.size(); ++i) {
                    data[i] = sum % (i + 1);
                }

                // Some I/O simulation
                std::this_thread::sleep_for(
                  std::chrono::milliseconds(complexity / 10));

                // Use results
                if (sum < 0 || data[0] < 0) log::error("Unexpected result");
            };
        }
    }

    return []() {}; // Default empty function
}

// Run tasks sequentially and measure time
static PerformanceMetrics runSequentialTest(
  WorkloadType type, int num_tasks, int complexity) {
    std::vector< std::function< void() > > tasks;
    tasks.reserve(num_tasks);
    std::vector< int > results(num_tasks);

    // Create tasks
    for (int i = 0; i < num_tasks; ++i) {
        tasks.push_back(createTaskFn(type, complexity, results[i]));
    }

    // Execute tasks sequentially and measure time
    auto start = chr::high_resolution_clock::now();

    for (auto& task : tasks) {
        task();
    }

    auto end = chr::high_resolution_clock::now();
    double elapsed_ms =
      chr::duration_cast< chr::milliseconds >(end - start).count();

    // Calculate metrics
    PerformanceMetrics metrics;
    metrics.results          = std::move(results);
    metrics.total_time_ms    = elapsed_ms;
    metrics.tasks_per_second = (num_tasks * 1000.0) / elapsed_ms;
    metrics.avg_task_time_ms = elapsed_ms / num_tasks;
    metrics.speedup          = 1.0; // By definition
    metrics.efficiency       = 1.0; // By definition

    return metrics;
}

// Run tasks using LocalTaskQueue and measure time
static PerformanceMetrics runParallelTest(WorkloadType type, int num_tasks,
  int complexity, int num_queues = sys::getSysCPUs()) {
    // Create task queues
    std::vector< LocalTaskQueue > queues;
    queues.reserve(num_queues);
    std::vector< int > results(num_tasks);

    for (int i = 0; i < num_queues; ++i) {
        queues.emplace_back(
          num_tasks / num_queues + 1); // Distribute tasks evenly
        queues[i].run();
    }

    // Create and submit tasks
    auto start = chr::high_resolution_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
        auto task_fn = createTaskFn(type, complexity, results[i]);
        // Round-robin task distribution
        queues[i % num_queues].submit(Task::build(task_fn));
    }

    // Wait for all tasks to complete
    for (auto& q : queues) {
        q.waitForCompletion();
    }

    auto end = chr::high_resolution_clock::now();
    double elapsed_ms =
      chr::duration_cast< chr::milliseconds >(end - start).count();

    // Calculate metrics
    PerformanceMetrics metrics;
    metrics.results          = std::move(results);
    metrics.total_time_ms    = elapsed_ms;
    metrics.tasks_per_second = (num_tasks * 1000.0) / elapsed_ms;
    metrics.avg_task_time_ms = elapsed_ms / num_tasks;

    // These will be filled in by the comparison function
    metrics.speedup    = 0.0;
    metrics.efficiency = 0.0;

    return metrics;
}

// Run a comprehensive test comparing sequential vs parallel
static bool runPerformanceTest(
  WorkloadType type, int num_tasks, int complexity) {
    std::string type_name;
    bool all_valid = true;
    switch (type) {
        case WorkloadType::CPU_BOUND:
            type_name = "CPU-Bound";
            break;
        case WorkloadType::MEMORY_BOUND:
            type_name = "Memory-Bound";
            break;
        case WorkloadType::IO_BOUND:
            type_name = "IO-Bound";
            break;
        case WorkloadType::MIXED:
            type_name = "Mixed";
            break;
    }

    log::fmt_lib::print("\n========================================\n");
    log::fmt_lib::print("Performance Test: {}\n", type_name);
    log::fmt_lib::print("Tasks: {}\n", num_tasks);
    log::fmt_lib::print("Complexity: {}\n", complexity);
    log::fmt_lib::print("CPU Cores: {}\n", sys::getSysCPUs());
    log::fmt_lib::print("========================================\n\n");

    // Run sequential test
    PerformanceMetrics seq_metrics =
      runSequentialTest(type, num_tasks, complexity);
    printMetrics("Sequential Execution", seq_metrics);

    // Run parallel test with 1 queue
    PerformanceMetrics par1_metrics =
      runParallelTest(type, num_tasks, complexity, 1);
    par1_metrics.speedup =
      seq_metrics.total_time_ms / par1_metrics.total_time_ms;
    par1_metrics.efficiency = par1_metrics.speedup / 1.0;
    printMetrics("Parallel Execution (1 queue)", par1_metrics);
    all_valid &= judgeResults(seq_metrics, par1_metrics);
    log::fmt_lib::println("Result Valid: {}\n", all_valid);

    // Run parallel test with half the available cores
    int half_cores = std::max(1ul, sys::getSysCPUs() / 2);
    PerformanceMetrics par_half_metrics =
      runParallelTest(type, num_tasks, complexity, half_cores);
    par_half_metrics.speedup =
      seq_metrics.total_time_ms / par_half_metrics.total_time_ms;
    par_half_metrics.efficiency = par_half_metrics.speedup / half_cores;
    printMetrics(
      "Parallel Execution (" + std::to_string(half_cores) + " queues)",
      par_half_metrics);
    all_valid &= judgeResults(seq_metrics, par_half_metrics);
    log::fmt_lib::println("Result Valid: {}\n", all_valid);

    // Run parallel test with all available cores
    PerformanceMetrics par_all_metrics =
      runParallelTest(type, num_tasks, complexity);
    par_all_metrics.speedup =
      seq_metrics.total_time_ms / par_all_metrics.total_time_ms;
    par_all_metrics.efficiency = par_all_metrics.speedup / sys::getSysCPUs();
    printMetrics("Parallel Execution (all " +
                   std::to_string(sys::getSysCPUs()) + " cores)",
      par_all_metrics);
    all_valid &= judgeResults(seq_metrics, par_all_metrics);
    log::fmt_lib::println("Result Valid: {}\n", all_valid);

    // Run parallel test with 2x available cores to test oversubscription
    int double_cores = sys::getSysCPUs() * 2;
    PerformanceMetrics par_over_metrics =
      runParallelTest(type, num_tasks, complexity, double_cores);
    par_over_metrics.speedup =
      seq_metrics.total_time_ms / par_over_metrics.total_time_ms;
    par_over_metrics.efficiency = par_over_metrics.speedup / double_cores;
    printMetrics("Parallel Execution (oversubscribed: " +
                   std::to_string(double_cores) + " queues)",
      par_over_metrics);
    all_valid &= judgeResults(seq_metrics, par_over_metrics);
    log::fmt_lib::println("Result Valid: {}\n", all_valid);
    return all_valid;
}

// Main test function
static void runAllPerformanceTests() {
    bool all_valid = true;

    // Test CPU-bound workloads
    all_valid &=
      runPerformanceTest(WorkloadType::CPU_BOUND, 1000, 10); // Many small tasks
    all_valid &= runPerformanceTest(
      WorkloadType::CPU_BOUND, 100, 100); // Fewer larger tasks

    // Test memory-bound workloads
    all_valid &= runPerformanceTest(WorkloadType::MEMORY_BOUND, 100, 5);

    // Test IO-bound workloads
    all_valid &= runPerformanceTest(
      WorkloadType::IO_BOUND, 500, 20); // 20ms simulated I/O per task

    // Test mixed workloads
    all_valid &= runPerformanceTest(WorkloadType::MIXED, 200, 50);

    if (all_valid) {
        log::fmt_lib::println(
          R"(
==============================
ALL TESTS PASSED!
==============================
)"

        );
    }
    else {
        log::fmt_lib::println(
          R"(
==============================
SOME TESTS FAILED!
==============================
)"

        );
    }
}

static void moveConstruct() {
    using namespace std::chrono_literals;
    // testing the move constructor works
    auto ltq = LocalTaskQueue();
    ltq.run();
    for (size_t i = 0; i < 10; ++i) {
        ltq.submit(Task::build([i] {
            log::info("Task {} start", i);
            // some work
            std::this_thread::sleep_for(50ms);
            log::info("Task {} done", i);
        }));
    }

    std::this_thread::sleep_for(250ms);

    ltq.pause();

    auto moved_ltq = std::move(ltq);
    moved_ltq.unpause();

    std::this_thread::sleep_for(250ms);
    // moved_ltq.waitForCompletion();
    // moved_ltq.shutdown();
}

} // namespace test::ltq
