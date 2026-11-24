# ADR-0001: Escala física derivada da condição CFL

**Status:** aceita

## Contexto
FDTD 2D da equação da onda é estável se c·dt/dx ≤ 1/√2. A taxa de áudio (44,1 kHz) fixa dt.

## Decisão
- c = 343 m/s, dt = 1/44100 s → c·dt = 7,78 mm
- Número de Courant = 0,70 (margem de ~1% sobre o limite) → dx = c·dt/0,70 ≈ 11,1 mm
- Grade 256×256 → mundo de ≈ 2,84 m × 2,84 m; criaturas de alguns cm
- Tudo em `include/syn/config.hpp`; `static_assert` barra configurações instáveis.

## Alavancas para mundos maiores (parametrizadas, ainda não implementadas)
1. Simular a 16–22 kHz e fazer upsampling polifásico no microfone virtual.
2. Escalar c (acústica em câmera lenta).

## Determinismo (decisões ligadas)
Sem `-ffast-math`, `-ffp-contract=off`, PRNG próprio (xoshiro256**), nenhum relógio de parede na simulação.
