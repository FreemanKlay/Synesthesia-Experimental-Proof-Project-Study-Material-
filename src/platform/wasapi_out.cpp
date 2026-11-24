#include "syn/platform/wasapi_out.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <future>
#include <thread>
#include "syn/bench.hpp"

namespace syn::platform {

namespace {
template <class T> void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }
constexpr std::size_t kCbCapacity = 1u << 20;  // 1M callbacks (~2,9 h a 10 ms)
}

struct WasapiOut::Impl {
  AudioQueue& q;
  std::uint32_t sample_rate;
  std::thread th;
  std::atomic<bool> stopping{false};
  bool running = false;

  IMMDeviceEnumerator* en = nullptr;
  IMMDevice* dev = nullptr;
  IAudioClient* cl = nullptr;
  IAudioRenderClient* rc = nullptr;
  HANDLE event = nullptr;

  AudioInfo info;
  AudioStats stats;
  std::vector<std::uint32_t> cb_ns;
  std::uint64_t cb_idx = 0;  // so a thread de audio escreve; lido apos stop()

  Impl(AudioQueue& queue, std::uint32_t sr) : q(queue), sample_rate(sr), cb_ns(kCbCapacity) {}

  static std::string hr_msg(const char* what, HRESULT hr) {
    char b[160];
    std::snprintf(b, sizeof(b), "%s (HRESULT 0x%08lX)", what, static_cast<unsigned long>(hr));
    return b;
  }

