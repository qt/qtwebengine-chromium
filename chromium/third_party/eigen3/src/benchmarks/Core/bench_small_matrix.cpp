// SPDX-FileCopyrightText: The Eigen Authors
// SPDX-License-Identifier: MPL-2.0

#include <benchmark/benchmark.h>
#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/Cholesky>
#include <Eigen/QR>
#include <Eigen/SVD>
#include <Eigen/Eigenvalues>

using namespace Eigen;

// ============================================================================
// Fixed-size matrix multiply (the fundamental operation)
// ============================================================================

template <typename Scalar, int N>
static void BM_MatMul(benchmark::State& state) {
  Matrix<Scalar, N, N> a, b, c;
  a.setRandom();
  b.setRandom();
  for (auto _ : state) {
    c.noalias() = a * b;
    benchmark::DoNotOptimize(c.data());
  }
}

// Matrix-vector multiply
template <typename Scalar, int N>
static void BM_MatVec(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  Matrix<Scalar, N, 1> v, r;
  a.setRandom();
  v.setRandom();
  for (auto _ : state) {
    r.noalias() = a * v;
    benchmark::DoNotOptimize(r.data());
  }
}

// ============================================================================
// Fixed-size inverse (critical for transform operations)
// ============================================================================

template <typename Scalar, int N>
EIGEN_DONT_INLINE void do_inverse(const Matrix<Scalar, N, N>& a, Matrix<Scalar, N, N>& r) {
  r = a.inverse();
}

template <typename Scalar, int N>
static void BM_Inverse(benchmark::State& state) {
  Matrix<Scalar, N, N> a, r;
  a.setRandom();
  a += Matrix<Scalar, N, N>::Identity() * Scalar(N);  // ensure well-conditioned
  for (auto _ : state) {
    do_inverse(a, r);
    benchmark::DoNotOptimize(r.data());
  }
}

// ============================================================================
// Fixed-size determinant
// ============================================================================

template <typename Scalar, int N>
static void BM_Determinant(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  Scalar d;
  for (auto _ : state) {
    d = a.determinant();
    benchmark::DoNotOptimize(d);
  }
}

// ============================================================================
// LLT (Cholesky) — for SPD matrices (covariance, mass matrices)
// ============================================================================

template <typename Scalar, int N>
static void BM_LLT_Compute(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a = a.transpose() * a + Matrix<Scalar, N, N>::Identity();  // SPD
  LLT<Matrix<Scalar, N, N>> llt;
  for (auto _ : state) {
    llt.compute(a);
    benchmark::DoNotOptimize(&llt);
  }
}

template <typename Scalar, int N>
static void BM_LLT_Solve(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a = a.transpose() * a + Matrix<Scalar, N, N>::Identity();
  Matrix<Scalar, N, 1> b = Matrix<Scalar, N, 1>::Random();
  LLT<Matrix<Scalar, N, N>> llt(a);
  Matrix<Scalar, N, 1> x;
  for (auto _ : state) {
    x = llt.solve(b);
    benchmark::DoNotOptimize(x.data());
  }
}

// ============================================================================
// LDLT — for semi-definite matrices
// ============================================================================

template <typename Scalar, int N>
static void BM_LDLT_Compute(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a = a.transpose() * a + Matrix<Scalar, N, N>::Identity();
  LDLT<Matrix<Scalar, N, N>> ldlt;
  for (auto _ : state) {
    ldlt.compute(a);
    benchmark::DoNotOptimize(&ldlt);
  }
}

// ============================================================================
// PartialPivLU — for general square systems
// ============================================================================

template <typename Scalar, int N>
static void BM_PartialPivLU_Compute(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a += Matrix<Scalar, N, N>::Identity() * Scalar(N);
  PartialPivLU<Matrix<Scalar, N, N>> lu;
  for (auto _ : state) {
    lu.compute(a);
    benchmark::DoNotOptimize(lu.matrixLU().data());
  }
}

template <typename Scalar, int N>
static void BM_PartialPivLU_Solve(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a += Matrix<Scalar, N, N>::Identity() * Scalar(N);
  Matrix<Scalar, N, 1> b = Matrix<Scalar, N, 1>::Random();
  PartialPivLU<Matrix<Scalar, N, N>> lu(a);
  Matrix<Scalar, N, 1> x;
  for (auto _ : state) {
    x = lu.solve(b);
    benchmark::DoNotOptimize(x.data());
  }
}

