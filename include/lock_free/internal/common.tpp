namespace lock_free {
template <typename T>
class Hazard {
   private:
    static constexpr int SCAN_THRESHOLD = 64;

    std::atomic<bool> active_ = false;
    std::atomic<T*> ptr_ = nullptr;
    HazardRegistry<T>* registry_ = nullptr;
    std::vector<T*> retire_list_{};

   public:
    void init(HazardRegistry<T>* registry) { registry_ = registry; }
    void store(T* node) { ptr_.store(node, std::memory_order_seq_cst); }
    void release() { ptr_.store(nullptr, std::memory_order_seq_cst); }
    void deactivate() { active_.store(false, std::memory_order_seq_cst); }

    bool is_active() {
        bool expected = false;
        return active_.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                               std::memory_order_relaxed);
    }

    T* value() { return ptr_.load(std::memory_order_seq_cst); }

    void scan() {
        auto active_pointers = registry_->active_pointers();
        auto it = retire_list_.begin();
        while (it != retire_list_.end()) {
            T* node = *it;
            if (active_pointers.find(node) == active_pointers.end()) {
                delete (node);
                it = retire_list_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void retire(T* el) {
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
class HazardRegistry {
    static inline constexpr int ThreadReserveFactor = 3;

   public:
    explicit HazardRegistry(unsigned int threads_count = 0)
        : registry_(threads_count > 0
                        ? std::vector<Hazard<T>>(ThreadReserveFactor * threads_count)
                        : std::vector<Hazard<T>>(ThreadReserveFactor * default_threads_count())) {
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

    Hazard<T>& acquire() {
        for (auto& h : registry_) {
            if (h.is_active()) {
                return h;
            }
        }

        throw std::runtime_error("limit of max threads exceeded");
    }

    std::unordered_set<T*> active_pointers() {
        std::unordered_set<T*> set;
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
        const unsigned int threads_count = std::thread::hardware_concurrency();
        return threads_count ? threads_count : kMaxThreads;
    }

    std::vector<Hazard<T>> registry_;
};

template <typename T>
class PtrGuard {
   public:
    explicit PtrGuard(Hazard<T>& hazard) : hazard_(hazard) {}
    ~PtrGuard() { hazard_.release(); }

   private:
    Hazard<T>& hazard_;
};

template <typename T>
class HazardGuard {
   public:
    explicit HazardGuard(Hazard<T>& hazard) : hazard_(hazard) {}
    ~HazardGuard() { hazard_.deactivate(); }

   private:
    Hazard<T>& hazard_;
};
}  // namespace lock_free
