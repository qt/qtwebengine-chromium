// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>
#include "bench/utils.h"

#include <xnnpack.h>
#include <xnnpack/aligned-allocator.h>
#include <xnnpack/common.h>
#include <xnnpack/microfnptr.h>
#include <xnnpack/microparams-init.h>
#include <xnnpack/vcvt.h>


static void qs8_f32_vcvt(
  benchmark::State& state,
  xnn_qs8_f32_vcvt_ukernel_fn cvt,
  xnn_init_qs8_f32_cvt_params_fn init_params,
  benchmark::utils::IsaCheckFunction isa_check = nullptr)
{
  if (isa_check != nullptr && !isa_check(state)) {
    return;
  }

  const size_t num_elements = state.range(0);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto i8rng = std::bind(
    std::uniform_int_distribution<int32_t>(std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max()),
    std::ref(rng));

  std::vector<int8_t, AlignedAllocator<int8_t, 64>> x(num_elements + XNN_EXTRA_BYTES / sizeof(int8_t));
  std::vector<float, AlignedAllocator<float, 64>> y(num_elements);
  std::generate(x.begin(), x.end(), std::ref(i8rng));
  std::fill(y.begin(), y.end(), std::nanf(""));

  xnn_qs8_f32_cvt_params params;
  init_params(&params,
    0.25f /* scale */,
    1 /* output zero point */);
  for (auto _ : state) {
    cvt(num_elements * sizeof(int8_t), x.data(), y.data(), &params);
  }

  const uint64_t cpu_frequency = benchmark::utils::GetCurrentCpuFrequency();
  if (cpu_frequency != 0) {
    state.counters["cpufreq"] = cpu_frequency;
  }

  const size_t elements_per_iteration = num_elements;
  state.counters["elements"] =
    benchmark::Counter(uint64_t(state.iterations()) * elements_per_iteration, benchmark::Counter::kIsRate);

  const size_t bytes_per_iteration = num_elements * (sizeof(int8_t) + sizeof(float));
  state.counters["bytes"] =
    benchmark::Counter(uint64_t(state.iterations()) * bytes_per_iteration, benchmark::Counter::kIsRate);
}

#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  BENCHMARK_CAPTURE(qs8_f32_vcvt, neon_u8,
                    xnn_qs8_f32_vcvt_ukernel__neon_u8,
                    xnn_init_qs8_f32_cvt_neon_params,
                    benchmark::utils::CheckNEON)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, neon_u16,
                    xnn_qs8_f32_vcvt_ukernel__neon_u16,
                    xnn_init_qs8_f32_cvt_neon_params,
                    benchmark::utils::CheckNEON)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, neon_u24,
                    xnn_qs8_f32_vcvt_ukernel__neon_u24,
                    xnn_init_qs8_f32_cvt_neon_params,
                    benchmark::utils::CheckNEON)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, neon_u32,
                    xnn_qs8_f32_vcvt_ukernel__neon_u32,
                    xnn_init_qs8_f32_cvt_neon_params,
                    benchmark::utils::CheckNEON)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64

