#include <cassert>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "stack.hpp"

using namespace lock_free;

void RunSingleThreadTest() {
    std::cout << "[TEST] Running single thread test... " << std::flush;
    Stack<int> s{};

    s.push(10);
    s.push(20);
    s.push(30);

    assert(s.pop() == 30);
    assert(s.pop() == 20);
    assert(s.pop() == 10);
    assert(s.pop() == std::nullopt);

    std::cout << "PASSED!" << std::endl;
}

void RunMultiThreadStressTest() {
    std::cout << "[TEST] Running multi-thread stress test... " << std::flush;
    Stack<int> s{};

    const int kNumThreads = 8;
    const int kOperationsPerThread = 10000;

    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::unordered_map<int, std::atomic<int>> checker(kOperationsPerThread * kNumThreads);
    for (int i = 0; i < kOperationsPerThread * kNumThreads; i++) {
        checker.emplace(i, 0);
    }

    for (int i = 0; i < kNumThreads; ++i) {
        producers.emplace_back([&s, &total_pushed, i, &checker, kOperationsPerThread]() {
            for (int j = i; j < i + kOperationsPerThread; ++j) {
                s.push(j);
                checker[j].fetch_add(1, std::memory_order_relaxed);
                total_pushed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int i = 0; i < kNumThreads; ++i) {
        consumers.emplace_back([&s, &total_popped, &checker, kOperationsPerThread]() {
            int local_pops = 0;
            while (local_pops < kOperationsPerThread) {
                if (auto v = s.pop(); v.has_value()) {
                    total_popped.fetch_add(1, std::memory_order_relaxed);
                    checker[v.value()].fetch_sub(1, std::memory_order_relaxed);
                    local_pops++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    assert(total_pushed.load() == total_popped.load());
    assert(s.pop() == std::nullopt);
    int non_empty_buckets = 0;

    for (auto& it : checker) {
        if (it.second.load() != 0) {
            non_empty_buckets++;
        }
    }

    assert(non_empty_buckets == 0);

    std::cout << "PASSED! (Successfully processed " << total_pushed.load() << " elements)"
              << std::endl;
}

int main() {
    RunSingleThreadTest();
    RunMultiThreadStressTest();

    std::cout << "\nAll tests completed successfully!" << std::endl;
    return 0;
}