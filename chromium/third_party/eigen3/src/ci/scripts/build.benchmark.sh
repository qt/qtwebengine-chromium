#!/bin/bash
# Build Eigen benchmarks for a given ISA target.
#
# Expected environment variables:
#   EIGEN_CI_BUILDDIR         - build directory (default: .bench-build)
#   EIGEN_CI_CXX_COMPILER     - C++ compiler
#   EIGEN_CI_C_COMPILER        - C compiler
#   EIGEN_BENCH_ISA_FLAGS     - ISA-specific compiler flags (e.g. "-mavx2 -mfma")
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

set -ex

rootdir=$(pwd)
builddir=${EIGEN_CI_BUILDDIR:-.bench-build}
mkdir -p "${builddir}"
cd "${builddir}"

# Install Google Benchmark from source if not already present.
# The common before_script already installs cmake/ninja; we only need
# git and ca-certificates for the clone. The CI image only ships
# versioned compilers (e.g. g++-10), so unversioned c++/cc are absent
# and we must pass CMAKE_*_COMPILER explicitly.
if ! pkg-config --exists benchmark 2>/dev/null; then
  apt-get update -qq
  apt-get install -y --no-install-recommends git ca-certificates
  git clone --depth 1 --branch v1.9.1 https://github.com/google/benchmark.git /tmp/gbench
  cmake -G Ninja -S /tmp/gbench -B /tmp/gbench-build \
    -DCMAKE_C_COMPILER="${EIGEN_CI_C_COMPILER}" \
    -DCMAKE_CXX_COMPILER="${EIGEN_CI_CXX_COMPILER}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBENCHMARK_ENABLE_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr/local
  cmake --build /tmp/gbench-build --target install
  rm -rf /tmp/gbench /tmp/gbench-build
fi

# Configure benchmarks.  ISA flags are passed via CMAKE_CXX_FLAGS so they
# apply globally to all benchmark targets.
cmake -G Ninja \
  -DCMAKE_CXX_COMPILER="${EIGEN_CI_CXX_COMPILER}" \
  -DCMAKE_C_COMPILER="${EIGEN_CI_C_COMPILER}" \
  -DCMAKE_CXX_FLAGS="${EIGEN_BENCH_ISA_FLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  "${rootdir}/benchmarks"

# Build all benchmark targets.  The nightly/weekly scope filtering happens
# at run time, not build time.
cmake --build . -- -k0 || cmake --build . -- -k0 -j1

cd "${rootdir}"
