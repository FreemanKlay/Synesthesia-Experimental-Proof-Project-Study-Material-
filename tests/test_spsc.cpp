// Estresse da fila SPSC com duas threads reais: ordem, integridade e perda zero.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include "check.hpp"
#include "syn/rng.hpp"
#include "syn/spsc_queue.hpp"

static void test_single_thread() {
  syn::SpscQueue<int> q(4);
  int x;
  CHECK(!q.try_pop(x));
  for (int i = 0; i < 4; ++i) CHECK(q.try_push(i));
  CHECK(!q.try_push(99));                       // cheia
  for (int i = 0; i < 4; ++i) { CHECK(q.try_pop(x)); CHECK(x == i); }
  CHECK(!q.try_pop(x));
  // wrap-around muitas vezes
  for (int i = 0; i < 1000; ++i) { CHECK(q.try_push(i)); CHECK(q.try_pop(x)); CHECK(x == i); }
  // blocos
  int in[3] = {7, 8, 9}, out[8];
  CHECK(q.push_n(in, 3) == 3);
  CHECK(q.push_n(in, 3) == 1);                  // so cabe 1
  CHECK(q.pop_n(out, 8) == 4);
  CHECK(out[0] == 7 && out[2] == 9 && out[3] == 7);
}

static void test_two_threads_items(std::uint64_t n) {
  syn::SpscQueue<std::uint64_t> q(1024);
  std::atomic<bool> ok{true};
  std::thread prod([&] { for (std::uint64_t i = 0; i < n; ++i) while (!q.try_push(i)) std::this_thread::yield(); });
  std::thread cons([&] {
    std::uint64_t v, expect = 0;
    while (expect < n) {
      if (q.try_pop(v)) { if (v != expect) { ok = false; return; } ++expect; }
      else std::this_thread::yield();
    }
  });
  prod.join(); cons.join();
  CHECK(ok.load());
}

static void test_two_threads_blocks(std::uint64_t n) {
  syn::SpscQueue<float> q(4096);
  std::atomic<bool> ok{true};
  std::thread prod([&] {
    syn::Rng r(1);
    std::vector<float> buf(512);
    std::uint64_t next = 0;
    while (next < n) {
      const std::size_t want = 1 + r.below(512);
      const std::size_t k = static_cast<std::size_t>(std::min<std::uint64_t>(want, n - next));
      for (std::size_t i = 0; i < k; ++i) buf[i] = static_cast<float>((next + i) % 100000);
      std::size_t done = 0;
      while (done < k) { done += q.push_n(buf.data() + done, k - done); if (done < k) std::this_thread::yield(); }
      next += k;
    }
  });
  std::thread cons([&] {
    syn::Rng r(2);
    std::vector<float> buf(512);
    std::uint64_t got = 0;
    while (got < n) {
      const std::size_t k = q.pop_n(buf.data(), 1 + r.below(512));
      if (k == 0) { std::this_thread::yield(); continue; }
      for (std::size_t i = 0; i < k; ++i)
        if (buf[i] != static_cast<float>((got + i) % 100000)) { ok = false; return; }
      got += k;
    }
  });
  prod.join(); cons.join();
  CHECK(ok.load());
}

int main(int argc, char** argv) {
  const std::uint64_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 20'000'000ULL;
  test_single_thread();
  test_two_threads_items(n);
  test_two_threads_blocks(n);
  std::printf("spsc ok (%llu itens por teste)\n", (unsigned long long)n);
  return 0;
}
