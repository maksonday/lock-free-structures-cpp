# LockFree

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20" />
  <img src="https://img.shields.io/badge/Build-CMake-blue.svg" alt="CMake" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="MIT License" />
</p>

A collection of lock-free data structures implemented in C++20.

## Contents

- [Stack](#stack)

## Overview

This project contains a set of lock-free data structures designed for concurrent access without mutexes. Each structure is documented in a dedicated section below.

## Stack

### Overview

`lock_free::Stack<T>` provides concurrent `push()` and `pop()` operations without mutexes. The implementation uses atomic operations and hazard pointers to safely manage node reclamation in multithreaded workloads.

### Features

- lock-free stack semantics
- template-based API
- `std::optional<T>` return type for `pop()`
- header-only style
- CMake build configuration
- built-in stress tests

### Quick Start

```bash
git clone <repository-url>
cd lock-free
cmake -S . -B build
cmake --build build
./build/lock_free_tests
```

### Usage

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include "stack.hpp"

int main() {
    lock_free::Stack<int> stack{};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&stack, i]() {
            for (int value = i * 100; value < i * 100 + 50; ++value) {
                stack.push(value);
            }
        });
    }

    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back([&stack]() {
            for (int j = 0; j < 50; ++j) {
                auto value = stack.pop();
                if (value.has_value()) {
                    std::cout << value.value() << ' ';
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
}
```

### API

```cpp
namespace lock_free {

template <typename T>
class Stack {
public:
    Stack(size_t threads_count = 0);
    ~Stack();

    void push(T value);
    std::optional<T> pop();
};

} // namespace lock_free
```

### Internal behavior

The stack is implemented as a singly linked list:

- `push()` inserts a new node at the head using CAS loops
- `pop()` reads the current head, publishes the pointer as a hazard pointer, validates the node, and then atomically updates the head
- retired nodes are deferred from immediate deletion until it is safe to reclaim them

This design helps prevent use-after-free situations in concurrent access.

## Verifying correctness

The project includes a test executable that validates both basic behavior and concurrent stress scenarios.

### Standard build

```bash
cmake -S . -B build
cmake --build build
./build/lock_free_tests
```

### AddressSanitizer build

This is useful for catching memory bugs and use-after-free issues during concurrent access.

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/lock_free_tests
```

The default test program checks:

- single-thread stack behavior
- multi-thread push/pop correctness
- total pushed and popped counts match
- no duplicate or lost elements appear under contention

## Project Structure

```text
.
├── CMakeLists.txt
├── LICENSE
├── README.md
├── include/
│   ├── stack.hpp
│   └── stack.tpp
└── tests/
    └── main.cpp
```

## License

This project is licensed under the [MIT License](LICENSE).
