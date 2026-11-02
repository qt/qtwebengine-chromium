# SIMD and GPU Changes

Use this guide for packet math, architecture backends, device annotations, CUDA/HIP/SYCL code, Tensor device
execution, and the `unsupported/Eigen/GPU` module. The repository-root `AGENTS.md` still applies.

## Packet math

Eigen's vectorization API is the `Eigen::internal` packet layer. `packet_traits` and `unpacket_traits` describe a
packet type and its capabilities; `p*` operations such as `pload`, `pstore`, `padd`, and `pmul` provide the common
interface used by evaluators.

- Scalar fallbacks and the default traits live in `Eigen/src/Core/GenericPacketMath.h`. Shared vector
  implementations live under `Eigen/src/Core/arch/Default/`; backend specializations live in the relevant sibling
  directories under `Eigen/src/Core/arch/`.
- Start a new operation with a correct generic fallback when one is possible. Add specializations only for relevant
  backends that support the operation; do not require an implementation in every backend merely because one backend
  gains an intrinsic.
- Guard each intrinsic with the feature macro that enables it, even inside a broader backend directory. For example,
  AVX2 or FMA intrinsics in `arch/AVX/` still require `EIGEN_VECTORIZE_AVX2` or `EIGEN_VECTORIZE_FMA`, plus a fallback
  for narrower configurations. Consult `Eigen/src/Core/util/ConfigureVectorization.h` for the current feature macros.
- Keep capability flags (`Has*`), packet and half-packet types, alignment, masked access, casts, and cost metadata
  consistent with the implementation. A capability flag must not advertise an unavailable or semantically different
  operation.
- Preserve scalar-remainder behavior and unaligned paths. Packet-sized inputs alone do not cover an evaluator.
- Standard mathematical functions should match the scalar contract for special values. Measure ordinary-input error
  in ULPs against an appropriate scalar or higher-precision reference; test NaN, infinities, signed zero, subnormals,
  and domain boundaries explicitly where the platform exposes those IEEE-754 behaviors.

The current source tree and `test/CMakeLists.txt` are authoritative for supported backends and configuration options;
do not copy an architecture inventory into documentation.

## Device-callable code

For CUDA and HIP, `EIGEN_DEVICE_FUNC` supplies the host/device qualifiers required by functions reached from device
code. Under SYCL device compilation it supplies Eigen's required flattening and inlining attributes rather than alone
determining callability. Preserve it on coefficient accessors, evaluators, functors, small helpers, constructors, and
operators reached from device code.

- Keep device code allocation-free unless the specific backend and API deliberately provide an allocator.
- Avoid host-only standard-library calls, exceptions, RTTI assumptions, and function-local static state on device
  paths.
- Define configuration macros before the first Eigen public header and keep index configuration consistent across
  translation units that exchange Eigen objects.
- Include public module headers in tests and examples. Implementation headers under `Eigen/src/` and
  `unsupported/Eigen/src/` are not user include points.

## Three GPU models

### Core types inside kernels

CUDA and HIP kernels can use fixed-size owning matrices, vectors, and arrays through public Eigen headers; see
`doc/UsingNVCC.dox`. Dynamic owning `Matrix` and `Array` objects require allocation and are generally unsuitable
inside kernels. Runtime dimensions are not inherently unsupported: `Map` over caller-managed device memory can use
dynamic dimensions when every operation reached by the expression is device-callable.

CUDA/HIP compilation disables host SIMD. Move substantial host-side Eigen work to a normal `.cpp` translation unit.
Use `EIGEN_NO_CUDA` or `EIGEN_NO_HIP` only when the corresponding compiler processes Eigen exclusively for host use.
If device code requires a different dense index type, define `EIGEN_DEFAULT_DENSE_INDEX_TYPE` consistently wherever
objects cross the host/device boundary.

### Tensor devices

`unsupported/Eigen/Tensor` evaluates expressions through an explicit device. `GpuDevice` handles CUDA/HIP and
`SyclDevice` handles SYCL; Tensor GPU kernels remain part of the Tensor implementation. Device-resident storage is
normally supplied through `TensorMap`, and the destination selects execution with `out.device(device) = expression`.
Consult `unsupported/Eigen/src/Tensor/README.md` and the nearby device implementation before changing memory,
synchronization, or callback semantics.

### `unsupported/Eigen/GPU`

This is a host-side NVIDIA-library wrapper selected explicitly with `Eigen::gpu` types. `gpu::DeviceMatrix` is not a
`MatrixBase` expression, and a supported expression maps to a CUDA library operation rather than Core coefficient
evaluation or packet fusion. Define `EIGEN_USE_GPU` before including `<unsupported/Eigen/GPU>`, and consult
`unsupported/Eigen/src/GPU/README.md`. Its tests under `unsupported/test/GPU/` are intentionally host-compiled `.cpp`
files.

## Validation

- Packet API or math changes: run the relevant parts of `packetmath`, the generic packet tests, and
  `special_packetmath`; exercise every locally available affected backend.
- Core device-callability changes: build and run the relevant `gpu_basic` or `gpu_example` target with the available
  CUDA/HIP compiler.
- Tensor device changes: run the operation's CPU Tensor test plus the matching CUDA/HIP/SYCL test where available.
- `unsupported/Eigen/GPU` changes: run the focused target under `unsupported/test/GPU/` and any affected library
  integration target enabled by the local toolkit.
- Report backends or hardware that were unavailable. Do not claim cross-backend validation from a host-only build.

Performance-sensitive packet or GPU changes require a representative benchmark under identical compiler flags,
device state, and workload conditions. Correctness tests are not performance evidence.
