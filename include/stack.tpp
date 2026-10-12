#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <vector>

#include "common.hpp"

namespace lock_free {
template <typename T>
struct Stack<T>::Node {
    T val;
    Node* next;

    explicit Node(T item) : val(item), next(nullptr) {}
};

template <typename T>
Stack<T>::Stack(size_t threads_count)
    : head_(nullptr), hazard_registry_(HazardRegistry<Node>(threads_count)) {}

template <typename T>
Stack<T>::~Stack() {
    Node* node = head_.load(std::memory_order_relaxed);
    while (node != nullptr) {
        Node* next = node->next;
        delete (node);
        node = next;
    }

    hazard_registry_.clear_retired();
}

template <typename T>
void Stack<T>::push(T value) {
    auto h = head_.load(std::memory_order_acquire);
    auto new_node = new Node{value};
    do {
        new_node->next = h;
    } while (!head_.compare_exchange_weak(h, new_node, std::memory_order_release,
                                          std::memory_order_relaxed));
}

template <typename T>
std::optional<T> Stack<T>::pop() {
    Hazard<Node>& hazard = hazard_registry_.acquire();
    HazardGuard<Node> h_guard(hazard);
    auto h = head_.load(std::memory_order_acquire);

    std::optional<T> val{std::nullopt};
    while (h) {
        PtrGuard<Node> huard(hazard);
        hazard.store(h);

        // need to check if h is still actual since we've published it to hazard-pointer
        // otherwise we need to update h and retry to publish it to hazard-pointer
        if (head_.load(std::memory_order_acquire) != h) {
            h = head_.load(std::memory_order_acquire);
            continue;
        }

        auto next = h->next;
        if (head_.compare_exchange_weak(h, next, std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
            val = std::move(h->val);
            hazard.retire(h);
            break;
        }
    }

    return val;
}

}  // namespace lock_free
