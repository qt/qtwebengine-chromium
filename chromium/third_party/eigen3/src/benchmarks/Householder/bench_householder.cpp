// Benchmarks for Householder reflections.
//
// Tests makeHouseholder, makeHouseholderInPlace, applyHouseholderOnTheLeft,
// applyHouseholderOnTheRight, HouseholderSequence evaluation, and block
// Householder operations.

#include <benchmark/benchmark.h>
#include <Eigen/Householder>
#include <Eigen/QR>

using namespace Eigen;

// =============================================================================
// makeHouseholderInPlace: compute Householder reflector in-place.
// =============================================================================

template <typename Scalar>
static void BM_MakeHouseholderInPlace(benchmark::State& state) {
  const Index n = state.range(0);
  using Vec = Matrix<Scalar, Dynamic, 1>;
  using RealScalar = typename NumTraits<Scalar>::Real;

  Vec v = Vec::Random(n);
  Vec v_copy = v;
  Scalar tau;
  RealScalar beta;
  for (auto _ : state) {
    v = v_copy;
    v.makeHouseholderInPlace(tau, beta);
    benchmark::DoNotOptimize(tau);
    benchmark::DoNotOptimize(beta);
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// makeHouseholder: compute Householder reflector, storing essential part
// separately.
// =============================================================================

template <typename Scalar>
static void BM_MakeHouseholder(benchmark::State& state) {
  const Index n = state.range(0);
  using Vec = Matrix<Scalar, Dynamic, 1>;
  using RealScalar = typename NumTraits<Scalar>::Real;

  Vec v = Vec::Random(n);
  Vec essential(n - 1);
  Scalar tau;
  RealScalar beta;
  for (auto _ : state) {
    v.makeHouseholder(essential, tau, beta);
    benchmark::DoNotOptimize(essential.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// applyHouseholderOnTheLeft: apply H = I - tau * v * v^* from the left.
// =============================================================================

template <typename Scalar>
static void BM_ApplyHouseholderOnTheLeft(benchmark::State& state) {
  const Index rows = state.range(0);
  const Index cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  using Vec = Matrix<Scalar, Dynamic, 1>;
  using RealScalar = typename NumTraits<Scalar>::Real;

  // Create a Householder reflector from a random vector.
  Vec v = Vec::Random(rows);
  Vec essential(rows - 1);
  Scalar tau;
  RealScalar beta;
  v.makeHouseholder(essential, tau, beta);

  Mat A = Mat::Random(rows, cols);
  Mat A_copy = A;
  Vec workspace(cols);
  for (auto _ : state) {
    A = A_copy;
    A.applyHouseholderOnTheLeft(essential, tau, workspace.data());
    benchmark::DoNotOptimize(A.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// applyHouseholderOnTheRight: apply H = I - tau * v * v^* from the right.
// =============================================================================

template <typename Scalar>
static void BM_ApplyHouseholderOnTheRight(benchmark::State& state) {
  const Index rows = state.range(0);
  const Index cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  using Vec = Matrix<Scalar, Dynamic, 1>;
  using RealScalar = typename NumTraits<Scalar>::Real;

  // Create a Householder reflector from a random vector.
  Vec v = Vec::Random(cols);
  Vec essential(cols - 1);
  Scalar tau;
  RealScalar beta;
  v.makeHouseholder(essential, tau, beta);

  Mat A = Mat::Random(rows, cols);
  Mat A_copy = A;
  Vec workspace(rows);
  for (auto _ : state) {
    A = A_copy;
    A.applyHouseholderOnTheRight(essential, tau, workspace.data());
    benchmark::DoNotOptimize(A.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// HouseholderSequence evalTo: materialize Q = H_0 * H_1 * ... * H_{k-1}
// as a dense matrix.
// =============================================================================

template <typename Scalar>
static void BM_HouseholderSequence_EvalTo(benchmark::State& state) {
  const Index rows = state.range(0);
  const Index cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;

  // Build a Householder sequence via QR factorization.
  Mat A = Mat::Random(rows, cols);
  HouseholderQR<Mat> qr(A);
  Mat Q(rows, rows);
  for (auto _ : state) {
    Q = qr.householderQ();
    benchmark::DoNotOptimize(Q.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// HouseholderSequence applyOnTheLeft: apply Q from the left to a matrix
// without materializing Q.
// =============================================================================

template <typename Scalar>
static void BM_HouseholderSequence_ApplyLeft(benchmark::State& state) {
  const Index rows = state.range(0);
  const Index cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;

  Mat A = Mat::Random(rows, cols);
  HouseholderQR<Mat> qr(A);
  Mat B = Mat::Random(rows, cols);
  Mat C(rows, cols);
  for (auto _ : state) {
    C.noalias() = qr.householderQ() * B;
    benchmark::DoNotOptimize(C.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// HouseholderSequence applyOnTheRight: apply Q from the right to a matrix
// without materializing Q.
// =============================================================================

template <typename Scalar>
static void BM_HouseholderSequence_ApplyRight(benchmark::State& state) {
  const Index rows = state.range(0);
  const Index cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;

  Mat A = Mat::Random(rows, cols);
  HouseholderQR<Mat> qr(A);
  Mat B = Mat::Random(cols, rows);
  Mat C(cols, rows);
  for (auto _ : state) {
    C.noalias() = B * qr.householderQ();
    benchmark::DoNotOptimize(C.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// HouseholderSequence adjoint apply: apply Q^* from the left (common in
// least-squares solves: Q^* * b).
// =============================================================================

template <typename Scalar>
static void BM_HouseholderSequence_AdjointApplyLeft(benchmark::State& state) {
  const Index rows = state.range(0);
  const Index cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  using Vec = Matrix<Scalar, Dynamic, 1>;

  Mat A = Mat::Random(rows, cols);
  HouseholderQR<Mat> qr(A);
  Vec b = Vec::Random(rows);
  Vec c(rows);
  for (auto _ : state) {
    c.noalias() = qr.householderQ().adjoint() * b;
    benchmark::DoNotOptimize(c.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// Block Householder: make_block_householder_triangular_factor.
// =============================================================================

template <typename Scalar>
static void BM_BlockHouseholder_TriangularFactor(benchmark::State& state) {
  const Index n = state.range(0);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  using Vec = Matrix<Scalar, Dynamic, 1>;

  // Build Householder vectors via QR.
  Mat A = Mat::Random(n, n);
  HouseholderQR<Mat> qr(A);
  const Mat& qrMat = qr.matrixQR();
  Vec hCoeffs = qr.hCoeffs();

  // Use the full set of vectors.
  Index nbVecs = (std::min)(n, n);
  Mat vectors = qrMat.leftCols(nbVecs);
  Mat T(nbVecs, nbVecs);
  for (auto _ : state) {
    internal::make_block_householder_triangular_factor(T, vectors, hCoeffs.head(nbVecs));
    benchmark::DoNotOptimize(T.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// Block Householder: apply_block_householder_on_the_left.
// =============================================================================

template <typename Scalar>
static void BM_BlockHouseholder_ApplyLeft(benchmark::State& state) {
  const Index n = state.range(0);
  const Index rhs_cols = state.range(1);
  using Mat = Matrix<Scalar, Dynamic, Dynamic>;
  using Vec = Matrix<Scalar, Dynamic, 1>;

  // Build Householder vectors via QR.
  Mat A = Mat::Random(n, n);
  HouseholderQR<Mat> qr(A);
  const Mat& qrMat = qr.matrixQR();
  Vec hCoeffs = qr.hCoeffs();

  // Use a block of reflectors.
  Index nbVecs = (std::min)(n, Index(48));
  Mat vectors = qrMat.block(0, 0, n, nbVecs);

  Mat B = Mat::Random(n, rhs_cols);
  Mat B_copy = B;
  for (auto _ : state) {
    B = B_copy;
    internal::apply_block_householder_on_the_left(B, vectors, hCoeffs.head(nbVecs), true);
    benchmark::DoNotOptimize(B.data());
  }
  state.SetItemsProcessed(state.iterations());
}

// =============================================================================
// Size configurations
// =============================================================================

static void VectorSizes(::benchmark::Benchmark* b) {
  for (int n : {8, 16, 32, 64, 128, 256, 512, 1024, 4096}) b->Arg(n);
}

static void SquareSizes(::benchmark::Benchmark* b) {
  for (int n : {32, 48, 64, 80, 96, 112, 128, 160, 192, 256, 384, 512, 768, 1024}) b->Args({n, n});
}

// Fine-grained sizes around the blocking threshold to find the crossover point.
static void SquareSizesFine(::benchmark::Benchmark* b) {
  for (int n : {32, 40, 48, 56, 64, 72, 80, 88, 96, 112, 128, 160, 192, 256}) b->Args({n, n});
}

// Rectangular: many rows, fewer columns (m_length = cols, dst is rows x rows).
static void RectApplyRight(::benchmark::Benchmark* b) {
  // Square
  for (int n : {48, 64, 96, 128, 256, 512, 1024}) b->Args({n, n});
  // Wide dst * narrow Q: dst is (rows x rows), Q is (cols x cols), so rows > cols.
  b->Args({256, 64});
  b->Args({256, 128});
  b->Args({512, 64});
  b->Args({512, 128});
  b->Args({1024, 64});
  b->Args({1024, 128});
  b->Args({1024, 256});
}

static void RectSizes(::benchmark::Benchmark* b) {
  // Square
  for (int n : {32, 64, 128, 256, 512, 1024}) b->Args({n, n});
  // Tall-thin
  b->Args({1000, 32});
  b->Args({1000, 100});
  b->Args({10000, 32});
  b->Args({10000, 100});
}

static void BlockSizes(::benchmark::Benchmark* b) {
  for (int n : {64, 128, 256, 512, 1024}) {
    b->Args({n, n});
    b->Args({n, 32});
  }
}

// =============================================================================
// Register benchmarks: float
// =============================================================================

BENCHMARK(BM_MakeHouseholderInPlace<float>)->Apply(VectorSizes)->Name("MakeHouseholderInPlace_float");
BENCHMARK(BM_MakeHouseholder<float>)->Apply(VectorSizes)->Name("MakeHouseholder_float");
BENCHMARK(BM_ApplyHouseholderOnTheLeft<float>)->Apply(RectSizes)->Name("ApplyHouseholderOnTheLeft_float");
BENCHMARK(BM_ApplyHouseholderOnTheRight<float>)->Apply(RectSizes)->Name("ApplyHouseholderOnTheRight_float");
BENCHMARK(BM_HouseholderSequence_EvalTo<float>)->Apply(SquareSizesFine)->Name("HouseholderSequence_EvalTo_float");
BENCHMARK(BM_HouseholderSequence_ApplyLeft<float>)->Apply(RectSizes)->Name("HouseholderSequence_ApplyLeft_float");
BENCHMARK(BM_HouseholderSequence_ApplyRight<float>)
    ->Apply(RectApplyRight)
    ->Name("HouseholderSequence_ApplyRight_float");
BENCHMARK(BM_HouseholderSequence_AdjointApplyLeft<float>)
    ->Apply(RectSizes)
    ->Name("HouseholderSequence_AdjointApplyLeft_float");
BENCHMARK(BM_BlockHouseholder_TriangularFactor<float>)
    ->Apply(VectorSizes)
    ->Name("BlockHouseholder_TriangularFactor_float");
BENCHMARK(BM_BlockHouseholder_ApplyLeft<float>)->Apply(BlockSizes)->Name("BlockHouseholder_ApplyLeft_float");

// =============================================================================
// Register benchmarks: double
// =============================================================================

BENCHMARK(BM_MakeHouseholderInPlace<double>)->Apply(VectorSizes)->Name("MakeHouseholderInPlace_double");
BENCHMARK(BM_MakeHouseholder<double>)->Apply(VectorSizes)->Name("MakeHouseholder_double");
BENCHMARK(BM_ApplyHouseholderOnTheLeft<double>)->Apply(RectSizes)->Name("ApplyHouseholderOnTheLeft_double");
BENCHMARK(BM_ApplyHouseholderOnTheRight<double>)->Apply(RectSizes)->Name("ApplyHouseholderOnTheRight_double");
BENCHMARK(BM_HouseholderSequence_EvalTo<double>)->Apply(SquareSizesFine)->Name("HouseholderSequence_EvalTo_double");
BENCHMARK(BM_HouseholderSequence_ApplyLeft<double>)->Apply(RectSizes)->Name("HouseholderSequence_ApplyLeft_double");
BENCHMARK(BM_HouseholderSequence_ApplyRight<double>)
    ->Apply(RectApplyRight)
    ->Name("HouseholderSequence_ApplyRight_double");
BENCHMARK(BM_HouseholderSequence_AdjointApplyLeft<double>)
    ->Apply(RectSizes)
    ->Name("HouseholderSequence_AdjointApplyLeft_double");
BENCHMARK(BM_BlockHouseholder_TriangularFactor<double>)
    ->Apply(VectorSizes)
    ->Name("BlockHouseholder_TriangularFactor_double");
BENCHMARK(BM_BlockHouseholder_ApplyLeft<double>)->Apply(BlockSizes)->Name("BlockHouseholder_ApplyLeft_double");

// =============================================================================
// Register benchmarks: std::complex<double>
// =============================================================================

BENCHMARK(BM_MakeHouseholderInPlace<std::complex<double>>)
    ->Apply(VectorSizes)
    ->Name("MakeHouseholderInPlace_complexdouble");
BENCHMARK(BM_ApplyHouseholderOnTheLeft<std::complex<double>>)
    ->Apply(RectSizes)
    ->Name("ApplyHouseholderOnTheLeft_complexdouble");
BENCHMARK(BM_HouseholderSequence_EvalTo<std::complex<double>>)
    ->Apply(SquareSizes)
    ->Name("HouseholderSequence_EvalTo_complexdouble");
BENCHMARK(BM_HouseholderSequence_ApplyLeft<std::complex<double>>)
    ->Apply(SquareSizes)
    ->Name("HouseholderSequence_ApplyLeft_complexdouble");
