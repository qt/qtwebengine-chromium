# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

eigen_add_benchmark(bench_cholesky bench_cholesky.cpp)
eigen_add_benchmark(bench_cholesky_double bench_cholesky.cpp DEFINITIONS SCALAR=double)
