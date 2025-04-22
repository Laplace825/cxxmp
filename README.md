## CXXMP -- A Mutil-Thread Processers Manage For you

Just `submit` any of your task, and your task will be runned in Parallel. 
**This may not be that stable for you to use in official development. 
Just an Implenmentation for A GMP inspired(ps. not that great though)**

### Versions

Because the `concept` used, minimum C++ version should be over `C++ 20`.

### Dependencies

`spdlog` is only for this to logging. And I use `libfmt` to print testing.
Actually these are removable because there are not effecting the main framework.

### Example

```cpp

using namespace std::chrono_literals;
fmt::println("====== Parallel Summing Test ======");

// the threads automatically runned in background after build
// but in a waiting state (not busy waiting).
auto scheduler = cxxmp::Scheduler::build();

// pause executing 
scheduler->pause();

const int numCores = scheduler->numCPUs();

std::atomic< int > summing{0};

// get local task queue capacity through index, though all the local task queue's are the same
size_t capacity  = scheduler->getLocalCapcity(0);
size_t totalTask = numCores * capacity * 2;

fmt::println(R"(
With {} tasks
With {} cores
)",
  totalTask, numCores);

// Submit tasks, I recommend to using lambda because of capture feature
for (int i = 0; i < totalTask; ++i) {
    scheduler->submit(core::Task::build([&summing]() { summing += 1; }));
}

{
    // a RAII Timer (destroyed and print the duration with libfmt)
    cxxmp::core::RAIITimer t{cxxmp::core::RAIITimer::Unit::Microseconds};

    // block there and wait for all tasks to complete
    // like `join`
    scheduler->waitForAllCompletion();
}

fmt::println(
  "Result: {} Valid: {}\n", summing.load(), totalTask == summing.load());
```

### Details About this Work

1. Abstract the logic cpu core to a Thread
2. For each Thread, we made it like a "local task queue" which be treated as a logic cpu core
  - Each task just waiting for runned
  - Each "local task queue" could be pause or "unpause"
  - Each "local task queue" has their own capacity (now just 32 * Number of logic CPU Cores)
3. Using a "global task queue" to store the "overflowed" tasks (when all "local task queue" are not able to store)
4. "local task queue" will tell "global task queue" to give a task to run whenever "local task queue" has space.