// ============================================================================
// ColPivHouseholderQR — for least-squares (camera calibration, etc.)
// ============================================================================

template <typename Scalar, int Rows, int Cols>
static void BM_ColPivQR_Compute(benchmark::State& state) {
  Matrix<Scalar, Rows, Cols> a;
  a.setRandom();
  ColPivHouseholderQR<Matrix<Scalar, Rows, Cols>> qr;
  for (auto _ : state) {
    qr.compute(a);
    benchmark::DoNotOptimize(qr.matrixR().data());
  }
}

// ============================================================================
// JacobiSVD — the workhorse for small matrices in CV
// ============================================================================

template <typename Scalar, int Rows, int Cols, int Options = ComputeThinU | ComputeThinV>
static void BM_JacobiSVD_Compute(benchmark::State& state) {
  Matrix<Scalar, Rows, Cols> a;
  a.setRandom();
  JacobiSVD<Matrix<Scalar, Rows, Cols>, Options> svd;
  for (auto _ : state) {
    svd.compute(a);
    benchmark::DoNotOptimize(svd.singularValues().data());
  }
}

template <typename Scalar, int Rows, int Cols>
static void BM_JacobiSVD_Solve(benchmark::State& state) {
  Matrix<Scalar, Rows, Cols> a;
  a.setRandom();
  Matrix<Scalar, Rows, 1> b = Matrix<Scalar, Rows, 1>::Random();
  JacobiSVD<Matrix<Scalar, Rows, Cols>, ComputeThinU | ComputeThinV> svd(a);
  Matrix<Scalar, Cols, 1> x;
  for (auto _ : state) {
    x = svd.solve(b);
    benchmark::DoNotOptimize(x.data());
  }
}

// ============================================================================
// SelfAdjointEigenSolver — PCA, normal estimation
// ============================================================================

template <typename Scalar, int N>
static void BM_SelfAdjointEig_Compute(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a = a.transpose() * a;
  SelfAdjointEigenSolver<Matrix<Scalar, N, N>> eig;
  for (auto _ : state) {
    eig.compute(a);
    benchmark::DoNotOptimize(eig.eigenvalues().data());
  }
}

// SelfAdjointEigenSolver::computeDirect — closed-form for 2x2 and 3x3
template <typename Scalar, int N>
static void BM_SelfAdjointEig_ComputeDirect(benchmark::State& state) {
  Matrix<Scalar, N, N> a;
  a.setRandom();
  a = a.transpose() * a;
  SelfAdjointEigenSolver<Matrix<Scalar, N, N>> eig;
  for (auto _ : state) {
    eig.computeDirect(a);
    benchmark::DoNotOptimize(eig.eigenvalues().data());
  }
}

// ============================================================================
// Registration — focus on robotics/CV sizes
// ============================================================================

// Matrix multiply: 2x2, 3x3, 4x4, 6x6
BENCHMARK(BM_MatMul<float, 2>);
BENCHMARK(BM_MatMul<float, 3>);
BENCHMARK(BM_MatMul<float, 4>);
BENCHMARK(BM_MatMul<float, 6>);
BENCHMARK(BM_MatMul<double, 2>);
BENCHMARK(BM_MatMul<double, 3>);
BENCHMARK(BM_MatMul<double, 4>);
BENCHMARK(BM_MatMul<double, 6>);

// Matrix-vector multiply
BENCHMARK(BM_MatVec<float, 3>);
BENCHMARK(BM_MatVec<float, 4>);
BENCHMARK(BM_MatVec<float, 6>);
BENCHMARK(BM_MatVec<double, 3>);
BENCHMARK(BM_MatVec<double, 4>);
BENCHMARK(BM_MatVec<double, 6>);

// Inverse
BENCHMARK(BM_Inverse<float, 2>);
BENCHMARK(BM_Inverse<float, 3>);
BENCHMARK(BM_Inverse<float, 4>);
BENCHMARK(BM_Inverse<double, 2>);
BENCHMARK(BM_Inverse<double, 3>);
BENCHMARK(BM_Inverse<double, 4>);

