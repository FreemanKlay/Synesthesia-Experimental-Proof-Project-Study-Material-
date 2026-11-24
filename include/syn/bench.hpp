#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace syn::bench {
inline std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct Percentiles { double p50, p90, p99, p999, max; };
inline Percentiles percentiles(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  auto at = [&](double q) { return v[static_cast<std::size_t>(q * static_cast<double>(v.size() - 1))]; };
  return {at(0.50), at(0.90), at(0.99), at(0.999), v.back()};
}
} // namespace syn::bench
