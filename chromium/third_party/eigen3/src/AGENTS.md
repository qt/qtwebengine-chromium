# AGENTS.md

Guidance for AI coding agents (Claude Code, Cursor, Copilot Workspace, Aider, OpenAI Codex, etc.) working in this repository. Human contributors should start from [`README.md`](README.md) and the project sites it links to (<https://eigen.tuxfamily.org>, <https://libeigen.gitlab.io>, and the upstream repo at <https://gitlab.com/libeigen/eigen>); this file is the agent-facing condensation.

If your tool also reads its own per-tool config file, treat that as a thin pointer and rely on this file for substance.

## Agent guidelines (read first)

These are the cross-cutting rules that catch agents most often:

1. **Provenance and attribution — no plagiarism, no license laundering.** Eigen code must be original or derived from publicly published MPL-2.0-compatible material. Do **not** copy — verbatim, paraphrased, or "translated" to another syntax — from incompatibly-licensed sources (proprietary, NDA-encumbered, prior-employer internal); paraphrasing is still a derivative work. **Cite published references inline** when they inform an implementation — LAPACK / LAWN, ACM TOMS / SIAM papers, Higham, Golub & van Loan, textbook algorithms, Boost components, vendor application notes — by name (author, year, identifier; a comment or Doxygen `\note` block suffices). Same discipline for AI-suggested code: cite the source, or rewrite from a known reference and cite that, or drop it. Ideas aren't copyrightable, specific expressions are — learn from the source, write your own, credit it.
2. **Header-only contract.** Eigen ships as headers only. **Never** include anything under `Eigen/src/...` or `unsupported/Eigen/src/...` directly — `InternalHeaderCheck.h` makes that a hard compile error. User code reaches implementation only through the umbrella module headers (`Eigen/Core`, `Eigen/Dense`, `Eigen/SVD`, …). When moving or renaming files inside any `src/` subtree, delete the old file outright; only the public umbrella headers ever get back-compat forwarding shims.
3. **Preserve `EIGEN_DEVICE_FUNC`.** It is pervasive on coefficient-level methods so the same code compiles for host and CUDA / HIP / SYCL device. Dropping it silently breaks GPU builds and is rarely caught locally.
4. **Don't apply general C++ "modernize" advice.** Do not propose `modernize-*` or `cppcoreguidelines-*` clang-tidy fixes, replace `EIGEN_STRONG_INLINE` with `inline`, "fix" Eigen's macro indentation, or reorder includes. Eigen has its own conventions encoded in `.clang-format` (`SortIncludes: false`, custom `StatementMacros` / `AttributeMacros`); CI will diff against them.
5. **Format before you commit.** Style is `clang-format-17` exactly (Google base, 120 cols). Other versions diff against CI. Run `scripts/format.sh` (whole tree) or `clang-format-17 -i <file>` (one file) before pushing. Format failures are the single most common reason an MR is red.
6. **Tests are not gtest** (today). They use Eigen's own framework (`test/main.h`): `VERIFY_*` assertions, `CALL_SUBTEST_N` / `EIGEN_TEST_PART_N` for splitting, `EIGEN_DECLARE_TEST(<name>) { ... }` as the entry point. See "Adding a test" below. **In-flight migration to Google Test:** [MR 2159](https://gitlab.com/libeigen/eigen/-/merge_requests/2159) (draft) replaces `EIGEN_DECLARE_TEST` / `CALL_SUBTEST_N` with gtest `TEST` / `TYPED_TEST`, brings in gtest 1.15.2 via `FetchContent`, and bridges `VERIFY` / `VERIFY_IS_APPROX` to gtest expectations. When that lands, this rule flips — write new tests as gtest fixtures and use the bridged `VERIFY_*` macros.
7. **Tests are not in the default `all` target.** Bare `ninja` builds nothing relevant. Use the named targets (`buildtests`, `check`, `BuildOfficial`, `BuildUnsupported`, `buildsmoketests`, `buildtests_gpu`, `check_gpu`).
8. **`auto` traps.** `auto x = A + B;` captures a lazy expression holding references that can dangle. Use `.eval()` or an explicit type. Likewise be careful with `.noalias()` — it must only be used when the destination doesn't appear on the right side.
9. **Stage commits explicitly.** Don't `git add -A` / `git add .` inside an Eigen working copy. Repo roots commonly accumulate untracked dotfiles and tool config (`.vscode/`, `.idea/`, `.claude/`, etc.) that must not enter commits. Add files by path or with targeted globs (e.g. `git add 'Eigen/src/Cholesky/'`).
10. **Pause before pushing or filing an MR.** External-system writes (push, MR creation, MR comments) have non-trivial blast radius. After a local commit, summarize what changed and wait for the human to say "push" or "file the MR" before doing so. The same applies to scope decisions like bundling/splitting commits.
11. **Tests and benchmarks ship with the code.** New functionality lands with its tests; performance-sensitive changes land with a benchmark. Don't defer either to a follow-up MR.
12. **Benchmarking discipline.** Benchmarks on a loaded system are useless. Before running any benchmark: check `uptime` shows a low load average, finish/cancel background builds, and **never** run two benchmark binaries in the same shell invocation (parallel or chained with `&&`) — run each in its own shell, take medians within a binary, and optionally alternate (A, B, A, B) across separate invocations to detect drift.
13. **Tensor module is foundational to TensorFlow.** `unsupported/Eigen/Tensor` and the work-stealing thread pool in `Eigen/ThreadPool` together form TensorFlow's core compute backend. "Unsupported" here means "looser API-stability guarantees" — it does **not** mean low-traffic or low-stakes. Breaking changes (signatures, header layout, semantics, or performance regressions on contraction / reduction / morphing kernels) ripple into every TensorFlow build and from there into every project that pulls TensorFlow as a dependency. Treat Tensor and ThreadPool as load-bearing: prefer additive changes, keep header paths stable, run downstream Tensor tests (`unsupported/test/tensor_*`), and call out any behavior change prominently in the MR description.

## Repository overview

Eigen is a **header-only** C++ template library for linear algebra: dense and sparse matrices, vectors, decompositions, geometry, iterative solvers. The library itself does not need to be built — consumers just `#include <Eigen/Dense>` (or other module headers). CMake is required only for tests, BLAS/LAPACK shims, demos, and docs.

Upstream is GitLab: <https://gitlab.com/libeigen/eigen>. Bug reports, feature requests, and merge requests go there — the GitHub mirror is read-only. Minimum standard is **C++14** (`target_compile_features(eigen INTERFACE cxx_std_14)` in `CMakeLists.txt`); SYCL builds force C++17. The project aspires to bump the baseline to C++17 in a future release. The pace on baseline bumps is deliberately slow so that new Eigen improvements remain available to users on embedded platforms, whose toolchains are often several years behind mainline. License: MPL2 for the bulk, with a few files under other compatible licenses (`COPYING.*` at the root).

## Quality bar

Eigen aspires to state-of-the-art results on two axes — **performance** and **numerical accuracy / IEEE-754 conformance** — and treats them as separate goals that are sometimes in tension.

**Performance.** Eigen Core emphasizes single-core throughput via two levers: SIMD through per-architecture packet backends (see "SIMD / packet math layer"), and memory-hierarchy use through blocked algorithms — cache-aware blocking and panel/kernel decompositions in `Eigen/src/Core/products/`, tile-based traversal in the Tensor module. **Eigen Core is mostly optimized for single-core throughput**; multi-core in Core is opt-in (OpenMP or `EIGEN_GEMM_THREADPOOL`) and covers a subset of operations. The **Tensor module** is the opposite — designed for multi-core via `ThreadPoolDevice`, which dispatches across worker threads of the work-stealing thread pool. See "Multi-threading" below.

**Numerical accuracy.** The bar is LAPACK-level for linear algebra (decompositions, solvers — backward stability, pivoting, conditioning) and C++ standard-library level for standard math functions (`exp`, `log`, `sin`, `pow`, …) on scalars. For **special values** — IEEE-754 entities (NaN, ±0, ±∞, subnormals) and function-specific edge cases (singularities, branch boundaries; e.g. `log(0)`, `pow(0, 0)`) — the bar is exact conformance to IEEE 754 / ISO C / C++ specifications, with the cppreference page for each function as the authoritative spec (e.g. [`std::pow`](https://en.cppreference.com/cpp/numeric/math/pow)). On regular inputs **a few ULPs** of error in vectorized math in exchange for SIMD throughput is the long-standing trade-off; larger deviations need explicit justification. **Special-value handling is not subject to the few-ULPs trade-off** — it must match spec exactly.

When adding or modifying a numerical kernel:

- Test **numerical corner cases**, not just typical inputs: ±0, ±∞, NaN, subnormals, values at and around the function's domain boundaries (e.g. `log(0)`, `log(-x)`, `pow(0, 0)`), values near overflow / underflow, denormalized results, signed-zero preservation, and ULP behavior near hard cases (e.g. argument-reduction breakdown for `sin`/`cos` at large arguments).
- **Matrix coverage matters as much as scalar coverage.** Decomposition, solver, and matrix-function tests should exercise ill-conditioned inputs across a range of condition numbers (well-conditioned through near-singular through singular) and matrices with structure relevant to the algorithm: Hilbert, Vandermonde, Pascal, Wilkinson's W, Frank, Lehmer, KMS / Toeplitz, banded, rank-deficient, defective and near-defective (Jordan blocks), positive-definite-but-barely, etc. Standard references: **Higham's *Accuracy and Stability of Numerical Algorithms* and *Functions of Matrices*** (and MATLAB `gallery()`, largely drawn from those books) and **Golub & van Loan, *Matrix Computations***. Where the algorithm has a LAPACK counterpart, match its `TESTING/` category coverage as the bar.
- Verify behavior across all enabled packet backends — `test/packetmath.cpp` and `unsupported/test/special_packetmath.cpp` are the canonical entry points; a missing or divergent backend specialization usually shows up there first.
- Quantify accuracy regressions in ULPs against the scalar reference, not just relative error. Sollya / MPFR are the standard tools for ground-truth and polynomial generation; do the verification in C++ with MPFR rather than Python.
- Performance-sensitive changes ship with a benchmark (under `benchmarks/` or `unsupported/benchmarks/`). See "Benchmarking discipline" in the agent guidelines above.

For decompositions and solvers: the bar is matching LAPACK on conditioning, pivoting strategy, and backward stability. Don't trade numerical robustness for speed in those code paths without explicit sign-off.

## Build / test

> **In-flight test-framework migration:** [MR 2159](https://gitlab.com/libeigen/eigen/-/merge_requests/2159) migrates the test framework from Eigen's custom `EIGEN_DECLARE_TEST` / `CALL_SUBTEST_N` macros to Google Test. The "Adding a test" and "Test split" subsections below describe the **current** framework on master. Once 2159 lands, tests become gtest `TEST` / `TYPED_TEST` fixtures, per-`N` executable splitting goes away, and `VERIFY_*` survives as a bridge to gtest expectations.

Tests are intentionally **not** in the default `all` target — `ninja` (or `make`) on its own builds nothing relevant. Drive everything through the named targets:

```bash
mkdir -p build && cd build
cmake -G Ninja ..                        # plain config

ninja buildtests                         # build all unit tests
ninja buildtests_gpu                     # GPU-only tests
ninja BuildOfficial                      # only test/      (subproject "Official")
ninja BuildUnsupported                   # only unsupported/test/ (subproject "Unsupported")
ninja buildsmoketests                    # the MR smoke set
ninja check                              # = buildtests + ctest
ninja check_gpu                          # = buildtests_gpu + ctest -L gpu

# generated wrappers (after `cmake` configure; run from inside the build dir):
./buildtests.sh <regex>                  # build tests whose name matches
./check.sh <regex>                       # build + run tests whose name matches
```

### Common CMake knobs

ISA / vectorization (turn on per-ISA test compile flags — see `CMakeLists.txt` and `cmake/EigenTesting.cmake` for the authoritative list):
- x86: `EIGEN_TEST_SSE2`, `EIGEN_TEST_SSE3`, `EIGEN_TEST_SSSE3`, `EIGEN_TEST_SSE4_1`, `EIGEN_TEST_SSE4_2`, `EIGEN_TEST_AVX`, `EIGEN_TEST_AVX2`, `EIGEN_TEST_AVX512`, `EIGEN_TEST_AVX512DQ`, `EIGEN_TEST_AVX512FP16`, `EIGEN_TEST_FMA`, `EIGEN_TEST_F16C`, `EIGEN_TEST_X87`, `EIGEN_TEST_32BIT`
- ARM: `EIGEN_TEST_NEON`, `EIGEN_TEST_NEON64`
- PowerPC: `EIGEN_TEST_VSX`, `EIGEN_TEST_ALTIVEC`
- IBM Z (s390x): `EIGEN_TEST_Z13`, `EIGEN_TEST_Z14`
- LoongArch: `EIGEN_TEST_LSX`
- MIPS: `EIGEN_TEST_MSA`
- GPU / SYCL: `EIGEN_TEST_CUDA`, `EIGEN_TEST_CUDA_CLANG`, `EIGEN_TEST_CUDA_NVC` (NVHPC), `EIGEN_TEST_HIP`, `EIGEN_TEST_SYCL`
- Negative / behavioral: `EIGEN_TEST_NO_EXPLICIT_VECTORIZATION`, `EIGEN_TEST_NO_EXPLICIT_ALIGNMENT`, `EIGEN_TEST_NO_EXCEPTIONS`

Test-wide knobs:
- `EIGEN_TEST_MAX_SIZE=320` (default) — clamp the random matrix sizes used by tests.
- `EIGEN_SPLIT_LARGE_TESTS=ON` (default) — splits any test using `CALL_SUBTEST_N` / `EIGEN_TEST_PART_N` into per-`N` executables (`foo_1`, `foo_2`, …); see "Test split" below.
- `EIGEN_DEFAULT_TO_ROW_MAJOR=ON` — re-runs the suite with row-major default storage.
- `EIGEN_LEAVE_TEST_IN_ALL_TARGET=ON` — adds tests back to `all` (used by some CI harnesses driving ctest's automatic build path).
- `EIGEN_TEST_CUSTOM_CXX_FLAGS=…`, `EIGEN_TEST_CUSTOM_LINKER_FLAGS=…` — extra flags applied **only** to test targets (handy for working around codegen bugs).
- `EIGEN_TEST_OPENMP=ON` — link tests against OpenMP.
- `EIGEN_TEST_EXTERNAL_BLAS=ON`, `EIGEN_TEST_EXTERNAL_LAPACK=ON` — exercise the `EIGEN_USE_BLAS` / `EIGEN_USE_LAPACKE` paths against an external implementation; without these, the in-tree `eigen_blas`/`eigen_lapack` from `blas/` and `lapack/` are used.

Auxiliary trees:
- `EIGEN_BUILD_BLAS=ON` / `EIGEN_BUILD_LAPACK=ON` (default ON only for top-level builds) — build the Eigen-backed BLAS/LAPACK shim libraries under `blas/` and `lapack/`.
- `EIGEN_BUILD_DOC=ON` — Doxygen documentation (`ninja doc`).
- `EIGEN_BUILD_DEMOS=ON` — the small demos under `demos/`.

### Running tests

```bash
ctest --parallel --output-on-failure          # everything that's been built
ctest -L Official                              # only tests under test/
ctest -L Unsupported                           # only tests under unsupported/test/
ctest -L gpu                                   # GPU-tagged tests
ctest -L smoketest                             # the MR smoke set
ctest -R '^cholesky'                           # regex over test names
ctest -R '^bdcsvd_3$' --output-on-failure -V   # one specific split
```

Subproject labels come from `set_property(GLOBAL PROPERTY EIGEN_CURRENT_SUBPROJECT "Official"|"Unsupported")` in `test/CMakeLists.txt` and `unsupported/test/CMakeLists.txt`. Build-group targets (`BuildOfficial`, `BuildUnsupported`) are kept in sync with the ctest labels.

A test binary can be invoked directly to control seed / repeat:

```bash
./test/foo_3 r5 s1234            # repeat 5 times, fixed seed 1234
EIGEN_REPEAT=10 EIGEN_SEED=1 ./test/foo_3
```

### Test split (important)

A test source file containing `CALL_SUBTEST_N(...)` or `EIGEN_TEST_PART_N` macros is compiled into **N separate executables** named `<testname>_1`, `<testname>_2`, …, each built with `-DEIGEN_TEST_PART_<N>=1` (logic in `cmake/EigenTesting.cmake` — `ei_add_test`). The umbrella `<testname>` target builds all parts. `ctest -R ^<testname>$` does **not** match individual parts; use `ctest -R <testname>` for the regex form. Set `EIGEN_SPLIT_LARGE_TESTS=OFF` to fold them into a single binary if you need to debug across parts.

### Adding a test

1. Create `test/<name>.cpp` (or `unsupported/test/<name>.cpp`). Include `main.h`. Use the `VERIFY*` macros (listed below). For multi-part tests, structure the body as `CALL_SUBTEST_1(...) ... CALL_SUBTEST_N(...)`.
2. End the file with the entry point: `EIGEN_DECLARE_TEST(<name>) { ... }` — that macro expands to the per-binary `main()` (random seed / repeat handling, signal handlers, etc.). It is **not** gtest.
3. Register with `ei_add_test(<name>)` in the matching `CMakeLists.txt` near similar tests. The second/third args are extra compile flags / libraries (e.g. `ei_add_test(packetmath "-DEIGEN_FAST_MATH=1")`).
4. Re-run CMake configure; the new target is then in `buildtests` and ctest.

Test assertion macros (defined in `test/main.h`):
- `VERIFY(cond)` — assert condition
- `VERIFY_IS_APPROX(a, b)`, `VERIFY_IS_NOT_APPROX(a, b)` — approximate floating-point equality
- `VERIFY_IS_EQUAL(a, b)`, `VERIFY_IS_NOT_EQUAL(a, b)` — exact equality
- `VERIFY_IS_MUCH_SMALLER_THAN(a, b)`
- `VERIFY_RAISES_ASSERT(expr)` — assert that `eigen_assert` fires

`failtest/` holds compile-failure tests: each has an `_ok` and `_ko` target — `_ok` must compile, `_ko` must fail to compile (driven via `-DEIGEN_SHOULD_FAIL_TO_BUILD`).

## Formatting and lint

Style is `clang-format-17` (Google base, 120 cols, see `.clang-format`). The version is hard-coded — newer or older clang-format will diff against CI.

```bash
scripts/format.sh                                 # reformat the whole tree
clang-format-17 -i <file>                         # reformat one file
clang-format-17 --dry-run --Werror <file>         # check (CI's `checkformat:clangformat`)
git clang-format --diff --commit <base-sha>       # diff what CI will diff
codespell --config setup.cfg                      # spell-check (also a CI job)
```

`.clang-format` registers Eigen-specific macros (`EIGEN_STATIC_ASSERT`, `EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED`, `EIGEN_INTERNAL_DENSE_STORAGE_CTOR_PLUGIN`, etc.) as `StatementMacros`, and `EIGEN_STRONG_INLINE` / `EIGEN_ALWAYS_INLINE` / `EIGEN_DEVICE_FUNC` / `EIGEN_DONT_INLINE` / `EIGEN_DEPRECATED` / `EIGEN_UNUSED` as `AttributeMacros`. Don't "fix" their indentation or strip them. `SortIncludes: false` — include order is meaningful in this codebase.

`clang-tidy` runs in the `checkformat:clangtidy` MR job. Locally: `cmake -G Ninja -S . -B .tidy-build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && clang-tidy -p .tidy-build <files>`. Eigen has its own conventions — do not apply `modernize-*` or `cppcoreguidelines-*` checks.

**SPDX / REUSE.** Every new source file (`.h`, `.cpp`, `.cu`, `.inc`, `.cmake`, `CMakeLists.txt`, etc.) must carry inline copyright + license headers; the `checkformat:reuse` CI job (`reuse lint`) blocks otherwise. The standard Eigen header is:

```cpp
// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) <year> <Your Name> <your.email@example.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
```

Top-level docs (`*.md`), generated files (`*.in`), and binary assets that can't carry inline headers are covered by path annotations in `REUSE.toml` — add the path if you're creating one.

## Module layout

> **In-flight rename:** [MR 2522](https://gitlab.com/libeigen/eigen/-/merge_requests/2522) renames the top-level `unsupported/` directory to `contrib/`. Paths below track master (still `unsupported/`). When 2522 lands, substitute `contrib/` for `unsupported/` mechanically — it doesn't affect the canonical/forwarding-shim distinction on the `unsupported/Eigen/` bullet.

- `Eigen/` — supported public headers. Each filename without an extension (`Eigen/Core`, `Eigen/Dense`, `Eigen/SVD`, …) is the umbrella include for one module; implementation lives in `Eigen/src/<Module>/`. **Never include anything under `Eigen/src/...` directly** — that is a hard error (each implementation header includes `InternalHeaderCheck.h` to enforce it).
- `Eigen/src/Core/arch/{SSE,AVX,AVX512,NEON,SVE,AltiVec,GPU,HIP,SYCL,LSX,RVV10,HVX,MSA,ZVector,clang,Default}/` — per-architecture packet-math (vectorization) backends. `GenericPacketMath.h` defines the `internal::p*` API each backend specializes. (PowerPC VSX support lives inside `AltiVec/`, no separate `VSX/` directory.)
- `Eigen/src/Core/products/` — gemm/gemv kernels (`GeneralBlockPanelKernel.h`, triangular / self-adjoint variants, BLAS bridges in `*_BLAS.h`).
- `Eigen/src/Core/util/` — meta-programming, macros, memory, `ForwardDeclarations.h`. `Macros.h` and `ConfigureVectorization.h` set the compile-time feature flags.
- `Eigen/ThreadPool` (with `Eigen/src/ThreadPool/`) — work-stealing thread pool (`NonBlockingThreadPool`, `RunQueue`, `EventCount`, `ForkJoin`, `CoreThreadPoolDevice`). Originally developed for TensorFlow; now part of Core. It is the backend behind `EIGEN_GEMM_THREADPOOL` and the `ThreadPoolDevice` used by the Tensor module.
- `unsupported/Eigen/` — modules with looser API-stability guarantees, but **not** low-traffic. The headliner is `Tensor` (umbrella `unsupported/Eigen/Tensor`, sources under `unsupported/Eigen/src/Tensor/`) — TensorFlow's core compute backend; treat as load-bearing (see agent guideline 13). Other modules: `TensorSymmetry`, `AutoDiff`, `Polynomials`, `MatrixFunctions`, `NNLS`, `FFT`, `GPU` (cuBLAS / cuSOLVER dispatch), `Splines`, `NumericalDiff`, etc. Tests go under `unsupported/test/`. Anything under `unsupported/Eigen/CXX11/` is **backward-compatibility forwarding shims only** — don't include those paths in new code or add new headers there.
- `test/` — main test sources and `test/main.h` (the test framework: `VERIFY_*`, `CALL_SUBTEST_N`, `EIGEN_TEST_PART_N`, `EIGEN_DECLARE_TEST`).
- `failtest/` — compile-failure tests.
- `blas/`, `lapack/` — Eigen's BLAS/LAPACK shim libraries (`eigen_blas`, `eigen_lapack`), built only when `EIGEN_BUILD_BLAS` / `EIGEN_BUILD_LAPACK` are on. These get linked by sparse-solver tests (CHOLMOD, UMFPACK, KLU, SuperLU, …) when those packages are present.
- `cmake/` — `EigenTesting.cmake`, `EigenConfigureTesting.cmake` define the `ei_add_test` / `ei_add_failtest` machinery. Also the `Find*.cmake` modules for optional backends.
- `ci/` — GitLab CI (`*.gitlab-ci.yml` per stage) and shell drivers under `ci/scripts/`.

| Module | Header | Contents |
|---|---|---|
| Core | `Eigen/Core` | Matrix, Array, basic linear algebra, Map, Block, Ref |
| LU | `Eigen/LU` | FullPivLU, PartialPivLU, inverse, determinant |
| Cholesky | `Eigen/Cholesky` | LLT, LDLT |
| QR | `Eigen/QR` | HouseholderQR, ColPivHouseholderQR, FullPivHouseholderQR |
| SVD | `Eigen/SVD` | JacobiSVD, BDCSVD |
| Eigenvalues | `Eigen/Eigenvalues` | SelfAdjointEigenSolver, EigenSolver, ComplexEigenSolver |
| Geometry | `Eigen/Geometry` | Quaternion, AngleAxis, Transform, Hyperplane, `cross()` |
| Sparse | `Eigen/Sparse` | SparseMatrix, sparse solvers (SparseLU, SparseQR, SimplicialCholesky) |
| IterativeLinearSolvers | `Eigen/IterativeLinearSolvers` | ConjugateGradient, BiCGSTAB, LeastSquaresConjugateGradient (additional iterative solvers — GMRES, DGMRES, IDRS, BiCGSTABL — live in `unsupported/Eigen/IterativeSolvers`) |

External backend umbrella headers (`Eigen/<Pkg>Support`): `AccelerateSupport`, `CholmodSupport`, `KLUSupport`, `MetisSupport`, `PaStiXSupport`, `PardisoSupport`, `SPQRSupport`, `SuperLUSupport`, `UmfPackSupport`. Intel MKL and AMD AOCL are *not* umbrella headers — activate them by defining `EIGEN_USE_MKL_ALL` / `EIGEN_USE_AOCL_ALL` (and friends like `EIGEN_USE_BLAS`, `EIGEN_USE_LAPACKE`, `EIGEN_USE_AOCL_VML`, `EIGEN_USE_AOCL_BLAS`) before including Core or Dense; glue in `Eigen/src/Core/util/MKL_support.h` and `AOCL_Support.h`. Convenience headers: `Eigen/Dense` = Core + all dense solvers; `Eigen/Eigen` = everything supported.

## Architecture

### Expression templates and lazy evaluation

Eigen's central design pattern is **expression templates with lazy evaluation**. Arithmetic operations do not compute immediately — they return lightweight expression objects that store references to operands and encode the operation. Evaluation happens only on assignment:

```cpp
// v + w returns CwiseBinaryOp<scalar_sum_op, VectorXf, VectorXf>
// No computation until assigned — then fused into a single vectorized loop
VectorXf u = v + w;
```

This eliminates temporaries and lets the compiler fuse operations into single-pass, SIMD-vectorized loops. Key expression types:
- `CwiseUnaryOp`, `CwiseBinaryOp`, `CwiseTernaryOp` — element-wise operations
- `Product` — matrix product (special eager rules, see below)
- `CwiseNullaryOp` — procedural matrices (Zero, Identity, Random, custom functors)
- `Block`, `Transpose`, `Diagonal`, `Reshaped`, `IndexedView`, `Map`, `Ref` — view-style expressions
- `Inverse`, `Solve` — solver-result expressions

**When Eigen evaluates eagerly into temporaries:**
1. **Matrix products on assignment** — `mat = mat * mat` auto-creates a temporary to prevent aliasing. Use `.noalias()` to suppress when safe: `m1.noalias() += m2 * m3;`
2. **Nested products** — in `mat1 = mat2 * mat3 + mat4 * mat5`, each product evaluates to a temporary before combining.
3. **Cost-based** — sub-expressions are cached when recomputation would be more expensive than storage (e.g. `mat1 = mat2 * (mat3 + mat4)` evaluates the sum once).

Use `.eval()` to force evaluation; `.noalias()` to override automatic temporary creation.

### Evaluator system

The expression-template engine is implemented through `evaluator<>` traits (in `Eigen/src/Core/CoreEvaluators.h` and `ProductEvaluators.h`). Assignment goes through `Eigen/src/Core/AssignEvaluator.h` and `Assign.h`. New expression types must specialize `evaluator<>` (and often `assign_op` / `nested_eval`). Operations are lazy by default — work happens at assignment time inside `AssignEvaluator`, which picks between scalar / vectorized / linear / inner / outer traversal strategies based on `Flags`.

### Class hierarchy (CRTP)

Eigen avoids virtual functions. Polymorphism is compile-time via CRTP — each class inherits from a base templated on itself (e.g. `Matrix` inherits `MatrixBase<Matrix>`).

```
EigenBase                          — root for anything evaluable to a matrix
├── DenseCoeffsBase                — coefficient accessors
│   └── DenseBase                  — shared dense ops (block, reshape, visitors)
│       ├── MatrixBase             — linear algebra ops (all dense matrix/vector expressions)
│       │   └── PlainObjectBase    — manages storage and resizing
│       │       ├── Matrix         — concrete dense matrix (linear algebra semantics)
│       │       └── Array          — concrete dense array (coefficient-wise semantics)
│       └── ArrayBase              — coefficient-wise ops (all array expressions)
└── SparseMatrixBase               — sparse matrix expressions
    └── SparseCompressedBase       — compressed sparse storage (CSC/CSR)
        └── SparseMatrix, SparseVector
```

Every expression type (Block, Transpose, Map, CwiseBinaryOp, Product, …) inherits from `MatrixBase<Derived>` or `ArrayBase<Derived>` without owning storage. `PlainObjectBase` → `DenseStorage` manages actual memory for `Matrix` and `Array`.

**Matrix vs Array**: `Matrix` types live in MatrixBase-world (linear algebra semantics: `*` is matrix multiply). `Array` types live in ArrayBase-world (coefficient-wise semantics: `*` is element-wise multiply). Convert with `.array()` and `.matrix()`.

### Writing functions that accept Eigen types

Because each expression has a unique type, functions should accept base-class references to avoid forcing evaluation:

```cpp
// Good: accepts any dense matrix expression, no temporaries
template <typename Derived>
void foo(const Eigen::MatrixBase<Derived>& x);

// Also good: non-templated, uses Ref to avoid copies when layouts match
void bar(const Eigen::Ref<const Eigen::MatrixXf>& x);

// Hierarchy of genericity:
// EigenBase > DenseBase > MatrixBase/ArrayBase > concrete types
```

For writable parameters, take `const MatrixBase<Derived>&` and `const_cast` internally — that is the standard Eigen pattern. The reason is that callers commonly want to pass *expression* arguments like `m.row(i)` or `m.block(...)` as out-params; those are temporaries whose const-ness is a language artifact, not a semantic restriction. The const-ref-plus-`const_cast` idiom lets you accept them without forcing the user to materialize a named lvalue.

### SIMD / packet math layer (`src/Core/arch/`)

Vectorization is abstracted through a "packet" layer. Each scalar type maps to a platform-specific SIMD vector type via `internal::packet_traits<Scalar>::type`. Architecture backends provide specializations of packet operations (`padd`, `pmul`, `pload`, `pstore`, `pblend`, …):

- `GenericPacketMath.h` — generic scalar fallback API
- `arch/Default/` — shared helpers (`GenericPacketMathFunctions.h`, `BFloat16.h`, `Half.h`)
- x86: `arch/SSE/`, `arch/AVX/`, `arch/AVX512/`
- ARM: `arch/NEON/`, `arch/SVE/`
- RISC-V: `arch/RVV10/` (scalable vector, multiple LMUL)
- PowerPC: `arch/AltiVec/` (includes MMA support)
- IBM Z: `arch/ZVector/`
- Other: `arch/MSA/` (MIPS), `arch/LSX/` (Loongson), `arch/HVX/` (Qualcomm Hexagon)
- GPU: `arch/GPU/` (CUDA), `arch/HIP/`, `arch/SYCL/`
- `arch/clang/` — generic clang vector-extension backend

Packets are selected at compile time; the assignment loop splits into an aligned vectorized path plus a scalar remainder. New packet-math intrinsics get added in **every** backend that supports the type. `arch/Default/` holds generic SIMD implementations shared across backends; scalar fallbacks live in `Eigen/src/Core/GenericPacketMath.h` and `Eigen/src/Core/MathFunctions.h`. `test/packetmath.cpp` (and `unsupported/test/special_packetmath.cpp`) exercises them across all enabled backends — failures there often indicate a missing or divergent specialization.

**Guard intrinsics by ISA feature macro.** Inside a backend directory, an intrinsic is only available when its ISA is enabled — `arch/AVX/` is compiled when `EIGEN_VECTORIZE_AVX` is set, but AVX2 / FMA / AVX512* intrinsics within those files must each be guarded by their own `EIGEN_VECTORIZE_*` macro (`#ifdef EIGEN_VECTORIZE_AVX2`, `EIGEN_VECTORIZE_FMA`, `EIGEN_VECTORIZE_AVX512DQ`, etc.), with a fallback for the un-guarded path. Full list in `Eigen/src/Core/util/ConfigureVectorization.h`. Same discipline applies elsewhere (`EIGEN_VECTORIZE_NEON_FP16`, `EIGEN_VECTORIZE_VSX`, …). Missing guards typically compile fine locally and break CI on narrower ISA targets.

### CUDA / HIP / SYCL

Eigen has **two independent GPU stories**, and conflating them causes confusion:

1. **In-kernel use of Eigen types.** When Eigen headers are included from `.cu` / HIP / SYCL files, most functions are automatically annotated `__device__ __host__` via `EIGEN_DEVICE_FUNC` (unified under `EIGEN_GPUCC` for both CUDA and HIP). Only fixed-size types work in kernels. Host SIMD is disabled in `.cu` files — move expensive host-side Eigen code to `.cpp`. Define `EIGEN_NO_CUDA` or `EIGEN_NO_HIP` to suppress device annotations for the respective backend. On 64-bit systems, set `EIGEN_DEFAULT_DENSE_INDEX_TYPE` to `int` for device compatibility.
2. **Host-side dispatch to NVIDIA libraries** (`unsupported/Eigen/GPU`). Plain `.cpp` files orchestrating cuBLAS / cuSOLVER / cuFFT / cuSPARSE / cuDSS calls on device-resident `gpu::DeviceMatrix<Scalar>`. Public API in `Eigen::gpu`, internals in `Eigen::gpu::internal`; solvers (`gpu::LLT`, `gpu::LU`, `gpu::QR`, `gpu::SVD`, `gpu::SelfAdjointEigenSolver`) wrap cuSOLVER. **Not** an expression-template system — every supported expression maps to a single library call, and `DeviceMatrix` does not inherit from `MatrixBase`. Tests compile as `.cpp` (not `.cu`) so NVCC doesn't instantiate Eigen CPU packet ops for CUDA vector types. See `unsupported/Eigen/src/GPU/README.md`.

Tensor GPU kernels live under `unsupported/Eigen/src/Tensor/` (`TensorReductionGpu.h`, `TensorContractionGpu.h`, `TensorDeviceGpu.h`).

### Multi-threading

Eigen parallelizes general dense matrix-matrix products, `PartialPivLU`, row-major sparse-dense products, and some iterative solvers (CG, BiCGSTAB, LeastSquaresCG) via OpenMP or the `EIGEN_GEMM_THREADPOOL` backend (mutually exclusive with OpenMP). Enable OpenMP with `-fopenmp` (GCC) or equivalent. Control threads with `Eigen::setNbThreads(n)` or `OMP_NUM_THREADS`. Limit to physical cores — hyperthreading hurts Eigen's cache-bound kernels. `Eigen::initParallel()` is deprecated and no longer needed.

The `EIGEN_GEMM_THREADPOOL` backend is Eigen's own **work-stealing thread pool**: umbrella `Eigen/ThreadPool`, implementation under `Eigen/src/ThreadPool/`. Headline class `NonBlockingThreadPool` (work-stealing pool over `RunQueue` per-thread deques, `EventCount` for parking), with `CoreThreadPoolDevice` wiring it into Eigen's parallel-for loops and `ThreadPoolInterface` as the abstract base. Originally developed for TensorFlow and used by the Tensor module via `ThreadPoolDevice` — changes are subject to the same caution as Tensor itself.

## Key preprocessor macros

**Performance / behavior** (define before including Eigen):
- `EIGEN_DONT_VECTORIZE` — disable explicit SIMD vectorization
- `EIGEN_DONT_PARALLELIZE` — disable multi-threading
- `EIGEN_FAST_MATH` — exists, default 1 (current usage across the codebase is uneven; a future cleanup will either standardize or remove it)
- `EIGEN_NO_MALLOC` / `EIGEN_RUNTIME_NO_MALLOC` — assert on heap allocation
- `EIGEN_UNROLLING_LIMIT` — loop unrolling threshold (default: 110)
- `EIGEN_STACK_ALLOCATION_LIMIT` — max stack buffer size (default: 128 KB)
- `EIGEN_DEFAULT_TO_ROW_MAJOR` — change default storage from column-major to row-major
- `EIGEN_MAX_ALIGN_BYTES` — alignment for dynamic/static data (auto-detected: 64 for AVX-512, 32 for AVX, 16 default)
- `EIGEN_USE_BLAS` / `EIGEN_USE_LAPACKE` — delegate to external BLAS/LAPACK
- `EIGEN_USE_MKL_ALL` / `EIGEN_USE_AOCL_ALL` — delegate broadly to MKL / AOCL

**Debugging:**
- `EIGEN_NO_DEBUG` — disable runtime assertions (auto-set when `NDEBUG` is defined)
- `EIGEN_INITIALIZE_MATRICES_BY_NAN` — initialize all matrices to NaN
- `EIGEN_INITIALIZE_MATRICES_BY_ZERO` — initialize all matrices to zero
- `EIGEN_INTERNAL_DEBUGGING` — enable assertions in internal routines

**Extending Eigen:**
- `EIGEN_MATRIXBASE_PLUGIN`, `EIGEN_MATRIX_PLUGIN`, `EIGEN_ARRAYBASE_PLUGIN`, … — path to a header file `#include`d inside the class body, adding custom methods to all expressions of that base.

**Compiler annotations** (used in Eigen source code):
- `EIGEN_STRONG_INLINE` — force inline (`__forceinline` on MSVC)
- `EIGEN_ALWAYS_INLINE` — stronger than `STRONG_INLINE`
- `EIGEN_DEVICE_FUNC` — marks functions callable from CUDA/HIP/SYCL device code
- `EIGEN_DONT_INLINE` — prevent inlining
- `EIGEN_DEPRECATED` — deprecation marker

## Conventions worth knowing

(Header-only contract and `EIGEN_DEVICE_FUNC`: see agent guidelines 2 and 3.)

- **Aliasing**: aliasing safety is not a uniform invariant inside Eigen. The standard product-evaluation path inserts an auto-temporary so `mat = mat * mat` is safe; many other paths rely on the user being correct (`.noalias()` is a promise from the caller, not a check). Optimized fast paths that bypass the general assignment machinery have historically introduced aliasing bugs — when writing or modifying one, think explicitly about whether the LHS can alias the RHS, and prefer falling back to the general path when in doubt. User-facing rules are in "Common pitfalls" below.
- **Storage order**: most expressions are templated on `int Options` carrying `RowMajor` / `ColMajor`. When writing new evaluators, propagate `Flags & RowMajorBit` correctly — it is the source of many subtle bugs.
- **`Eigen::internal` namespace**: all internal implementation lives there. Public-facing types and functions stay in `Eigen::` (or specific module namespaces).
- **Default storage order**: column-major.
- **Index type**: `Eigen::Index` (alias for `std::ptrdiff_t`).
- **Naming**: classes PascalCase; methods camelCase; macros / constants `EIGEN_UPPER_CASE`.
- **Forward declarations**: `Eigen/src/Core/util/ForwardDeclarations.h` is the canonical entry point for "where is type X declared?". `internal::traits<T>` carries compile-time information (scalar type, dimensions, flags) without forward-declaration issues.
- **Assertions**: use `eigen_assert(cond)` (defined in `Eigen/src/Core/util/Macros.h`), not raw `assert()` / `static_assert()` for runtime preconditions in library code. The test harness redefines `eigen_assert` so `VERIFY_RAISES_ASSERT(expr)` can verify failures, and it honors `EIGEN_NO_DEBUG` / `NDEBUG`. For internal-only invariants, `eigen_internal_assert(cond)` is gated on `EIGEN_INTERNAL_DEBUGGING`. For compile-time conditions, `EIGEN_STATIC_ASSERT(cond, MSG_TOKEN)` is preferred over plain `static_assert` because it integrates with Eigen's diagnostic-token machinery.

## Common pitfalls

- **Aliasing**: `mat = mat * mat` is safe (auto-temporary), but `mat.noalias() = mat * mat` is **wrong**. Only use `.noalias()` when the destination doesn't appear on the right side.
- **`auto` with expressions**: `auto x = A + B;` captures a lazy expression holding references — the references may dangle. Use `auto x = (A + B).eval();` or an explicit type.
- **Missing headers**: some methods require additional includes (e.g. `cross()` needs `Eigen/Geometry`).
- **Ternary operator**: `cond ? exprA : exprB` can fail with expression types because the two branches have different types. Use `if/else`.
- **`template` keyword**: in dependent contexts, write `x.template triangularView<Upper>()` — without `template`, `<` is parsed as less-than.
- **Pass-by-value alignment**: pre-C++17, passing fixed-size vectorizable Eigen objects by value can crash due to alignment. Pass by const reference. With C++17 and modern compilers (GCC ≥ 7, Clang ≥ 5, MSVC ≥ 19.12), over-aligned allocation handles this automatically.
- **`Random()` not thread-safe**: `DenseBase::Random()` and `setRandom()` use `std::rand` internally and are not re-entrant. Use C++11 `<random>` generators via `NullaryExpr` for multi-threaded code.

## CI (GitLab)

Pipeline stages: `checkformat` → `build` → `test` → `deploy`. Configuration in `.gitlab-ci.yml` and `ci/*.gitlab-ci.yml`; shell drivers under `ci/scripts/`.

`build` jobs produce a `.build/` artifact (test binaries) consumed by the matching `test` job — the test job only runs `ctest`, it does **not** rebuild. A test job that runs ctest without restricting via `-L` or `-R` will report `Could not find executable` for everything outside the build job's target. Test-job and build-job names must stay paired (see `needs:` in `ci/test.linux.gitlab-ci.yml`).

Test jobs filter via `EIGEN_CI_CTEST_LABEL` (consumed in `ci/scripts/test.linux.script.sh` as `ctest -L $LABEL`). The `:official` and `:unsupported` job-name suffixes are convention only — actual filtering is through that variable. Defaults live in `ci/scripts/vars.linux.sh` (`EIGEN_CI_CTEST_LABEL=Official`, `EIGEN_CI_BUILD_TARGET=buildtests`, `EIGEN_CI_BUILDDIR=.build`).

MR pipelines build / run only a smoke subset; scheduled (nightly) pipelines exercise the full matrix. Format failures (`scripts/format.sh` diff) are the single most common reason an MR is red — run it before pushing.

### Commit message convention

`Category: Short description` (e.g. `GPU: Fix special-function test coverage`, `TriangularView: alias-aware fallback for structured-diagonal product fast path`).
