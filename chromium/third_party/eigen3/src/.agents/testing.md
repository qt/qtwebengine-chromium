# Testing Eigen Changes

Use this guide when adding or changing tests. The checked-out source is authoritative:

- [`test/main.h`](../test/main.h) configures and runs the test framework and aggregates the shared helpers.
- [`test/numerical_test_helpers.h`](../test/numerical_test_helpers.h) defines numerical comparison, assertion, and
  tolerance helpers.
- [`test/product_test_helpers.h`](../test/product_test_helpers.h) defines matrix-product error bounds.
- [`test/random_matrix_helper.h`](../test/random_matrix_helper.h) and
  [`test/type_test_helpers.h`](../test/type_test_helpers.h) define random-matrix and type utilities.
- [`cmake/EigenTesting.cmake`](../cmake/EigenTesting.cmake) defines test registration and splitting.
- [`test/CMakeLists.txt`](../test/CMakeLists.txt) and
  [`unsupported/test/CMakeLists.txt`](../unsupported/test/CMakeLists.txt) register the suites.
- [`cmake/EigenConfigureTesting.cmake`](../cmake/EigenConfigureTesting.cmake) defines aggregate build and check
  targets.

## Configure And Build

Configure a dedicated build directory. Unit tests are excluded from CMake's default `all` target, although a bare
build may still build enabled auxiliary libraries.

```bash
cmake -G Ninja -S . -B build
cmake --build build --target buildtests
ctest --test-dir build --parallel --output-on-failure
```

Useful aggregate targets are `BuildOfficial`, `BuildUnsupported`, `buildsmoketests`, `buildtests_gpu`, `check`, and
`check_gpu`. Build and run one test explicitly when possible:

```bash
cmake --build build --target bdcsvd_3
ctest --test-dir build -R '^bdcsvd_3$' --output-on-failure
```

Run the generated wrappers from the build directory because they invoke the configured build tool relative to their
working directory:

```bash
cd build
./buildtests.sh <regex>
./check.sh <regex>
```

They filter registered parent names such as `bdcsvd`, not generated part names such as `bdcsvd_3`; use the explicit
target recipe for one part.

Use a separate build directory for each materially different configuration. Do not rewrite one cache and describe
the result as a second test run.

```bash
cmake -G Ninja -S . -B build-row-major -DEIGEN_DEFAULT_TO_ROW_MAJOR=ON
cmake -G Ninja -S . -B build-no-vector -DEIGEN_TEST_NO_EXPLICIT_VECTORIZATION=ON
```

Consult the top-level [`CMakeLists.txt`](../CMakeLists.txt) and nearby test CMake files for current options instead of
copying an option inventory into documentation.

## Current Test Framework

Eigen currently uses its own framework, not GoogleTest:

1. Add `test/<name>.cpp` or `unsupported/test/<name>.cpp`.
2. Include `main.h`, then the public umbrella header for tests of public behavior. A focused test of a private utility
   may include its implementation header only when that matches an established nearby pattern; never present such a
   path as a user include.
3. Use `VERIFY`, `VERIFY_IS_EQUAL`, `VERIFY_IS_APPROX`, and the other helpers exposed through `test/main.h`.
4. End with `EIGEN_DECLARE_TEST(<name>) { ... }`.
5. Register the source with `ei_add_test(<name>)` in the matching `CMakeLists.txt`, then reconfigure.

Keep `test/main.h` limited to framework configuration, registration, shared-helper aggregation, and the test driver.
Put reusable utilities in a narrowly named helper header; include it from `main.h` only when most tests need it.

For compile-failure coverage, use the established `failtest/` pattern. Its `_ok` target must compile and its `_ko`
target must fail with `EIGEN_SHOULD_FAIL_TO_BUILD` defined.

## Split Tests

`ei_add_test` scans the source for `CALL_SUBTEST_N`, `EIGEN_TEST_PART_N`, and `EIGEN_SUFFIXES;...` markers.

- With `EIGEN_SPLIT_LARGE_TESTS=ON`, every discovered suffix becomes an executable `<name>_<N>` compiled with
  `EIGEN_TEST_PART_<N>=1`; the parent `<name>` target builds all parts.
- `EIGEN_SUFFIXES;...` supplies an explicit suffix list when ordinary source scanning cannot see macro-generated or
  conditional parts.
- With splitting off, tests containing only `CALL_SUBTEST_N` or `EIGEN_SUFFIXES` fold into one `<name>` executable
  compiled with `EIGEN_TEST_PART_ALL=1`.
- An explicit `EIGEN_TEST_PART_N` marker forces splitting even when the option is off. If any such marker is present,
  all suffixes discovered in that source are emitted.

`ctest -R '^<name>$'` does not match split parts. Use `ctest -R '<name>'` for every part or anchor one generated name.

## Numerical Assertions

`VERIFY_IS_APPROX` is a convenient broad comparison, not a machine-epsilon guarantee. `test_precision<T>()` uses
`NumTraits<T>::dummy_precision()` generically and currently specializes float to `1e-3` and double/long double to
`1e-6`. Do not use it alone to claim ULP accuracy, backward stability, or IEEE special-value conformance.

For numerical kernels, add explicit named bounds based on epsilon, dimension, conditioning, or a backward-error
model as appropriate. Check NaN, infinity, and signed zero explicitly when their distinction matters. Follow
[`numerics.md`](numerics.md) for solver, packet, and scalar-math coverage.

Run reproducible failures directly with a fixed seed and repeat count:

```bash
EIGEN_REPEAT=10 EIGEN_SEED=1 build/test/foo_3
build/test/foo_3 r10 s1
```

## External BLAS And Shim Libraries

`EIGEN_TEST_EXTERNAL_BLAS=ON` finds a system BLAS, defines `EIGEN_USE_BLAS`, and links that BLAS into applicable
official tests. With it off, ordinary tests exercise Eigen's normal implementation; they do not transparently use
the in-tree `eigen_blas` library. `EIGEN_BUILD_BLAS` and `EIGEN_BUILD_LAPACK` separately build Eigen's ABI shim
libraries, which are also used to satisfy some optional sparse-backend links. There is currently no
`EIGEN_TEST_EXTERNAL_LAPACK` option.

Report the exact targets, CTest regexes, configurations, compiler, and seeds run. Also report relevant hardware or
optional backends that were unavailable locally.
