#include <cstdint>
#include "check.hpp"
#include "syn/rng.hpp"
int main() {
  // Vetor de referencia do SplitMix64 (seed 0)
  syn::SplitMix64 sm(0);
  CHECK(sm.next() == 0xE220A8397B1DCDAFULL);
  CHECK(sm.next() == 0x6E789E6AA1B965F4ULL);
  CHECK(sm.next() == 0x06C45D188009454FULL);

  syn::Rng a(42), b(42), c(43);
  bool differs = false;
  for (int i = 0; i < 1000; ++i) {
    const auto x = a.next_u64();
    CHECK(x == b.next_u64());
    if (x != c.next_u64()) differs = true;
  }
  CHECK(differs);

  syn::Rng r(7);
  double sum = 0;
  for (int i = 0; i < 200000; ++i) {
    const float u = r.uniform();
    CHECK(u >= 0.0f && u < 1.0f);
    sum += static_cast<double>(u);
    CHECK(r.below(10) < 10);
  }
  CHECK(sum / 200000.0 > 0.49 && sum / 200000.0 < 0.51);

  // Checkpoint: restaurar estado continua a mesma sequencia
  syn::Rng s(99);
  for (int i = 0; i < 10; ++i) s.next_u64();
  const auto saved = s.state();
  const auto n1 = s.next_u64();
  s.set_state(saved);
  CHECK(s.next_u64() == n1);
  return 0;
}
