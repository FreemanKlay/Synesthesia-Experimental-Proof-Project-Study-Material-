#pragma once
// Saida de audio WASAPI (modo compartilhado, orientada a evento, float32 estereo).
// A thread de audio SO consome da fila SPSC: sem alocacao, sem locks, sem syscalls bloqueantes
// alem da espera pelo evento. Se a fila secar, preenche com silencio e conta um underrun.
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "syn/spsc_queue.hpp"

namespace syn::platform {

struct StereoFrame { float l; float r; };
static_assert(sizeof(StereoFrame) == 8, "layout intercalado L,R float32");
using AudioQueue = syn::SpscQueue<StereoFrame>;

struct AudioInfo {
  std::uint32_t sample_rate = 0, channels = 0, buffer_frames = 0;
  double period_ms = 0, buffer_ms = 0, stream_latency_ms = 0;
};

// Escritos pela thread de audio, lidos por qualquer thread (relaxed).
struct AudioStats {
  std::atomic<std::uint64_t> callbacks{0};
  std::atomic<std::uint64_t> frames_rendered{0};
  std::atomic<std::uint64_t> underruns{0};          // callbacks em que a fila nao tinha frames suficientes
  std::atomic<std::uint64_t> starved_frames{0};     // frames de silencio inseridos
  std::atomic<std::uint64_t> late_wakeups{0};       // intervalo entre callbacks > 2x o periodo do dispositivo
  std::atomic<std::uint64_t> max_wake_gap_us{0};
  std::atomic<std::uint64_t> min_queue_fill{~0ULL}; // frames na fila antes de cada consumo
  std::atomic<std::uint64_t> max_callback_ns{0};
  std::atomic<std::uint64_t> fill_after_sum{0};     // soma da ocupacao da fila apos cada consumo (frames)
  std::atomic<std::uint64_t> fill_after_samples{0};
};

class WasapiOut {
public:
  struct Impl;
  explicit WasapiOut(AudioQueue& queue, std::uint32_t sample_rate = 44100);
  ~WasapiOut();
  WasapiOut(const WasapiOut&) = delete;
  WasapiOut& operator=(const WasapiOut&) = delete;

  bool start(std::string* error);  // inicializa o dispositivo e lanca a thread de audio
  void stop();
  AudioInfo info() const;
  const AudioStats& stats() const;
  std::vector<std::uint32_t> callback_times_ns() const;  // chamar depois de stop()
private:
  std::unique_ptr<Impl> impl_;
};

} // namespace syn::platform
