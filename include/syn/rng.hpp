#pragma once
// PRNG proprio e portavel: SplitMix64 (semeadura) + xoshiro256** (Blackman & Vigna).
// Mesma semente => mesma sequencia, em qualquer compilador/plataforma.
#include <array>
#include <bit>
#include <cstdint>

namespace syn {

class SplitMix64 {
public:
  explicit constexpr SplitMix64(std::uint64_t seed) : s_(seed) {}
  constexpr std::uint64_t next() {
    std::uint64_t z = (s_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
private:
  std::uint64_t s_;
};

// Parte alta (64 bits) do produto 64x64 -> 128, sem extensoes de compilador.
constexpr std::uint64_t mulhi64(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t a_lo = a & 0xFFFFFFFFULL, a_hi = a >> 32;
  const std::uint64_t b_lo = b & 0xFFFFFFFFULL, b_hi = b >> 32;
  const std::uint64_t p0 = a_lo * b_lo, p1 = a_lo * b_hi, p2 = a_hi * b_lo, p3 = a_hi * b_hi;
  const std::uint64_t mid = (p0 >> 32) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
  return p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
}

class Rng {
public:
  explicit constexpr Rng(std::uint64_t seed = 1) {
    SplitMix64 sm(seed);
    for (auto& w : s_) w = sm.next();
  }
  constexpr std::uint64_t next_u64() {
    const std::uint64_t result = std::rotl(s_[1] * 5, 7) * 9;
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0]; s_[3] ^= s_[1]; s_[1] ^= s_[2]; s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = std::rotl(s_[3], 45);
    return result;
  }
  // [0,1) com 24 bits de mantissa: exato e portavel (sem depender de <random>)
  constexpr float uniform() {
    return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f);
  }
  constexpr float symmetric() { return uniform() * 2.0f - 1.0f; }  // [-1,1)
  // Inteiro em [0,n) (Lemire): parte alta de next*n, portavel (sem __int128, funciona em MSVC)
  constexpr std::uint64_t below(std::uint64_t n) { return mulhi64(next_u64(), n); }
  // Para checkpoints (Fase 7)
  constexpr const std::array<std::uint64_t, 4>& state() const { return s_; }
  constexpr void set_state(const std::array<std::uint64_t, 4>& s) { s_ = s; }
private:
  std::array<std::uint64_t, 4> s_{};
};

} // namespace syn
