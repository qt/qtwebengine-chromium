// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/webplugininfo.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"

namespace content {

WebPluginMimeType::WebPluginMimeType() {}

WebPluginMimeType::WebPluginMimeType(const std::string& m,
                                     const std::string& f,
                                     const std::string& d)
    : mime_type(m),
      file_extensions(),
      description(ASCIIToUTF16(d)) {
  file_extensions.push_back(f);
}

WebPluginMimeType::~WebPluginMimeType() {}

WebPluginInfo::WebPluginInfo()
    : type(PLUGIN_TYPE_NPAPI),
      pepper_permissions(0) {
}

WebPluginInfo::WebPluginInfo(const WebPluginInfo& rhs)
    : name(rhs.name),
      path(rhs.path),
      version(rhs.version),
      desc(rhs.desc),
      mime_types(rhs.mime_types),
      type(rhs.type),
      pepper_permissions(rhs.pepper_permissions) {
}

WebPluginInfo::~WebPluginInfo() {}

WebPluginInfo& WebPluginInfo::operator=(const WebPluginInfo& rhs) {
  name = rhs.name;
  path = rhs.path;
  version = rhs.version;
  desc = rhs.desc;
  mime_types = rhs.mime_types;
  type = rhs.type;
  pepper_permissions = rhs.pepper_permissions;
  return *this;
}

WebPluginInfo::WebPluginInfo(const base::string16& fake_name,
                             const base::FilePath& fake_path,
                             const base::string16& fake_version,
                             const base::string16& fake_desc)
    : name(fake_name),
      path(fake_path),
      version(fake_version),
      desc(fake_desc),
      mime_types(),
      type(PLUGIN_TYPE_NPAPI),
      pepper_permissions(0) {
}

void WebPluginInfo::CreateVersionFromString(
    const base::string16& version_string,
    base::Version* parsed_version) {
  // Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::string version = UTF16ToASCII(version_string);
  base::RemoveChars(version, ") ", &version);
  std::replace(version.begin(), version.end(), 'd', '.');
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  // Remove leading zeros from each of the version components.
  std::string no_leading_zeros_version;
  std::vector<std::string> numbers;
  base::SplitString(version, '.', &numbers);
  for (size_t i = 0; i < numbers.size(); ++i) {
    size_t n = numbers[i].size();
    size_t j = 0;
    while (j < n && numbers[i][j] == '0') {
      ++j;
    }
    no_leading_zeros_version += (j < n) ? numbers[i].substr(j) : "0";
    if (i != numbers.size() - 1) {
      no_leading_zeros_version += ".";
    }
  }

  *parsed_version = Version(no_leading_zeros_version);
}

}  // namespace content
