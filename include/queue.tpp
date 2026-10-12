#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <vector>

#include "common.hpp"

namespace lock_free {
template <typename T>
struct Queue<T>::Node {
    T val;
    std::atomic<Node*> next;

    Node() : val{}, next(nullptr) {}
    explicit Node(T val) : val(val), next(nullptr) {}
};

template <typename T>
class Queue<T>::Hazard {
   private:
    static constexpr int SCAN_THRESHOLD = 64;

    std::atomic<bool> active_ = false;
    std::atomic<Node*> ptr_ = nullptr;
    HazardRegistry* registry_ = nullptr;
    std::vector<Node*> retire_list_{};

   public:
    void init(HazardRegistry* registry) { registry_ = registry; }
    void store(Node* node) { ptr_.store(node, std::memory_order_seq_cst); }
    void release() { ptr_.store(nullptr, std::memory_order_seq_cst); }
    void deactivate() { active_.store(false, std::memory_order_seq_cst); }

    bool is_active() {
        bool expected = false;
        return active_.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                               std::memory_order_relaxed);
    }

    Node* value() { return ptr_.load(std::memory_order_seq_cst); }

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

    void clear() {
        for (auto& node : retire_list_) {
            delete (node);
        }
    }
};

template <typename T>
class Queue<T>::PtrGuard {
   public:
    PtrGuard(Hazard& hazard) : hazard_(hazard) {}
    ~PtrGuard() { hazard_.release(); }

   private:
    Hazard& hazard_;
};

template <typename T>
class Queue<T>::HazardGuard {
   public:
    HazardGuard(Hazard& hazard) : hazard_(hazard) {}
    ~HazardGuard() { hazard_.deactivate(); }

   private:
    Hazard& hazard_;
};

template <typename T>
class Queue<T>::HazardRegistry {
    static inline constexpr int ThreadReserveFactor = 3;

   public:
    HazardRegistry(uint threads_count = 0)
        : registry_(threads_count > 0
                        ? std::vector<Hazard>(ThreadReserveFactor * threads_count)
                        : std::vector<Hazard>(ThreadReserveFactor * default_threads_count())) {
        for (auto& h : registry_) {
            h.init(this);
        }
    }

    ~HazardRegistry() {
        for (auto& h : registry_) {
            h.release();
            h.deactivate();
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

    void clear_retired() {
        for (auto& hazard : registry_) {
            hazard.clear();
        }
    }

   private:
    static std::size_t default_threads_count() {
        const uint threads_count = std::thread::hardware_concurrency();
        return threads_count ? threads_count : kMaxThreads;
    }

    std::vector<Hazard> registry_;
};

template <typename T>
Queue<T>::Queue(size_t threads_count)
    : dummy_(new Node{}),
      head_(dummy_),
      tail_(dummy_),
      hazard_registry_(HazardRegistry(threads_count)) {}

template <typename T>
Queue<T>::~Queue() {
    Node* node = head_.load(std::memory_order_relaxed);
    while (node != nullptr) {
        Node* next = node->next.load(std::memory_order_relaxed);
        delete (node);
        node = next;
    }

    hazard_registry_.clear_retired();
}

template <typename T>
void Queue<T>::push(T val) {
    auto new_node = new Node(val);
    Hazard& hazard = hazard_registry_.acquire();
    HazardGuard hazard_guard(hazard);

    while (true) {
        PtrGuard guard(hazard);
        auto t = tail_.load(std::memory_order_acquire);
        hazard.store(t);
        if (tail_.load(std::memory_order_acquire) != t) {
            continue;
        }

        auto t_next = t->next.load(std::memory_order_acquire);
        if (t_next != nullptr) {
            tail_.compare_exchange_weak(t, t_next, std::memory_order_release,
                                        std::memory_order_relaxed);
            continue;
        }

        Node* tmp = nullptr;
        if (t->next.compare_exchange_weak(tmp, new_node, std::memory_order_release,
                                          std::memory_order_relaxed)) {
            break;
        }
    }
}

template <typename T>
std::optional<T> Queue<T>::pop() {
    Hazard& h_hazard = hazard_registry_.acquire();
    Hazard& h_next_hazard = hazard_registry_.acquire();
    HazardGuard h_hazard_guard(h_hazard), h_next_hazard_guard(h_next_hazard);

    std::optional<T> val{std::nullopt};
    while (true) {
        PtrGuard h_guard(h_hazard), h_next_guard(h_next_hazard);

        auto h = head_.load(std::memory_order_acquire);
        h_hazard.store(h);
        if (head_.load(std::memory_order_acquire) != h) {
            continue;
        }

        auto h_next = h->next.load(std::memory_order_acquire);
        h_next_hazard.store(h_next);
        if (h->next.load(std::memory_order_acquire) != h_next) {
            continue;
        }

        if (h_next == nullptr) {
            if (head_.load(std::memory_order_acquire) == h) {
                break;
            }
            continue;
        }

        auto t = tail_.load(std::memory_order_acquire);
        if (h == t) {
            tail_.compare_exchange_weak(t, h_next, std::memory_order_release,
                                        std::memory_order_relaxed);
            continue;
        }

        if (!head_.compare_exchange_weak(h, h_next, std::memory_order_release,
                                         std::memory_order_relaxed)) {
            continue;
        }
        val = std::move(h_next->val);
        h_hazard.retire(h);

        break;
    }

    return val;
}
}  // namespace lock_free