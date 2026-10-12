#pragma once

namespace lock_free {
inline constexpr std::size_t kMaxThreads = 1024;

template <typename T>
class HazardRegistry;

template <typename T>
class Hazard;

template <typename T>
class PtrGuard;

template <typename T>
class HazardGuard;
};  // namespace lock_free

#include "common.tpp"