#pragma once
// Escala fisica do mundo (ADR-0001). Tudo deriva da condicao CFL do FDTD 2D:
//   c*dt/dx <= 1/sqrt(2)
// Mude aqui e so aqui; os static_assert impedem configuracoes instaveis.
#include <cstdint>

namespace syn::config {

inline constexpr double sample_rate_hz = 44100.0;          // taxa acustica (relogio mestre)
inline constexpr double sound_speed    = 343.0;            // m/s (ar, ~20 C)
inline constexpr double dt             = 1.0 / sample_rate_hz;

inline constexpr double courant          = 0.70;           // numero de Courant escolhido
inline constexpr double courant_limit_2d = 0.70710678118654752;
inline constexpr double dx               = sound_speed * dt / courant; // m (~11.1 mm)

inline constexpr int grid_nx = 256;
inline constexpr int grid_ny = 256;
inline constexpr double world_w_m = grid_nx * dx;
inline constexpr double world_h_m = grid_ny * dx;

// Cerebros rodam a cada N amostras acusticas (~689 Hz)
inline constexpr int brain_stride_samples = 64;

static_assert(courant <= courant_limit_2d, "CFL violada: FDTD 2D instavel");
static_assert(courant > 0.0, "courant deve ser positivo");
static_assert(grid_nx % 8 == 0, "largura multipla de 8 (AVX2) facilita a Fase 2");

} // namespace syn::config
