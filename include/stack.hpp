#pragma once

#include <atomic>
#include <optional>

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

    class Hazard;
    class PtrGuard;
    class HazardGuard;
    class HazardRegistry;

    std::atomic<Node*> head_;
    HazardRegistry hazard_registry_;
};

}  // namespace lock_free

#include "stack.tpp"