// Determinant
BENCHMARK(BM_Determinant<float, 2>);
BENCHMARK(BM_Determinant<float, 3>);
BENCHMARK(BM_Determinant<float, 4>);
BENCHMARK(BM_Determinant<double, 2>);
BENCHMARK(BM_Determinant<double, 3>);
BENCHMARK(BM_Determinant<double, 4>);

// LLT (Cholesky)
BENCHMARK(BM_LLT_Compute<float, 3>);
BENCHMARK(BM_LLT_Compute<float, 4>);
BENCHMARK(BM_LLT_Compute<float, 6>);
BENCHMARK(BM_LLT_Compute<double, 3>);
BENCHMARK(BM_LLT_Compute<double, 4>);
BENCHMARK(BM_LLT_Compute<double, 6>);
BENCHMARK(BM_LLT_Solve<double, 3>);
BENCHMARK(BM_LLT_Solve<double, 6>);

// LDLT
BENCHMARK(BM_LDLT_Compute<double, 3>);
BENCHMARK(BM_LDLT_Compute<double, 6>);

// PartialPivLU
BENCHMARK(BM_PartialPivLU_Compute<float, 3>);
BENCHMARK(BM_PartialPivLU_Compute<float, 4>);
BENCHMARK(BM_PartialPivLU_Compute<double, 3>);
BENCHMARK(BM_PartialPivLU_Compute<double, 4>);
BENCHMARK(BM_PartialPivLU_Solve<double, 3>);
BENCHMARK(BM_PartialPivLU_Solve<double, 4>);

// ColPivHouseholderQR
BENCHMARK(BM_ColPivQR_Compute<float, 3, 3>);
BENCHMARK(BM_ColPivQR_Compute<double, 3, 3>);
BENCHMARK(BM_ColPivQR_Compute<double, 6, 6>);
BENCHMARK(BM_ColPivQR_Compute<double, 8, 3>);  // overdetermined least-squares

// JacobiSVD — the key CV sizes
BENCHMARK(BM_JacobiSVD_Compute<float, 2, 2>);
BENCHMARK(BM_JacobiSVD_Compute<float, 3, 3>);
BENCHMARK(BM_JacobiSVD_Compute<float, 4, 4>);
BENCHMARK(BM_JacobiSVD_Compute<double, 2, 2>);
BENCHMARK(BM_JacobiSVD_Compute<double, 3, 3>);
BENCHMARK(BM_JacobiSVD_Compute<double, 4, 4>);
BENCHMARK(BM_JacobiSVD_Compute<double, 3, 4>);  // projection matrix
BENCHMARK(BM_JacobiSVD_Compute<double, 6, 6>);  // manipulator Jacobian
BENCHMARK(BM_JacobiSVD_Compute<double, 8, 9>);  // fundamental matrix (8-point)
BENCHMARK(BM_JacobiSVD_Solve<double, 3, 3>);
BENCHMARK(BM_JacobiSVD_Solve<double, 6, 6>);

// Values-only SVD (when you just need singular values)
BENCHMARK((BM_JacobiSVD_Compute<double, 3, 3, 0>));
BENCHMARK((BM_JacobiSVD_Compute<double, 6, 6, 0>));

// SelfAdjointEigenSolver — PCA, normal estimation
BENCHMARK(BM_SelfAdjointEig_Compute<float, 3>);
BENCHMARK(BM_SelfAdjointEig_Compute<float, 4>);
BENCHMARK(BM_SelfAdjointEig_Compute<double, 3>);
BENCHMARK(BM_SelfAdjointEig_Compute<double, 4>);
BENCHMARK(BM_SelfAdjointEig_Compute<double, 6>);

// SelfAdjointEigenSolver::computeDirect (closed-form, 2x2 and 3x3 only)
BENCHMARK(BM_SelfAdjointEig_ComputeDirect<float, 2>);
BENCHMARK(BM_SelfAdjointEig_ComputeDirect<float, 3>);
BENCHMARK(BM_SelfAdjointEig_ComputeDirect<double, 2>);
BENCHMARK(BM_SelfAdjointEig_ComputeDirect<double, 3>);
