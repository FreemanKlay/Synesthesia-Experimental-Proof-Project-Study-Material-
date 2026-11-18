# Synesthesia Experimental Proof Project *(Study Material)*

**An experimental C++23 acoustic ecosystem and CPU-only 3D software rasterizer built from scratch as a deep-dive study in low-latency systems, SIMD, and artificial life.**

---

## Overview

This repository is a personal research sandbox exploring what happens when wave physics, evolutionary neural networks, real-time audio synthesis, and software rendering collide without external engines or heavy abstractions.

Digital organisms inhabit a 2D pressure grid governed by wave equations. Without traditional vision, they rely on simulated acoustic sensors ("ears") and frequency emitters ("mouths") driven by custom SIMD-accelerated neural networks. Over generations, survival pressure forces emergent acoustic behaviors, such as echolocation, pack signaling, and active noise cancellation, to evolve from pure math.

## Core Pillars

* **Acoustic Wave Engine:** Real-time 2D air pressure simulation modeling wave propagation, reflection, and energy dissipation.
* **Lock-Free Audio Pipeline:** A virtual microphone captures raw pressure fluctuations directly from the grid and streams them to the audio hardware at 44.1 kHz with zero mutexes on the hot path.
* **SIMD Neural Brains:** Custom matrix math accelerated via AVX2 intrinsics, paired with genetic algorithms for generational mutation.
* **CPU-Only 3D Rasterizer:** No OpenGL, Vulkan, or DirectX. Terrain, organisms, and dynamic sound wave deformations are projected and shaded purely on the CPU.
* **Zero-Overhead Memory:** Custom arena allocators and cache-line alignment (`alignas(64)`) to prevent false sharing and dynamic allocation stalls.

## Status

`[In Active Development]` — Built from the ground up as a low-level C++ engineering study.
