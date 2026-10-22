#include <benchmark/benchmark.h>
#include <Eigen/Dense>

using namespace Eigen;

// ---------- TRSV: triangular solve with single RHS vector ----------

template <typename Scalar, unsigned int Mode>
static void BM_TRSV(benchmark::State& state) {
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  using Vec = Matrix<Scalar, Dynamic, 1>;
  const Index n = state.range(0);
  Mat A = Mat::Random(n, n);
  // Make diagonally dominant to ensure well-conditioned triangular part.
  A.diagonal().array() += Scalar(n);
  Vec x = Vec::Random(n);
  Vec b = x;
  for (auto _ : state) {
    x = b;
    A.template triangularView<Mode>().solveInPlace(x);
    benchmark::DoNotOptimize(x.data());
  }
  state.SetItemsProcessed(state.iterations() * n * n);
}

// ---------- TRSM: triangular solve with multiple RHS (OnTheLeft) ----------

template <typename Scalar, unsigned int Mode>
static void BM_TRSM_Left(benchmark::State& state) {
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  const Index n = state.range(0);
  const Index nrhs = state.range(1);
  Mat A = Mat::Random(n, n);
  A.diagonal().array() += Scalar(n);
  Mat X = Mat::Random(n, nrhs);
  Mat B = X;
  for (auto _ : state) {
    X = B;
    A.template triangularView<Mode>().solveInPlace(X);
    benchmark::DoNotOptimize(X.data());
  }
  state.SetItemsProcessed(state.iterations() * n * n * nrhs);
}

// ---------- TRSM: triangular solve with multiple RHS (OnTheRight) ----------

template <typename Scalar, unsigned int Mode>
static void BM_TRSM_Right(benchmark::State& state) {
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  const Index n = state.range(0);
  const Index nrhs = state.range(1);
  Mat A = Mat::Random(n, n);
  A.diagonal().array() += Scalar(n);
  Mat X = Mat::Random(nrhs, n);
  Mat B = X;
  for (auto _ : state) {
    X = B;
    A.template triangularView<Mode>().template solveInPlace<OnTheRight>(X);
    benchmark::DoNotOptimize(X.data());
  }
  state.SetItemsProcessed(state.iterations() * n * n * nrhs);
}

// ---------- TRSV benchmarks ----------
// Only Lower is benchmarked; Upper exercises the same kernel via transposed storage.

BENCHMARK(BM_TRSV<float, Lower>)->Arg(32)->Arg(128)->Arg(512)->Name("TRSV_float_Lower");
BENCHMARK(BM_TRSV<double, Lower>)->Arg(32)->Arg(128)->Arg(512)->Name("TRSV_double_Lower");

// ---------- TRSM Left benchmarks ----------

// clang-format off
BENCHMARK(BM_TRSM_Left<float, Lower>)->ArgsProduct({{64, 256, 512}, {1, 16, 64}})->Name("TRSM_Left_float_Lower");
BENCHMARK(BM_TRSM_Left<double, Lower>)->ArgsProduct({{64, 256, 512}, {1, 16, 64}})->Name("TRSM_Left_double_Lower");

// ---------- TRSM Right benchmarks ----------

BENCHMARK(BM_TRSM_Right<float, Lower>)->ArgsProduct({{64, 256, 512}, {1, 16, 64}})->Name("TRSM_Right_float_Lower");
BENCHMARK(BM_TRSM_Right<double, Lower>)->ArgsProduct({{64, 256, 512}, {1, 16, 64}})->Name("TRSM_Right_double_Lower");
// clang-format on
