#include <benchmark/benchmark.h>
#include <Eigen/Core>

using namespace Eigen;

#ifndef SCALAR
#define SCALAR float
#endif

typedef SCALAR Scalar;
typedef Matrix<Scalar, Dynamic, Dynamic> Mat;

template <typename A, typename B, typename C>
EIGEN_DONT_INLINE void gemm(const A& a, const B& b, C& c) {
  c.noalias() += a * b;
}

static void BM_EigenGemm(benchmark::State& state) {
  int m = state.range(0);
  int n = state.range(1);
  int p = state.range(2);
  Mat a(m, p);
  a.setRandom();
  Mat b(p, n);
  b.setRandom();
  Mat c = Mat::Zero(m, n);
  for (auto _ : state) {
    c.setZero();
    gemm(a, b, c);
    benchmark::DoNotOptimize(c.data());
    benchmark::ClobberMemory();
  }
  state.counters["GFLOPS"] =
      benchmark::Counter(2.0 * m * n * p, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::kIs1000);
}

// clang-format off
BENCHMARK(BM_EigenGemm)
    ->Args({8, 8, 8})->Args({16, 16, 16})->Args({32, 32, 32})
    ->Args({64, 64, 64})->Args({96, 96, 96})->Args({128, 128, 128})
    ->Args({160, 160, 160})->Args({192, 192, 192})->Args({224, 224, 224})
    ->Args({256, 256, 256})->Args({288, 288, 288})->Args({320, 320, 320})
    ->Args({384, 384, 384})->Args({448, 448, 448})->Args({512, 512, 512})
    ->Args({768, 768, 768})->Args({1024, 1024, 1024})->Args({1536, 1536, 1536})
    ->Args({2048, 2048, 2048})->Args({4096, 4096, 4096})
    // Non-square sizes
    ->Args({64, 64, 1024})->Args({1024, 64, 64})->Args({64, 1024, 64})
    ->Args({256, 256, 1024})->Args({1024, 256, 256});
// clang-format on

#ifdef HAVE_BLAS
extern "C" {
#include <Eigen/src/misc/blas.h>
}

static void BM_BlasGemm(benchmark::State& state) {
  int m = state.range(0);
  int n = state.range(1);
  int p = state.range(2);
  Mat a(m, p);
  a.setRandom();
  Mat b(p, n);
  b.setRandom();
  Mat c = Mat::Zero(m, n);
  char notrans = 'N';
  Scalar one = 1, zero = 0;
  for (auto _ : state) {
    c.setZero();
    if constexpr (std::is_same_v<Scalar, float>) {
      sgemm_(&notrans, &notrans, &m, &n, &p, &one, a.data(), &m, b.data(), &p, &one, c.data(), &m);
    } else {
      dgemm_(&notrans, &notrans, &m, &n, &p, &one, a.data(), &m, b.data(), &p, &one, c.data(), &m);
    }
    benchmark::DoNotOptimize(c.data());
    benchmark::ClobberMemory();
  }
  state.counters["GFLOPS"] =
      benchmark::Counter(2.0 * m * n * p, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::kIs1000);
}
// clang-format off
BENCHMARK(BM_BlasGemm)
    ->Args({8, 8, 8})->Args({16, 16, 16})->Args({32, 32, 32})
    ->Args({64, 64, 64})->Args({96, 96, 96})->Args({128, 128, 128})
    ->Args({160, 160, 160})->Args({192, 192, 192})->Args({224, 224, 224})
    ->Args({256, 256, 256})->Args({288, 288, 288})->Args({320, 320, 320})
    ->Args({384, 384, 384})->Args({448, 448, 448})->Args({512, 512, 512})
    ->Args({768, 768, 768})->Args({1024, 1024, 1024})->Args({1536, 1536, 1536})
    ->Args({2048, 2048, 2048})->Args({4096, 4096, 4096})
    // Non-square sizes
    ->Args({64, 64, 1024})->Args({1024, 64, 64})->Args({64, 1024, 64})
    ->Args({256, 256, 1024})->Args({1024, 256, 256});
// clang-format on
#endif
