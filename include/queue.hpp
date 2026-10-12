#pragma once

#include <atomic>
#include <optional>

namespace lock_free {

template <typename T>
class Queue {
   public:
    /*
      @brief Creates lock-free multi-threaded Queue.
      @param threads_count Limit of threads that will use Queue
      If @p threads_count is non-positive or not provided, defaults to
      std::thread::hardware_concurrency(). If std::thread::hardware_concurrency() equals 0, limits
      to 1024 threads.
      */
    explicit Queue(size_t threads_count = 0);
    ~Queue();

    void push(T val);

    std::optional<T> pop();

   private:
    struct Node;

    class Hazard;
    class PtrGuard;
    class HazardGuard;
    class HazardRegistry;

    Node* dummy_;
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    HazardRegistry hazard_registry_;
};
}  // namespace lock_free

#include "queue.tpp"