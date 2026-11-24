#pragma once
// Esqueleto do loop de simulacao em passo fixo. Nenhuma dependencia do relogio de parede.
// ToyWorld e um substituto: sera trocado pelo FDTD na Fase 2, mantendo a interface
// (step / state_hash) e o teste de determinismo.
#include <array>
#include <cstdint>
#include <span>
#include "syn/hash.hpp"
#include "syn/rng.hpp"

namespace syn {

class ToyWorld {
public:
  static constexpr std::size_t N = 256;
  explicit ToyWorld(std::uint64_t seed) : rng_(seed) {
    for (auto& v : field_) v = rng_.symmetric();
  }
  void step() {
    // difusao + decaimento + ruido injetado: usa FP e RNG, como a simulacao real
    std::array<float, N> next{};
    for (std::size_t i = 0; i < N; ++i) {
      const float l = field_[(i + N - 1) % N];
      const float c = field_[i];
      const float r = field_[(i + 1) % N];
      next[i] = (0.25f * l + 0.5f * c + 0.25f * r) * 0.9995f;
    }
    next[rng_.below(N)] += rng_.symmetric();
    field_ = next;
    ++tick_;
  }
  std::uint64_t tick() const { return tick_; }
  std::uint64_t state_hash() const {
    std::uint64_t h = hash_pod_range<float>(std::span<const float>(field_));
    h = hash_pod_range<std::uint64_t>(std::span<const std::uint64_t>(rng_.state()), h);
    return h ^ tick_;
  }
private:
  std::array<float, N> field_{};
  Rng rng_;
  std::uint64_t tick_ = 0;
};

template <class World>
std::uint64_t run_fixed_steps(World& w, std::uint64_t steps) {
  for (std::uint64_t i = 0; i < steps; ++i) w.step();
  return w.state_hash();
}

} // namespace syn