  bool init(std::string& err) {
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en));
    if (FAILED(hr)) { err = hr_msg("CoCreateInstance(MMDeviceEnumerator)", hr); return false; }
    hr = en->GetDefaultAudioEndpoint(eRender, eConsole, &dev);
    if (FAILED(hr)) { err = hr_msg("GetDefaultAudioEndpoint", hr); return false; }
    hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&cl));
    if (FAILED(hr)) { err = hr_msg("Activate(IAudioClient)", hr); return false; }

    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = sample_rate;
    fmt.wBitsPerSample = 32;
    fmt.nBlockAlign = static_cast<WORD>(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.cbSize = 0;

    REFERENCE_TIME def_period = 0, min_period = 0;
    cl->GetDevicePeriod(&def_period, &min_period);

    // AUTOCONVERTPCM + SRC: o motor do Windows converte do nosso formato para o do dispositivo.
    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    hr = cl->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, def_period, 0, &fmt, nullptr);
    if (FAILED(hr)) { err = hr_msg("IAudioClient::Initialize", hr); return false; }

    event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) { err = "CreateEvent falhou"; return false; }
    hr = cl->SetEventHandle(event);
    if (FAILED(hr)) { err = hr_msg("SetEventHandle", hr); return false; }

    UINT32 buf_frames = 0;
    cl->GetBufferSize(&buf_frames);
    REFERENCE_TIME stream_lat = 0;
    cl->GetStreamLatency(&stream_lat);
    hr = cl->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&rc));
    if (FAILED(hr)) { err = hr_msg("GetService(IAudioRenderClient)", hr); return false; }

    info.sample_rate = sample_rate;
    info.channels = 2;
    info.buffer_frames = buf_frames;
    info.period_ms = static_cast<double>(def_period) / 10000.0;
    info.buffer_ms = 1000.0 * buf_frames / sample_rate;
    info.stream_latency_ms = static_cast<double>(stream_lat) / 10000.0;

    // pre-enche com silencio e inicia
    BYTE* data = nullptr;
    hr = rc->GetBuffer(buf_frames, &data);
    if (SUCCEEDED(hr)) rc->ReleaseBuffer(buf_frames, AUDCLNT_BUFFERFLAGS_SILENT);
    hr = cl->Start();
    if (FAILED(hr)) { err = hr_msg("IAudioClient::Start", hr); return false; }
    return true;
  }

  void release_all() {
    safe_release(rc);
    safe_release(cl);
    safe_release(dev);
    safe_release(en);
    if (event) { CloseHandle(event); event = nullptr; }
  }

  void run(std::promise<std::string> ready) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::string err;
    const bool ok = init(err);
    ready.set_value(err);
    if (!ok) { release_all(); if (SUCCEEDED(co)) CoUninitialize(); return; }

    DWORD task = 0;
    HANDLE mm = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);

    const std::uint32_t buf = info.buffer_frames;
    const std::uint64_t late_thresh_ns = static_cast<std::uint64_t>(info.period_ms * 2.0 * 1e6);
    std::uint64_t prev_wake = syn::bench::now_ns();

    while (!stopping.load(std::memory_order_relaxed)) {
      const DWORD w = WaitForSingleObject(event, 200);
      if (w != WAIT_OBJECT_0) continue;
      const std::uint64_t t0 = syn::bench::now_ns();
      const std::uint64_t gap = t0 - prev_wake;
      prev_wake = t0;
      if (gap > late_thresh_ns) stats.late_wakeups.fetch_add(1, std::memory_order_relaxed);
      if (gap / 1000 > stats.max_wake_gap_us.load(std::memory_order_relaxed))
        stats.max_wake_gap_us.store(gap / 1000, std::memory_order_relaxed);

      UINT32 padding = 0;
      if (FAILED(cl->GetCurrentPadding(&padding))) continue;
      const UINT32 frames = buf - padding;
      if (frames == 0) continue;
      BYTE* data = nullptr;
      if (FAILED(rc->GetBuffer(frames, &data))) continue;

      auto* out = reinterpret_cast<StereoFrame*>(data);
      const std::uint64_t fill = q.size_approx();
      if (fill < stats.min_queue_fill.load(std::memory_order_relaxed))
        stats.min_queue_fill.store(fill, std::memory_order_relaxed);
      const std::size_t got = q.pop_n(out, frames);
      if (got < frames) {
        std::memset(out + got, 0, (frames - got) * sizeof(StereoFrame));
        stats.underruns.fetch_add(1, std::memory_order_relaxed);
        stats.starved_frames.fetch_add(frames - got, std::memory_order_relaxed);
      }
      rc->ReleaseBuffer(frames, 0);
      stats.fill_after_sum.fetch_add(q.size_approx(), std::memory_order_relaxed);
      stats.fill_after_samples.fetch_add(1, std::memory_order_relaxed);

      const std::uint64_t dt = syn::bench::now_ns() - t0;
      if (dt > stats.max_callback_ns.load(std::memory_order_relaxed))
        stats.max_callback_ns.store(dt, std::memory_order_relaxed);
      cb_ns[cb_idx & (kCbCapacity - 1)] = static_cast<std::uint32_t>(std::min<std::uint64_t>(dt, 0xFFFFFFFFu));
      ++cb_idx;
      stats.callbacks.fetch_add(1, std::memory_order_relaxed);
      stats.frames_rendered.fetch_add(frames, std::memory_order_relaxed);
    }

    cl->Stop();
    if (mm) AvRevertMmThreadCharacteristics(mm);
    release_all();
    if (SUCCEEDED(co)) CoUninitialize();
  }
};

WasapiOut::WasapiOut(AudioQueue& q, std::uint32_t sr) : impl_(std::make_unique<Impl>(q, sr)) {}
WasapiOut::~WasapiOut() { stop(); }

bool WasapiOut::start(std::string* error) {
  if (impl_->running) return true;
  std::promise<std::string> p;
  std::future<std::string> f = p.get_future();
  impl_->stopping = false;
  impl_->th = std::thread([this, pr = std::move(p)]() mutable { impl_->run(std::move(pr)); });
  const std::string err = f.get();
  if (!err.empty()) {
    if (impl_->th.joinable()) impl_->th.join();
    if (error) *error = err;
    return false;
  }
  impl_->running = true;
  return true;
}

void WasapiOut::stop() {
  if (!impl_ || !impl_->running) return;
  impl_->stopping = true;
  if (impl_->th.joinable()) impl_->th.join();
  impl_->running = false;
}

AudioInfo WasapiOut::info() const { return impl_->info; }
const AudioStats& WasapiOut::stats() const { return impl_->stats; }
std::vector<std::uint32_t> WasapiOut::callback_times_ns() const {
  const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(impl_->cb_idx, kCbCapacity));
  return std::vector<std::uint32_t>(impl_->cb_ns.begin(), impl_->cb_ns.begin() + static_cast<std::ptrdiff_t>(n));
}

} // namespace syn::platform
