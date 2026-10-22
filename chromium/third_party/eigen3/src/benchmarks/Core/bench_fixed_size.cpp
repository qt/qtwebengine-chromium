// Benchmarks for fixed-size matrix operations (2x2, 3x3, 4x4).
// Critical for PCL, ROS, Sophus, Drake which use small matrices extensively.

#include <benchmark/benchmark.h>
#include <Eigen/Core>
#include <Eigen/LU>

using namespace Eigen;

#ifndef SCALAR
#define SCALAR float
#endif

typedef SCALAR Scalar;

// --- Fixed-size GEMM ---
template <int N>
static void BM_FixedGemm(benchmark::State& state) {
  typedef Matrix<Scalar, N, N> Mat;
  Mat a = Mat::Random();
  Mat b = Mat::Random();
  Mat c;

  for (auto _ : state) {
    c.noalias() = a * b;
    benchmark::DoNotOptimize(c.data());
    benchmark::ClobberMemory();
  }
  state.counters["GFLOPS"] =
      benchmark::Counter(2.0 * N * N * N, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::kIs1000);
}

// --- Fixed-size inverse ---
template <int N>
static void BM_FixedInverse(benchmark::State& state) {
  typedef Matrix<Scalar, N, N> Mat;
  Mat a = Mat::Random();
  // Make well-conditioned.
  a = a * a.transpose() + Mat::Identity();
  Mat result;

  for (auto _ : state) {
    result = a.inverse();
    benchmark::DoNotOptimize(result.data());
    benchmark::ClobberMemory();
  }
}

// --- Fixed-size determinant ---
template <int N>
static void BM_FixedDeterminant(benchmark::State& state) {
  typedef Matrix<Scalar, N, N> Mat;
  Mat a = Mat::Random();
  Scalar result;

  for (auto _ : state) {
    result = a.determinant();
    benchmark::DoNotOptimize(&result);
    benchmark::ClobberMemory();
  }
}

// --- Batch transform: Matrix4 * Matrix<4,N> ---
static void BM_BatchTransform4xN(benchmark::State& state) {
  int N = state.range(0);
  typedef Matrix<Scalar, 4, 4> Mat4;
  typedef Matrix<Scalar, 4, Dynamic> MatXN;

  Mat4 transform = Mat4::Random();
  MatXN points = MatXN::Random(4, N);
  MatXN result(4, N);

  for (auto _ : state) {
    result.noalias() = transform * points;
    benchmark::DoNotOptimize(result.data());
    benchmark::ClobberMemory();
  }
  state.counters["GFLOPS"] =
      benchmark::Counter(2.0 * 4 * 4 * N, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::kIs1000);
}

// --- Fixed 3x3 batch operations (common in point cloud processing) ---
static void BM_Batch3x3Gemm(benchmark::State& state) {
  int count = state.range(0);
  typedef Matrix<Scalar, 3, 3> Mat3;

  std::vector<Mat3> a(count), b(count), c(count);
  for (int i = 0; i < count; ++i) {
    a[i] = Mat3::Random();
    b[i] = Mat3::Random();
  }

  for (auto _ : state) {
    for (int i = 0; i < count; ++i) {
      c[i].noalias() = a[i] * b[i];
    }
    benchmark::DoNotOptimize(c.data());
    benchmark::ClobberMemory();
  }
  state.counters["GFLOPS"] =
      benchmark::Counter(2.0 * 27 * count, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::kIs1000);
}

// Fixed-size GEMM
BENCHMARK(BM_FixedGemm<2>)->Name("FixedGemm_2x2");
BENCHMARK(BM_FixedGemm<3>)->Name("FixedGemm_3x3");
BENCHMARK(BM_FixedGemm<4>)->Name("FixedGemm_4x4");

// Fixed-size inverse
BENCHMARK(BM_FixedInverse<2>)->Name("FixedInverse_2x2");
BENCHMARK(BM_FixedInverse<3>)->Name("FixedInverse_3x3");
BENCHMARK(BM_FixedInverse<4>)->Name("FixedInverse_4x4");

// Fixed-size determinant
BENCHMARK(BM_FixedDeterminant<2>)->Name("FixedDet_2x2");
BENCHMARK(BM_FixedDeterminant<3>)->Name("FixedDet_3x3");
BENCHMARK(BM_FixedDeterminant<4>)->Name("FixedDet_4x4");

// Batch 4xN transform
BENCHMARK(BM_BatchTransform4xN)->Arg(1)->Arg(4)->Arg(8)->Arg(16)->Arg(64);

// Batch 3x3 GEMM
BENCHMARK(BM_Batch3x3Gemm)->Arg(100)->Arg(1000)->Arg(10000);
