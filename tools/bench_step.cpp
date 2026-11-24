#include <cstdio>
#include <vector>
#include "syn/bench.hpp"
#include "syn/sim.hpp"
int main() {
  syn::ToyWorld w(1);
  // Cronometra lotes de passos: o relogio do Windows tem tick de ~100 ns, maior que um passo.
  constexpr int Batches = 4000, Batch = 100;
  std::vector<double> t; t.reserve(Batches);
  for (int i = 0; i < Batches; ++i) {
    const auto a = syn::bench::now_ns();
    for (int j = 0; j < Batch; ++j) w.step();
    t.push_back(static_cast<double>(syn::bench::now_ns() - a) / Batch);
  }
  const auto p = syn::bench::percentiles(t);
  std::printf("ToyWorld::step (ns/passo, lotes de 100)  p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f max=%.0f  hash=%016llx\n",
              p.p50, p.p90, p.p99, p.p999, p.max, (unsigned long long)w.state_hash());
}
