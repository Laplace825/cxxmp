#pragma once

/**
 * cxxmp: C++ Multi-Processing (GMP inspired)
 *
 * The Task class is a placeholder for the task system.
 * Each task is a unit of work that can be executed in parallel.
 * The task system is an abstarct of execution of tasks
 *
 */

#include <functional>
#include <memory>

#include "cxxmp/Common/typing.h"

namespace cxxmp::core {

class Task {
  public:
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

    template < typing::Callable CallableFn >
    static typing::Rc< Task > build(CallableFn&& fn) {
        return std::make_shared< Task >(std::forward< Fn >(fn));
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
};

using TaskPtr     = typing::Box< Task >;
using RcTaskPtr   = typing::Rc< Task >;
using WeakTaskPtr = typing::Weak< Task >;

template < typename TaskType >
concept isValidTask = ::std::is_same_v< ::std::decay_t< TaskType >, Task > ||
                      ::std::is_same_v< ::std::decay_t< TaskType >, RcTaskPtr >;

} // namespace cxxmp::core
