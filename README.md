# LockFree

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20" />
  <img src="https://img.shields.io/badge/Build-CMake-blue.svg" alt="CMake" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="MIT License" />
</p>

A lock-free stack implemented in C++20.

## Overview

`lock_free::Stack<T>` provides concurrent `push()` and `pop()` operations without mutexes. The implementation uses atomic instructions and hazard pointers to safely manage node reclamation in multithreaded workloads.

## Features

- lock-free stack semantics
- template-based API
- `std::optional<T>` return type for `pop()`
- header-only inclusion via `include/`
- CMake build configuration
- stress tests included

## Quick Start

```bash
git clone <repository-url>
cd lock-free
cmake -S . -B build
cmake --build build
./build/lock_free_tests
```

## Usage

```cpp
#include <iostream>
#include "stack.hpp"

int main() {
    lock_free::Stack<int> stack{};

    stack.push(10);
    stack.push(20);
    stack.push(30);

    std::cout << stack.pop().value() << '\n'; // 30
    std::cout << stack.pop().value() << '\n'; // 20
    std::cout << stack.pop().value() << '\n'; // 10

    return 0;
}
```

## API

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
