#pragma once
// Fila SPSC (um produtor, um consumidor) lock-free, de capacidade fixa (potencia de 2).
//  - Sem alocacao nem locks em push/pop: segura para a thread de audio (so consome).
//  - Indices monotonicos de 64 bits (sem problema de wrap pratico), mascarados no acesso.
//  - Cada lado tem sua propria cache line; cada lado guarda uma copia em cache do indice do
//    outro, e so recarrega (acquire) quando a fila parece cheia/vazia -> menos trafego de cache.
//  - Ordenacao: producer faz store(release) do tail depois de escrever o dado;
//    consumer faz load(acquire) do tail antes de ler. Simetrico para head.
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace syn {

inline constexpr std::size_t cache_line = 64;

template <class T>
class SpscQueue {
  static_assert(std::is_trivially_copyable_v<T>, "use tipos triviais (amostras, comandos POD)");
public:
  explicit SpscQueue(std::size_t capacity_pow2)
      : mask_(capacity_pow2 - 1), buf_(new T[capacity_pow2]()) {
    assert(capacity_pow2 >= 2 && (capacity_pow2 & (capacity_pow2 - 1)) == 0);
  }
  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  std::size_t capacity() const { return mask_ + 1; }

  // ---- lado produtor ----
  bool try_push(const T& v) {
    const std::uint64_t t = prod_.tail.load(std::memory_order_relaxed);
    if (t - prod_.head_cache > mask_) {
      prod_.head_cache = cons_.head.load(std::memory_order_acquire);
      if (t - prod_.head_cache > mask_) return false;
    }
    buf_[t & mask_] = v;
    prod_.tail.store(t + 1, std::memory_order_release);
    return true;
  }
  // Empurra ate n itens; retorna quantos couberam.
  std::size_t push_n(const T* src, std::size_t n) {
    const std::uint64_t t = prod_.tail.load(std::memory_order_relaxed);
    std::size_t free_slots = capacity() - static_cast<std::size_t>(t - prod_.head_cache);
    if (free_slots < n) {
      prod_.head_cache = cons_.head.load(std::memory_order_acquire);
      free_slots = capacity() - static_cast<std::size_t>(t - prod_.head_cache);
    }
    const std::size_t k = n < free_slots ? n : free_slots;
    for (std::size_t i = 0; i < k; ++i) buf_[(t + i) & mask_] = src[i];
    prod_.tail.store(t + k, std::memory_order_release);
    return k;
  }

  // ---- lado consumidor ----
  bool try_pop(T& out) {
    const std::uint64_t h = cons_.head.load(std::memory_order_relaxed);
    if (h == cons_.tail_cache) {
      cons_.tail_cache = prod_.tail.load(std::memory_order_acquire);
      if (h == cons_.tail_cache) return false;
    }
    out = buf_[h & mask_];
    cons_.head.store(h + 1, std::memory_order_release);
    return true;
  }
  // Retira ate n itens; retorna quantos havia.
  std::size_t pop_n(T* dst, std::size_t n) {
    const std::uint64_t h = cons_.head.load(std::memory_order_relaxed);
    std::size_t avail = static_cast<std::size_t>(cons_.tail_cache - h);
    if (avail < n) {
      cons_.tail_cache = prod_.tail.load(std::memory_order_acquire);
      avail = static_cast<std::size_t>(cons_.tail_cache - h);
    }
    const std::size_t k = n < avail ? n : avail;
    for (std::size_t i = 0; i < k; ++i) dst[i] = buf_[(h + i) & mask_];
    cons_.head.store(h + k, std::memory_order_release);
    return k;
  }

  // Aproximado (so para telemetria): pode estar defasado.
  std::size_t size_approx() const {
    const auto t = prod_.tail.load(std::memory_order_acquire);
    const auto h = cons_.head.load(std::memory_order_acquire);
    return static_cast<std::size_t>(t - h);
  }

private:
  struct alignas(cache_line) Prod { std::atomic<std::uint64_t> tail{0}; std::uint64_t head_cache = 0; };
  struct alignas(cache_line) Cons { std::atomic<std::uint64_t> head{0}; std::uint64_t tail_cache = 0; };

  const std::size_t mask_;
  std::unique_ptr<T[]> buf_;
  Prod prod_;
  Cons cons_;
};

} // namespace syn
