# Copyright (c) 2017 Facebook Inc.
# Copyright (c) 2015-2017 Georgia Institute of Technology
# All rights reserved.
#
# Copyright 2019 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

CMAKE_MINIMUM_REQUIRED(VERSION 3.5 FATAL_ERROR)

PROJECT(googletest-download NONE)

INCLUDE(ExternalProject)
ExternalProject_Add(googletest
	URL https://github.com/google/googletest/archive/d72f9c8aea6817cdf1ca0ac10887f328de7f3da2.zip
	URL_HASH SHA256=a4cb11930215b071168811982dfbebc82a2bb0f90db0e8713245931eb742ea46
	SOURCE_DIR "${CMAKE_BINARY_DIR}/googletest-source"
	BINARY_DIR "${CMAKE_BINARY_DIR}/googletest"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)
