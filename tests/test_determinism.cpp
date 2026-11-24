// Aceitacao da Fase 0: mesma semente => estado identico bit a bit apos 1 milhao de passos.
#include <cstdio>
#include <cstdlib>
#include "check.hpp"
#include "syn/sim.hpp"
int main(int argc, char** argv) {
  const std::uint64_t steps = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1'000'000ULL;
  const std::uint64_t seed = 0xC0FFEE;
  syn::ToyWorld w1(seed), w2(seed), w3(seed + 1);
  const auto h1 = syn::run_fixed_steps(w1, steps);
  const auto h2 = syn::run_fixed_steps(w2, steps);
  const auto h3 = syn::run_fixed_steps(w3, steps);
  std::printf("steps=%llu  h1=%016llx  h2=%016llx  h3(outra seed)=%016llx\n",
              (unsigned long long)steps, (unsigned long long)h1,
              (unsigned long long)h2, (unsigned long long)h3);
  CHECK(h1 == h2);
  // Hash de referencia: igual em Windows/Linux/macOS e em qualquer compilador.
  // ATENCAO: so muda de proposito (ex.: ao trocar o ToyWorld pelo FDTD). Atualize junto no mesmo commit.
  if (steps == 1'000'000ULL) CHECK(h1 == 0x7c8eb2fb5eee4c0eULL);
  CHECK(h1 != h3);
  return 0;
}
