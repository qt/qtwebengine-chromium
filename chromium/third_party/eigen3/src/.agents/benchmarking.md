# Benchmarking

Use this guidance for performance-sensitive changes and benchmark reviews. Performance claims need a benchmark that
ships in the same merge request; correctness tests still ship separately and run before timing.

## Projects and Builds

The supported and unsupported benchmark trees are separate, standalone CMake projects. They are not part of Eigen's
main test build and both require Google Benchmark:

```bash
cmake -G Ninja -S benchmarks -B build-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --target <benchmark-target>

cmake -G Ninja -S unsupported/benchmarks -B build-unsupported-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-unsupported-bench --target <benchmark-target>
```

The unsupported parent project automatically adds its GPU subtree when it detects `CUDAToolkit`. That configuration
also requires a working CUDA compiler and architecture selection. On a host with only a partial toolkit installation,
configure CPU-only unsupported benchmarks with `-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE` or report the GPU
configuration as unavailable.

Consult [`benchmarks/CMakeLists.txt`](../benchmarks/CMakeLists.txt) and
[`unsupported/benchmarks/CMakeLists.txt`](../unsupported/benchmarks/CMakeLists.txt) for current targets and compile
settings. CUDA benchmarks also have a standalone project and instructions in
[`unsupported/benchmarks/GPU/CMakeLists.txt`](../unsupported/benchmarks/GPU/CMakeLists.txt). The CI scripts
[`build.benchmark.sh`](../ci/scripts/build.benchmark.sh) and
[`run.benchmark.sh`](../ci/scripts/run.benchmark.sh) describe the supported-tree scheduled build and result format;
do not assume they validate `unsupported/benchmarks` changes.

## Benchmark Design

- Benchmark the user-visible operation affected by the change, with representative scalar types, sizes, shapes,
  storage layouts, sparsity, and thread counts. Include transition sizes where a kernel or blocking strategy changes.
- Keep allocation, input generation, validation, and unrelated setup outside the timed region. Prevent dead-code
  elimination with Google Benchmark's `DoNotOptimize` and `ClobberMemory` where appropriate.
- Validate results outside the measured loop. A faster incorrect kernel is not a useful result.
- Use enough work per iteration to dominate timer noise without hiding important small-problem behavior. Report
  meaningful rates or byte/operation counters when they improve interpretation.
- Compare the change against the relevant baseline with identical compiler, optimization, ISA, dependency, and
  benchmark arguments. Record the commit, hardware, compiler, flags, and command needed to reproduce the result.

## Argument Grids

Express static grids declaratively on the registration:

- `Args({a, b})` for individual points.
- `Range`, `DenseRange`, or `Ranges` for swept dimensions.
- `ArgsProduct({{...}, {...}})` for Cartesian products.

Use `Apply()` only for a genuinely computed grid that these APIs cannot express. In that exceptional case, match the
Google Benchmark version used by the project and note that the callback currently names
`benchmark::internal::Benchmark*`, an internal API. Prefer an existing local pattern and keep the grid-generation
function small and deterministic.

## Running Measurements

1. Check `uptime` and stop or finish competing builds and compute-heavy work. Run only one benchmark process at a
   time; concurrent benchmarks invalidate both measurements.
2. Keep the machine, CPU affinity, power/governor policy, thermal state, compiler, flags, ISA, and dependencies as
   constant as practical. Disclose anything that could not be controlled.
3. Use multiple repetitions, for example `--benchmark_repetitions=10`, and retain raw results. Compare medians plus a
   dispersion measure such as MAD, IQR, or standard deviation; do not select the best run.
4. For before/after binaries, alternate separate invocations (`A, B, A, B`) to expose thermal or background-load
   drift. Use the same benchmark filter and arguments for each pair.
5. Re-run suspicious or noisy cases. Treat changes smaller than the observed run-to-run variation as inconclusive,
   not as wins or regressions.

Never infer a general speedup from one convenient size or one warm run. State the tested domain, include regressions
as well as improvements, and keep numerical accuracy results separate from performance measurements.
