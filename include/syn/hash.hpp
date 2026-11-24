#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace syn {
// FNV-1a 64 sobre bytes: comparacao bit a bit barata do estado do mundo.
inline std::uint64_t fnv1a(std::span<const std::byte> bytes, std::uint64_t h = 0xCBF29CE484222325ULL) {
  for (std::byte b : bytes) { h ^= static_cast<std::uint64_t>(b); h *= 0x100000001B3ULL; }
  return h;
}
template <class T>
std::uint64_t hash_pod_range(std::span<const T> r, std::uint64_t h = 0xCBF29CE484222325ULL) {
  return fnv1a(std::as_bytes(r), h);
}
} // namespace syn
