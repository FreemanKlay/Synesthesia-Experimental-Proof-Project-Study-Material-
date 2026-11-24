// Fase 1 - caixa de areia: janela com framebuffer da CPU + senoide via WASAPI + fila SPSC.
// Uso: sandbox [--seconds N] [--freq HZ] [--load N] [--fill-ms MS] [--no-window]
//   --fill-ms MS : folga alvo da fila SPSC (padrao 93 ms). Menor = menos latencia, mais risco de underrun.
//   --load N : N threads gastando CPU (carga artificial) durante o teste
//   Sai com codigo 1 se houve underrun (util para o teste de 1 hora).
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>  // timeBeginPeriod / timeEndPeriod (winmm)
#include "syn/bench.hpp"
#include "syn/platform/wasapi_out.hpp"
#include "syn/platform/win32_window.hpp"

using namespace syn::platform;

static std::atomic<bool> g_quit{false};
static BOOL WINAPI on_ctrl(DWORD) { g_quit = true; return TRUE; }

struct Args { double seconds = 0; double freq = 440.0; int load = 0; double fill_ms = 93.0; bool window = true; };

static Args parse(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--seconds") && i + 1 < argc) a.seconds = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--freq") && i + 1 < argc) a.freq = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--load") && i + 1 < argc) a.load = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--fill-ms") && i + 1 < argc) a.fill_ms = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--no-window")) a.window = false;
    else { std::fprintf(stderr, "argumento desconhecido: %s\n", argv[i]); std::exit(2); }
  }
  return a;
}

