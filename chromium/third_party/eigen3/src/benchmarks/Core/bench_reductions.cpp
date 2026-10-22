// Benchmarks for full reductions: sum, prod, minCoeff, maxCoeff, mean,
// norm, squaredNorm, lpNorm<1>, lpNorm<Infinity>.
//
// These are memory-bandwidth-bound for large vectors, so we report
// bytes processed rather than FLOPS.

#include <benchmark/benchmark.h>
#include <Eigen/Core>

using namespace Eigen;

// --- Vector reductions (1-D) ---

template <typename Scalar>
static void BM_VectorSum(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar s = v.sum();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorProd(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Constant(n, Scalar(1));
  // Use values near 1 to avoid overflow/underflow.
  v += Scalar(0.001) * Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar p = v.prod();
    benchmark::DoNotOptimize(p);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorMinCoeff(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar m = v.minCoeff();
    benchmark::DoNotOptimize(m);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorMaxCoeff(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar m = v.maxCoeff();
    benchmark::DoNotOptimize(m);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorMean(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar m = v.mean();
    benchmark::DoNotOptimize(m);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorSquaredNorm(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar s = v.squaredNorm();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorNorm(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar s = v.norm();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorLpNorm1(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar s = v.template lpNorm<1>();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_VectorLpNormInf(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, 1> v = Matrix<Scalar, Dynamic, 1>::Random(n);
  for (auto _ : state) {
    Scalar s = v.template lpNorm<Infinity>();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * sizeof(Scalar));
}

// --- Matrix reductions (2-D) ---

template <typename Scalar>
static void BM_MatrixSum(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, Dynamic> m = Matrix<Scalar, Dynamic, Dynamic>::Random(n, n);
  for (auto _ : state) {
    Scalar s = m.sum();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * n * sizeof(Scalar));
}

template <typename Scalar>
static void BM_MatrixNorm(benchmark::State& state) {
  const Index n = state.range(0);
  Matrix<Scalar, Dynamic, Dynamic> m = Matrix<Scalar, Dynamic, Dynamic>::Random(n, n);
  for (auto _ : state) {
    Scalar s = m.norm();
    benchmark::DoNotOptimize(s);
  }
  state.SetBytesProcessed(state.iterations() * n * n * sizeof(Scalar));
}

// --- Size configurations ---

// clang-format off
#define VECTOR_SIZES ->Arg(64)->Arg(256)->Arg(1024)->Arg(4096)->Arg(16384)->Arg(65536)->Arg(262144)->Arg(1048576)
#define MATRIX_SIZES ->Arg(8)->Arg(32)->Arg(64)->Arg(128)->Arg(256)->Arg(512)->Arg(1024)

// --- Register: float ---
BENCHMARK(BM_VectorSum<float>) VECTOR_SIZES ->Name("VectorSum_float");
BENCHMARK(BM_VectorProd<float>) VECTOR_SIZES ->Name("VectorProd_float");
BENCHMARK(BM_VectorMinCoeff<float>) VECTOR_SIZES ->Name("VectorMinCoeff_float");
BENCHMARK(BM_VectorMaxCoeff<float>) VECTOR_SIZES ->Name("VectorMaxCoeff_float");
BENCHMARK(BM_VectorMean<float>) VECTOR_SIZES ->Name("VectorMean_float");
BENCHMARK(BM_VectorSquaredNorm<float>) VECTOR_SIZES ->Name("VectorSquaredNorm_float");
BENCHMARK(BM_VectorNorm<float>) VECTOR_SIZES ->Name("VectorNorm_float");
BENCHMARK(BM_VectorLpNorm1<float>) VECTOR_SIZES ->Name("VectorLpNorm1_float");
BENCHMARK(BM_VectorLpNormInf<float>) VECTOR_SIZES ->Name("VectorLpNormInf_float");
BENCHMARK(BM_MatrixSum<float>) MATRIX_SIZES ->Name("MatrixSum_float");
BENCHMARK(BM_MatrixNorm<float>) MATRIX_SIZES ->Name("MatrixNorm_float");

// --- Register: double ---
BENCHMARK(BM_VectorSum<double>) VECTOR_SIZES ->Name("VectorSum_double");
BENCHMARK(BM_VectorProd<double>) VECTOR_SIZES ->Name("VectorProd_double");
BENCHMARK(BM_VectorMinCoeff<double>) VECTOR_SIZES ->Name("VectorMinCoeff_double");
BENCHMARK(BM_VectorMaxCoeff<double>) VECTOR_SIZES ->Name("VectorMaxCoeff_double");
BENCHMARK(BM_VectorMean<double>) VECTOR_SIZES ->Name("VectorMean_double");
BENCHMARK(BM_VectorSquaredNorm<double>) VECTOR_SIZES ->Name("VectorSquaredNorm_double");
BENCHMARK(BM_VectorNorm<double>) VECTOR_SIZES ->Name("VectorNorm_double");
BENCHMARK(BM_VectorLpNorm1<double>) VECTOR_SIZES ->Name("VectorLpNorm1_double");
BENCHMARK(BM_VectorLpNormInf<double>) VECTOR_SIZES ->Name("VectorLpNormInf_double");
BENCHMARK(BM_MatrixSum<double>) MATRIX_SIZES ->Name("MatrixSum_double");
BENCHMARK(BM_MatrixNorm<double>) MATRIX_SIZES ->Name("MatrixNorm_double");

#undef VECTOR_SIZES
#undef MATRIX_SIZES
// clang-format on