#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx512skx_u16,
                    xnn_qs8_f32_vcvt_ukernel__avx512skx_u16,
                    xnn_init_qs8_f32_cvt_avx512_params,
                    benchmark::utils::CheckAVX512SKX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx512skx_u32,
                    xnn_qs8_f32_vcvt_ukernel__avx512skx_u32,
                    xnn_init_qs8_f32_cvt_avx512_params,
                    benchmark::utils::CheckAVX512SKX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx512skx_u48,
                    xnn_qs8_f32_vcvt_ukernel__avx512skx_u48,
                    xnn_init_qs8_f32_cvt_avx512_params,
                    benchmark::utils::CheckAVX512SKX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx512skx_u64,
                    xnn_qs8_f32_vcvt_ukernel__avx512skx_u64,
                    xnn_init_qs8_f32_cvt_avx512_params,
                    benchmark::utils::CheckAVX512SKX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();

  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx2_u8,
                    xnn_qs8_f32_vcvt_ukernel__avx2_u8,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx2_u16,
                    xnn_qs8_f32_vcvt_ukernel__avx2_u16,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx2_u24,
                    xnn_qs8_f32_vcvt_ukernel__avx2_u24,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx2_u32,
                    xnn_qs8_f32_vcvt_ukernel__avx2_u32,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX2)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();

  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx_u8,
                    xnn_qs8_f32_vcvt_ukernel__avx_u8,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx_u16,
                    xnn_qs8_f32_vcvt_ukernel__avx_u16,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx_u24,
                    xnn_qs8_f32_vcvt_ukernel__avx_u24,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, avx_u32,
                    xnn_qs8_f32_vcvt_ukernel__avx_u32,
                    xnn_init_qs8_f32_cvt_avx_params,
                    benchmark::utils::CheckAVX)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();

  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse41_u8,
                    xnn_qs8_f32_vcvt_ukernel__sse41_u8,
                    xnn_init_qs8_f32_cvt_sse4_params,
                    benchmark::utils::CheckSSE41)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse41_u16,
                    xnn_qs8_f32_vcvt_ukernel__sse41_u16,
                    xnn_init_qs8_f32_cvt_sse4_params,
                    benchmark::utils::CheckSSE41)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse41_u24,
                    xnn_qs8_f32_vcvt_ukernel__sse41_u24,
                    xnn_init_qs8_f32_cvt_sse4_params,
                    benchmark::utils::CheckSSE41)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse41_u32,
                    xnn_qs8_f32_vcvt_ukernel__sse41_u32,
                    xnn_init_qs8_f32_cvt_sse4_params,
                    benchmark::utils::CheckSSE41)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();

  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse2_u8,
                    xnn_qs8_f32_vcvt_ukernel__sse2_u8,
                    xnn_init_qs8_f32_cvt_sse2_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse2_u16,
                    xnn_qs8_f32_vcvt_ukernel__sse2_u16,
                    xnn_init_qs8_f32_cvt_sse2_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse2_u24,
                    xnn_qs8_f32_vcvt_ukernel__sse2_u24,
                    xnn_init_qs8_f32_cvt_sse2_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, sse2_u32,
                    xnn_qs8_f32_vcvt_ukernel__sse2_u32,
                    xnn_init_qs8_f32_cvt_sse2_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64

#if XNN_ARCH_WASMSIMD || XNN_ARCH_WASMRELAXEDSIMD
  BENCHMARK_CAPTURE(qs8_f32_vcvt, wasmsimd_u8,
                    xnn_qs8_f32_vcvt_ukernel__wasmsimd_u8,
                    xnn_init_qs8_f32_cvt_wasmsimd_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, wasmsimd_u16,
                    xnn_qs8_f32_vcvt_ukernel__wasmsimd_u16,
                    xnn_init_qs8_f32_cvt_wasmsimd_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, wasmsimd_u24,
                    xnn_qs8_f32_vcvt_ukernel__wasmsimd_u24,
                    xnn_init_qs8_f32_cvt_wasmsimd_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
  BENCHMARK_CAPTURE(qs8_f32_vcvt, wasmsimd_u32,
                    xnn_qs8_f32_vcvt_ukernel__wasmsimd_u32,
                    xnn_init_qs8_f32_cvt_wasmsimd_params)
    ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
    ->UseRealTime();
#endif  // XNN_ARCH_WASMSIMD || XNN_ARCH_WASMRELAXEDSIMD

BENCHMARK_CAPTURE(qs8_f32_vcvt, scalar_u1,
                  xnn_qs8_f32_vcvt_ukernel__scalar_u1,
                  xnn_init_qs8_f32_cvt_scalar_params)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
  ->UseRealTime();
BENCHMARK_CAPTURE(qs8_f32_vcvt, scalar_u2,
                  xnn_qs8_f32_vcvt_ukernel__scalar_u2,
                  xnn_init_qs8_f32_cvt_scalar_params)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
  ->UseRealTime();
BENCHMARK_CAPTURE(qs8_f32_vcvt, scalar_u3,
                  xnn_qs8_f32_vcvt_ukernel__scalar_u3,
                  xnn_init_qs8_f32_cvt_scalar_params)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
  ->UseRealTime();
BENCHMARK_CAPTURE(qs8_f32_vcvt, scalar_u4,
                  xnn_qs8_f32_vcvt_ukernel__scalar_u4,
                  xnn_init_qs8_f32_cvt_scalar_params)
  ->Apply(benchmark::utils::UnaryElementwiseParameters<int8_t, float>)
  ->UseRealTime();

#ifndef XNNPACK_BENCHMARK_NO_MAIN
BENCHMARK_MAIN();
#endif
