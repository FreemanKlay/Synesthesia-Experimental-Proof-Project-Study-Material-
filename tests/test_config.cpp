#include <cmath>
#include <cstdio>
#include "check.hpp"
#include "syn/config.hpp"
int main() {
  using namespace syn::config;
  const double cfl = sound_speed * dt / dx;
  std::printf("dx=%.3f mm  c*dt=%.3f mm  CFL=%.4f  mundo=%.2f x %.2f m\n",
              dx * 1e3, sound_speed * dt * 1e3, cfl, world_w_m, world_h_m);
  CHECK(cfl <= courant_limit_2d);
  CHECK(std::fabs(cfl - courant) < 1e-12);
  CHECK(world_w_m > 2.5 && world_w_m < 3.2);
  return 0;
}
