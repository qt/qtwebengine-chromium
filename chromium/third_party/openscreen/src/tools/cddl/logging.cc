// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/logging.h"

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

namespace Logger {

namespace {

std::once_flag g_header_written_flag;

void WriteHeader() {
  std::cerr << "CDDL GENERATION TOOL" << std::endl;
  std::cerr << "---------------------------------------------" << std::endl;
}

void EnsureHeaderWritten() {
  std::call_once(g_header_written_flag, WriteHeader);
}

}  // namespace

void Log(std::string_view message) {
  EnsureHeaderWritten();
  std::cerr << message << std::endl;
}

void Error(std::string_view message) {
  EnsureHeaderWritten();
  std::cerr << "Error: " << message << std::endl;
}

[[noreturn]] void Abort(const char* condition) {
  std::cerr << "CHECK(" << condition << ") failed!" << std::endl;
  std::abort();
}

}  // namespace Logger
