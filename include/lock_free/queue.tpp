namespace lock_free {
template <typename T>
struct Queue<T>::Node {
    T val;
    std::atomic<Node*> next;

    Node() : val{}, next(nullptr) {}
    explicit Node(T val) : val(val), next(nullptr) {}
};

template <typename T>
Queue<T>::Queue(size_t threads_count)
    : dummy_(new Node{}),
      head_(dummy_),
      tail_(dummy_),
      hazard_registry_(HazardRegistry<Node>(threads_count)) {}

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
    Hazard<Node>& hazard = hazard_registry_.acquire();
    HazardGuard<Node> hazard_guard(hazard);

    while (true) {
        PtrGuard<Node> guard(hazard);
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
    Hazard<Node>& h_hazard = hazard_registry_.acquire();
    Hazard<Node>& h_next_hazard = hazard_registry_.acquire();
    HazardGuard<Node> h_hazard_guard(h_hazard), h_next_hazard_guard(h_next_hazard);

    std::optional<T> val{std::nullopt};
    while (true) {
        PtrGuard<Node> h_guard(h_hazard), h_next_guard(h_next_hazard);

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
