// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_UTILS_PATH_SERVICE_H_
#define TESTING_UTILS_PATH_SERVICE_H_

#include <string>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

// Get the various file directory and path information.
class PathService {
 public:
  // Returns true when the path is a directory that exists.
  static bool DirectoryExists(const std::string& path);

  // Returns true when the path ends with a path separator.
  static bool EndsWithSeparator(const std::string& path);

  // Retrieves the directory where executables run from.
  // Returns an empty string on failure.
  static std::string GetExecutableDir();

  // Retrieves the root directory of the source tree.
  // Assumes executables always run from out/<build_dir_name>/, so the source
  // directory is two levels above the executable directory.
  // Returns an empty string on failure.
  static std::string GetSourceDir();

  // Retrieves the test data directory where test files are stored.
  // Tries <source_dir>/testing/resources/ first. If it does not exist, tries
  // checking <source_dir>/third_party/pdfium/testing/resources/.
  // Returns an empty string on failure.
  static std::string GetTestDataDir();

  // Gets the full path for a test file under the test data directory.
  // Returns an empty string on failure.
  static std::string GetTestFilePath(const std::string& file_name);

  // Gets the full path for a file under the third-party directory.
  // Returns an empty string on failure.
  static std::string GetThirdPartyFilePath(const std::string& file_name);
};
#endif  // TESTING_UTILS_PATH_SERVICE_H_