int main(int argc, char** argv) {
  const Args args = parse(argc, argv);
  constexpr std::uint32_t kRate = 44100;
  constexpr std::size_t kQueueFrames = 16384;  // ~371 ms de folga maxima
  constexpr std::size_t kBlock = 256;
  const std::size_t kTargetFill = std::min<std::size_t>(kQueueFrames - kBlock,
      std::max<std::size_t>(2 * kBlock, static_cast<std::size_t>(args.fill_ms * kRate / 1000.0)));  // o produtor mantem a fila neste nivel

  SetConsoleCtrlHandler(on_ctrl, TRUE);
  timeBeginPeriod(1);  // sleeps de ~1 ms para o produtor e o pacing de quadros

  AudioQueue queue(kQueueFrames);
  std::atomic<bool> stop_all{false};

  // Produtor de audio (stand-in da thread de fisica): senoide com fade-in, sempre adiantado.
  std::thread producer([&] {
    const double two_pi = 6.283185307179586;
    const double inc = two_pi * args.freq / kRate;
    double phase = 0.0;
    std::uint64_t produced = 0;
    std::vector<StereoFrame> blk(kBlock);
    while (!stop_all.load(std::memory_order_relaxed)) {
      if (queue.size_approx() + kBlock > kTargetFill) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
      for (auto& f : blk) {
        const double fade = std::min(1.0, static_cast<double>(produced++) / (kRate * 0.05));
        const float s = static_cast<float>(0.2 * fade * std::sin(phase));
        phase += inc;
        if (phase > two_pi) phase -= two_pi;
        f = {s, s};
      }
      std::size_t done = 0;
      while (done < kBlock && !stop_all.load(std::memory_order_relaxed)) {
        done += queue.push_n(blk.data() + done, kBlock - done);
        if (done < kBlock) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  });

  // Carga artificial de CPU
  std::vector<std::thread> loaders;
  for (int i = 0; i < args.load; ++i)
    loaders.emplace_back([&] { volatile double x = 1.0; while (!stop_all.load(std::memory_order_relaxed)) x = x * 1.0000001 + 1e-9; });

  while (queue.size_approx() < kTargetFill / 2) std::this_thread::sleep_for(std::chrono::milliseconds(1));

  WasapiOut audio(queue, kRate);
  std::string err;
  if (!audio.start(&err)) {
    std::fprintf(stderr, "Falha ao iniciar WASAPI: %s\n", err.c_str());
    stop_all = true; producer.join(); for (auto& t : loaders) t.join();
    return 3;
  }
  const AudioInfo ai = audio.info();
  std::printf("WASAPI: %u Hz, %u ch, buffer %u frames (%.1f ms), periodo %.1f ms, latencia do stream %.1f ms\n",
              ai.sample_rate, ai.channels, ai.buffer_frames, ai.buffer_ms, ai.period_ms, ai.stream_latency_ms);
  std::printf("Tom %.1f Hz | carga: %d thread(s) | janela: %s | %s\n", args.freq, args.load,
              args.window ? "sim" : "nao", args.seconds > 0 ? "duracao fixa" : "Ctrl+C ou ESC para sair");

  // Janela + framebuffer de teste (plasma)
  constexpr int W = 640, H = 360;
  Window win;
  std::vector<std::uint32_t> fb(static_cast<std::size_t>(W) * H);
  if (args.window && !win.create("Synesthesia - sandbox", W * 2, H * 2)) {
    std::fprintf(stderr, "Falha ao criar janela\n");
    g_quit = true;
  }
  std::uint8_t sintab[256];
  for (int i = 0; i < 256; ++i) sintab[i] = static_cast<std::uint8_t>(127.5 + 127.5 * std::sin(i * 6.283185307179586 / 256.0));

  using clk = std::chrono::steady_clock;
  const auto t_start = clk::now();
  auto next_frame = t_start;
  auto next_print = t_start + std::chrono::seconds(5);
  auto next_title = t_start;
  std::uint64_t frame = 0;
  std::vector<double> frame_ms;
  frame_ms.reserve(1 << 16);

  while (!g_quit.load()) {
    const auto now = clk::now();
    const double elapsed = std::chrono::duration<double>(now - t_start).count();
    if (args.seconds > 0 && elapsed >= args.seconds) break;
    if (args.window && !win.pump()) break;

    if (args.window) {
      const auto f0 = syn::bench::now_ns();
      const int t = static_cast<int>(frame);
      for (int y = 0; y < H; ++y) {
        std::uint32_t* row = fb.data() + static_cast<std::size_t>(y) * W;
        for (int x = 0; x < W; ++x) {
          const int v = (sintab[(x * 2 + t) & 255] + sintab[(y * 3 + t * 2) & 255] + sintab[(x + y + t) & 255]) / 3;
          const std::uint32_t r = sintab[(v + t) & 255], g = sintab[(v * 2) & 255], b = static_cast<std::uint32_t>(v);
          row[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
      }
      win.present(fb.data(), W, H);
      frame_ms.push_back(static_cast<double>(syn::bench::now_ns() - f0) / 1e6);
      ++frame;
      next_frame += std::chrono::microseconds(16667);
      std::this_thread::sleep_until(next_frame);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const auto& s = audio.stats();
    if (args.window && now >= next_title) {
      char title[256];
      std::snprintf(title, sizeof(title), "Synesthesia sandbox | fila %zu | underruns %llu | late %llu | %.0fs",
                    queue.size_approx(), static_cast<unsigned long long>(s.underruns.load()),
                    static_cast<unsigned long long>(s.late_wakeups.load()), elapsed);
      win.set_title(title);
      next_title = now + std::chrono::milliseconds(500);
    }
    if (now >= next_print) {
      std::printf("[%6.0fs] callbacks=%llu underruns=%llu late=%llu fila_min=%llu\n", elapsed,
                  static_cast<unsigned long long>(s.callbacks.load()), static_cast<unsigned long long>(s.underruns.load()),
                  static_cast<unsigned long long>(s.late_wakeups.load()), static_cast<unsigned long long>(s.min_queue_fill.load()));
      std::fflush(stdout);
      next_print += std::chrono::seconds(5);
    }
  }

  const double total_s = std::chrono::duration<double>(clk::now() - t_start).count();
  audio.stop();
  stop_all = true;
  producer.join();
  for (auto& t : loaders) t.join();
  timeEndPeriod(1);

  const auto& s = audio.stats();
  auto cb = audio.callback_times_ns();
  std::printf("\n===== Relatorio =====\n");
  std::printf("duracao: %.1f s | callbacks: %llu | frames: %llu\n", total_s,
              static_cast<unsigned long long>(s.callbacks.load()), static_cast<unsigned long long>(s.frames_rendered.load()));
  std::printf("underruns: %llu (frames de silencio: %llu) | late wakeups: %llu | maior intervalo entre callbacks: %.2f ms\n",
              static_cast<unsigned long long>(s.underruns.load()), static_cast<unsigned long long>(s.starved_frames.load()),
              static_cast<unsigned long long>(s.late_wakeups.load()), static_cast<double>(s.max_wake_gap_us.load()) / 1000.0);
  std::printf("menor nivel da fila: %llu frames (alvo %zu)\n", static_cast<unsigned long long>(s.min_queue_fill.load()), kTargetFill);
  {
    const auto n = s.fill_after_samples.load();
    const double fill_ms = n ? 1000.0 * (static_cast<double>(s.fill_after_sum.load()) / static_cast<double>(n)) / kRate : 0.0;
    // GetStreamLatency pode devolver 0 em modo compartilhado; o buffer do WASAPI e sempre mantido cheio.
    std::printf("latencia de saida estimada: fila (media) %.1f ms + buffer WASAPI %.1f ms = ~%.1f ms "
                "(+ mixagem do Windows, nao medida; GetStreamLatency reportou %.1f ms)\n",
                fill_ms, ai.buffer_ms, fill_ms + ai.buffer_ms, ai.stream_latency_ms);
  }
  if (!cb.empty()) {
    std::vector<double> v(cb.begin(), cb.end());
    const auto p = syn::bench::percentiles(std::move(v));
    std::printf("tempo do callback (us): p50=%.1f p90=%.1f p99=%.1f p99.9=%.1f max=%.1f\n",
                p.p50 / 1e3, p.p90 / 1e3, p.p99 / 1e3, p.p999 / 1e3, p.max / 1e3);
  }
  if (!frame_ms.empty()) {
    const auto p = syn::bench::percentiles(frame_ms);
    std::printf("quadro CPU+present (ms): p50=%.2f p99=%.2f max=%.2f\n", p.p50, p.p99, p.max);
  }
  const bool pass = s.underruns.load() == 0;
  std::printf("RESULTADO: %s\n", pass ? "PASS (0 underruns)" : "FAIL (houve underruns)");
  return pass ? 0 : 1;
}
