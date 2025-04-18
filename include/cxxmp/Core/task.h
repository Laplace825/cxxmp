#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The Task class is a placeholder for the task system.
 * Each task is a unit of work that can be executed in parallel.
 * The task system is responsible for managing the execution of tasks,
 *
 * The task system is designed to be lightweight and efficient,
 */

#include "cxxmp/Common/typing.h"

#include <functional>
#include <memory>
#include <string_view>

namespace cxxmp {

/**
 * @brief: The Task class is a placeholder for the task system.
 * Each task is a unit of work that can be executed in parallel.
 *
 * `Task` it self could be runned in async like `pause` `stop` `execute`
 */
class Task {
  public:
    enum class State {
        Created,
        Executing,
        Completed,
    };

    static constexpr std::string_view state2String(State state) {
#define Fn(stateName)      \
    case State::stateName: \
        return "Task::State::" #stateName;

        switch (state) {
            Fn(Created);
            Fn(Executing);
            Fn(Completed);
            default:
                return "Unknown";
        }
#undef Fn
    }

    using Fn = std::function< void() >;

    Task() : m_fn{nullptr} {}

    ~Task() = default;

    Task(const Task& other);
    Task(Task&& other) noexcept;

    template < typing::Callable CallableFn >
    explicit Task(CallableFn&& fn)
        : m_fn{std::make_unique< Fn >(std::forward< Fn >(fn))} {}

    // when the task could be runned
    constexpr bool executable() const noexcept { return this->m_fn != nullptr; }

    // when the task could be executed, execute the task
    void execute();

    State getState() const noexcept { return m_state; }

    template < typing::Callable CallableFn >
    static typing::Box< Task > build(CallableFn&& fn) {
        return std::make_unique< Task >(std::forward< Fn >(fn));
    }

  private:
    /**
     * @brief: The Task function
     * The function should be a callable object that takes no arguments and
     * returns void
     * So if user want to define a complex function with arguments and returns,
     * they should use a lambda function and make their complex function inside
     * and use a captured variable to get returns
     *
     * Example:
     * ```
     * int sum = 0;
     * std::vector<int> vec = {1, 2, 3, 4, 5};
     *
     * Task task = Task([&sum, &vec]() {
     *     for (int i = 0; i < vec.size(); ++i) {
     *         sum += vec[i];
     *     }
     *  });
     *
     *
     *  // a user defined function
     *  int add(int a, int b);
     *  int result = 0;
     *
     *  Task task = Task([&result]() {
     *      result = add(1, 2);
     *  });
     *
     * ```
     */
    typing::Box< Fn > m_fn;
    State m_state{State::Created};
};

using TaskPtr     = typing::Box< Task >;
using RcTaskPtr   = typing::Rc< Task >;
using WeakTaskPtr = typing::Weak< Task >;

} // namespace cxxmp
