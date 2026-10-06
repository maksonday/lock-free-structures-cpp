#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <vector>

namespace lock_free {

template <typename T>
Stack<T>::Stack(size_t threads_count)
    : head_(nullptr), hazard_registry_(HazardRegistry(threads_count)) {}

template <typename T>
Stack<T>::~Stack() {
    while (pop().has_value()) {
    }
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
    Hazard& hazard = hazard_registry_.acquire();
    HazardGuard guard(hazard);
    auto h = head_.load(std::memory_order_acquire);

    while (h) {
        hazard.store(h);

        // need to check if h is still actual since we've published it to hazard-pointer
        // otherwise we need to update h and retry to publish it to hazard-pointer
        if (head_.load(std::memory_order_acquire) != h) {
            hazard.store(nullptr);

            h = head_.load(std::memory_order_acquire);
            continue;
        }

        auto next = h->next;
        if (head_.compare_exchange_weak(h, next, std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
            auto val = std::move(h->val);
            hazard.retire(h);
            hazard.store(nullptr);

            return val;
        }
        hazard.store(nullptr);
    }

    return std::nullopt;
}

template <typename T>
struct Stack<T>::Node {
    T val;
    Node* next;

    explicit Node(T item) : val(item), next(nullptr) {}
};

template <typename T>
class Stack<T>::Hazard {
   private:
    static constexpr int SCAN_THRESHOLD = 64;

    std::atomic<bool> active_ = false;
    std::atomic<Stack<T>::Node*> ptr_ = nullptr;
    HazardRegistry* registry_ = nullptr;
    std::vector<Node*> retire_list_;

   public:
    void init(HazardRegistry* registry) { registry_ = registry; }
    void store(Node* node) { ptr_.store(node, std::memory_order_seq_cst); }
    void release() {
        ptr_.store(nullptr, std::memory_order_seq_cst);
        scan();
        active_.store(false, std::memory_order_seq_cst);
    }

    bool is_active() {
        bool expected = false;
        return active_.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                               std::memory_order_relaxed);
    }

    Node* value() { return ptr_.load(); }

    void scan() {
        auto active_pointers = registry_->active_pointers();
        auto it = retire_list_.begin();
        while (it != retire_list_.end()) {
            Node* node = *it;
            if (active_pointers.find(node) == active_pointers.end()) {
                delete (node);
                it = retire_list_.erase(it);
            } else {
                it++;
            }
        }
    }

    void retire(Node* el) {
        if (el == nullptr) {
            return;
        }

        retire_list_.push_back(el);

        if (retire_list_.size() >= SCAN_THRESHOLD) {
            scan();
        }
    }
};

template <typename T>
class Stack<T>::HazardRegistry {
   public:
    HazardRegistry(size_t threads_count = 0)
        : registry_(threads_count > 0 ? std::vector<Hazard>(threads_count)
                                      : std::vector<Hazard>(default_threads_count())) {
        for (auto& h : registry_) {
            h.init(this);
        }
    }

    ~HazardRegistry() {
        for (auto& h : registry_) {
            h.release();
        }
    }

    Hazard& acquire() {
        for (auto& h : registry_) {
            if (h.is_active()) {
                return h;
            }
        }

        throw std::runtime_error("limit of max threads exceeded");
    }

    std::unordered_set<Node*> active_pointers() {
        std::unordered_set<Node*> set;
        for (auto& h : registry_) {
            if (auto ptr = h.value()) {
                set.insert(ptr);
            }
        }

        return set;
    }

   private:
    static size_t default_threads_count() {
        const uint threads_count = std::thread::hardware_concurrency();
        return threads_count ? threads_count : kMaxThreads;
    }

    std::vector<Hazard> registry_;
};

template <typename T>
class Stack<T>::HazardGuard {
   public:
    explicit HazardGuard(Hazard& hazard) : hazard_(hazard) {}
    ~HazardGuard() { hazard_.release(); }

   private:
    Hazard& hazard_;
};

}  // namespace lock_free
