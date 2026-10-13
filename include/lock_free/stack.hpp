#pragma once

#include <atomic>
#include <optional>

#include "lock_free/internal/common.hpp"

namespace lock_free {

template <typename T>
class Stack {
   public:
    /*
     @brief Creates lock-free multi-threaded Stack.
     @param threads_count Limit of threads that will use Stack
     If @p threads_count is non-positive or not provided, defaults to
     std::thread::hardware_concurrency(). If std::thread::hardware_concurrency() equals 0, limits to
     1024 threads.
     */
    explicit Stack(size_t threads_count = 0);
    ~Stack();

    void push(T value);
    std::optional<T> pop();

   private:
    struct Node;

    std::atomic<Node*> head_;
    HazardRegistry<Node> hazard_registry_;
};

}  // namespace lock_free

#include "lock_free/stack.tpp"
