// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/url_pattern.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"
#include "url/url_util.h"

const char URLPattern::kAllUrlsPattern[] = "<all_urls>";

namespace {

// TODO(aa): What about more obscure schemes like data: and javascript: ?
// Note: keep this array in sync with kValidSchemeMasks.
const char* kValidSchemes[] = {
  content::kHttpScheme,
  content::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kChromeUIScheme,
  extensions::kExtensionScheme,
  chrome::kFileSystemScheme,
};

const int kValidSchemeMasks[] = {
  URLPattern::SCHEME_HTTP,
  URLPattern::SCHEME_HTTPS,
  URLPattern::SCHEME_FILE,
  URLPattern::SCHEME_FTP,
  URLPattern::SCHEME_CHROMEUI,
  URLPattern::SCHEME_EXTENSION,
  URLPattern::SCHEME_FILESYSTEM,
};

COMPILE_ASSERT(arraysize(kValidSchemes) == arraysize(kValidSchemeMasks),
               must_keep_these_arrays_in_sync);

const char kParseSuccess[] = "Success.";
const char kParseErrorMissingSchemeSeparator[] = "Missing scheme separator.";
const char kParseErrorInvalidScheme[] = "Invalid scheme.";
const char kParseErrorWrongSchemeType[] = "Wrong scheme type.";
const char kParseErrorEmptyHost[] = "Host can not be empty.";
const char kParseErrorInvalidHostWildcard[] = "Invalid host wildcard.";
const char kParseErrorEmptyPath[] = "Empty path.";
const char kParseErrorInvalidPort[] = "Invalid port.";

// Message explaining each URLPattern::ParseResult.
const char* const kParseResultMessages[] = {
  kParseSuccess,
  kParseErrorMissingSchemeSeparator,
  kParseErrorInvalidScheme,
  kParseErrorWrongSchemeType,
  kParseErrorEmptyHost,
  kParseErrorInvalidHostWildcard,
  kParseErrorEmptyPath,
  kParseErrorInvalidPort,
};

COMPILE_ASSERT(URLPattern::NUM_PARSE_RESULTS == arraysize(kParseResultMessages),
               must_add_message_for_each_parse_result);

const char kPathSeparator[] = "/";

bool IsStandardScheme(const std::string& scheme) {
  // "*" gets the same treatment as a standard scheme.
  if (scheme == "*")
    return true;

  return url_util::IsStandard(scheme.c_str(),
      url_parse::Component(0, static_cast<int>(scheme.length())));
}

bool IsValidPortForScheme(const std::string& scheme, const std::string& port) {
  if (port == "*")
    return true;

  // Only accept non-wildcard ports if the scheme uses ports.
  if (url_canon::DefaultPortForScheme(scheme.c_str(), scheme.length()) ==
      url_parse::PORT_UNSPECIFIED) {
    return false;
  }

  int parsed_port = url_parse::PORT_UNSPECIFIED;
  if (!base::StringToInt(port, &parsed_port))
    return false;
  return (parsed_port >= 0) && (parsed_port < 65536);
}

// Returns |path| with the trailing wildcard stripped if one existed.
//
// The functions that rely on this (OverlapsWith and Contains) are only
// called for the patterns inside URLPatternSet. In those cases, we know that
// the path will have only a single wildcard at the end. This makes figuring
// out overlap much easier. It seems like there is probably a computer-sciency
// way to solve the general case, but we don't need that yet.
std::string StripTrailingWildcard(const std::string& path) {
  size_t wildcard_index = path.find('*');
  size_t path_last = path.size() - 1;
  DCHECK(wildcard_index == std::string::npos || wildcard_index == path_last);
  return wildcard_index == path_last ? path.substr(0, path_last) : path;
}

}  // namespace

URLPattern::URLPattern()
    : valid_schemes_(SCHEME_NONE),
      match_all_urls_(false),
      match_subdomains_(false),
      port_("*") {}

URLPattern::URLPattern(int valid_schemes)
    : valid_schemes_(valid_schemes),
      match_all_urls_(false),
      match_subdomains_(false),
      port_("*") {}

URLPattern::URLPattern(int valid_schemes, const std::string& pattern)
    // Strict error checking is used, because this constructor is only
    // appropriate when we know |pattern| is valid.
    : valid_schemes_(valid_schemes),
      match_all_urls_(false),
      match_subdomains_(false),
      port_("*") {
  if (PARSE_SUCCESS != Parse(pattern))
    NOTREACHED() << "URLPattern is invalid: " << pattern;
}

URLPattern::~URLPattern() {
}

bool URLPattern::operator<(const URLPattern& other) const {
  return GetAsString() < other.GetAsString();
}

bool URLPattern::operator==(const URLPattern& other) const {
  return GetAsString() == other.GetAsString();
}

URLPattern::ParseResult URLPattern::Parse(const std::string& pattern) {
  spec_.clear();
  SetMatchAllURLs(false);
  SetMatchSubdomains(false);
  SetPort("*");

  // Special case pattern to match every valid URL.
  if (pattern == kAllUrlsPattern) {
    SetMatchAllURLs(true);
    return PARSE_SUCCESS;
  }

  // Parse out the scheme.
  size_t scheme_end_pos = pattern.find(content::kStandardSchemeSeparator);
  bool has_standard_scheme_separator = true;

  // Some urls also use ':' alone as the scheme separator.
  if (scheme_end_pos == std::string::npos) {
    scheme_end_pos = pattern.find(':');
    has_standard_scheme_separator = false;
  }

  if (scheme_end_pos == std::string::npos)
    return PARSE_ERROR_MISSING_SCHEME_SEPARATOR;

  if (!SetScheme(pattern.substr(0, scheme_end_pos)))
    return PARSE_ERROR_INVALID_SCHEME;

  bool standard_scheme = IsStandardScheme(scheme_);
  if (standard_scheme != has_standard_scheme_separator)
    return PARSE_ERROR_WRONG_SCHEME_SEPARATOR;

  // Advance past the scheme separator.
  scheme_end_pos +=
      (standard_scheme ? strlen(content::kStandardSchemeSeparator) : 1);
  if (scheme_end_pos >= pattern.size())
    return PARSE_ERROR_EMPTY_HOST;

  // Parse out the host and path.
  size_t host_start_pos = scheme_end_pos;
  size_t path_start_pos = 0;

  if (!standard_scheme) {
    path_start_pos = host_start_pos;
  } else if (scheme_ == chrome::kFileScheme) {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);
    if (host_end_pos == std::string::npos) {
      // Allow hostname omission.
      // e.g. file://* is interpreted as file:///*,
      // file://foo* is interpreted as file:///foo*.
      path_start_pos = host_start_pos - 1;
    } else {
      // Ignore hostname if scheme is file://.
      // e.g. file://localhost/foo is equal to file:///foo.
      path_start_pos = host_end_pos;
    }
  } else {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);

    // Host is required.
    if (host_start_pos == host_end_pos)
      return PARSE_ERROR_EMPTY_HOST;

    if (host_end_pos == std::string::npos)
      return PARSE_ERROR_EMPTY_PATH;

    host_ = pattern.substr(host_start_pos, host_end_pos - host_start_pos);

    // The first component can optionally be '*' to match all subdomains.
    std::vector<std::string> host_components;
    base::SplitString(host_, '.', &host_components);
    if (host_components[0] == "*") {
      match_subdomains_ = true;
      host_components.erase(host_components.begin(),
                            host_components.begin() + 1);
    }
    host_ = JoinString(host_components, '.');

    path_start_pos = host_end_pos;
  }

  SetPath(pattern.substr(path_start_pos));

  size_t port_pos = host_.find(':');
  if (port_pos != std::string::npos) {
    if (!SetPort(host_.substr(port_pos + 1)))
      return PARSE_ERROR_INVALID_PORT;
    host_ = host_.substr(0, port_pos);
  }

  // No other '*' can occur in the host, though. This isn't necessary, but is
  // done as a convenience to developers who might otherwise be confused and
  // think '*' works as a glob in the host.
  if (host_.find('*') != std::string::npos)
    return PARSE_ERROR_INVALID_HOST_WILDCARD;

  return PARSE_SUCCESS;
}

void URLPattern::SetValidSchemes(int valid_schemes) {
  spec_.clear();
  valid_schemes_ = valid_schemes;
}

void URLPattern::SetHost(const std::string& host) {
  spec_.clear();
  host_ = host;
}

void URLPattern::SetMatchAllURLs(bool val) {
  spec_.clear();
  match_all_urls_ = val;

  if (val) {
    match_subdomains_ = true;
    scheme_ = "*";
    host_.clear();
    SetPath("/*");
  }
}

void URLPattern::SetMatchSubdomains(bool val) {
  spec_.clear();
  match_subdomains_ = val;
}

bool URLPattern::SetScheme(const std::string& scheme) {
  spec_.clear();
  scheme_ = scheme;
  if (scheme_ == "*") {
    valid_schemes_ &= (SCHEME_HTTP | SCHEME_HTTPS);
  } else if (!IsValidScheme(scheme_)) {
    return false;
  }
  return true;
}

bool URLPattern::IsValidScheme(const std::string& scheme) const {
  if (valid_schemes_ == SCHEME_ALL)
    return true;

  for (size_t i = 0; i < arraysize(kValidSchemes); ++i) {
    if (scheme == kValidSchemes[i] && (valid_schemes_ & kValidSchemeMasks[i]))
      return true;
  }

  return false;
}

void URLPattern::SetPath(const std::string& path) {
  spec_.clear();
  path_ = path;
  path_escaped_ = path_;
  ReplaceSubstringsAfterOffset(&path_escaped_, 0, "\\", "\\\\");
  ReplaceSubstringsAfterOffset(&path_escaped_, 0, "?", "\\?");
}

bool URLPattern::SetPort(const std::string& port) {
  spec_.clear();
  if (IsValidPortForScheme(scheme_, port)) {
    port_ = port;
    return true;
  }
  return false;
}

bool URLPattern::MatchesURL(const GURL& test) const {
  const GURL* test_url = &test;
  bool has_inner_url = test.inner_url() != NULL;

  if (has_inner_url) {
    if (!test.SchemeIsFileSystem())
      return false;  // The only nested URLs we handle are filesystem URLs.
    test_url = test.inner_url();
  }

  if (!MatchesScheme(test_url->scheme()))
    return false;

  if (match_all_urls_)
    return true;

  std::string path_for_request = test.PathForRequest();
  if (has_inner_url)
    path_for_request = test_url->path() + path_for_request;

  return MatchesSecurityOriginHelper(*test_url) &&
         MatchesPath(path_for_request);
}

bool URLPattern::MatchesSecurityOrigin(const GURL& test) const {
  const GURL* test_url = &test;
  bool has_inner_url = test.inner_url() != NULL;

  if (has_inner_url) {
    if (!test.SchemeIsFileSystem())
      return false;  // The only nested URLs we handle are filesystem URLs.
    test_url = test.inner_url();
  }

  if (!MatchesScheme(test_url->scheme()))
    return false;

  if (match_all_urls_)
    return true;

  return MatchesSecurityOriginHelper(*test_url);
}

bool URLPattern::MatchesScheme(const std::string& test) const {
  if (!IsValidScheme(test))
    return false;

  return scheme_ == "*" || test == scheme_;
}

bool URLPattern::MatchesHost(const std::string& host) const {
  std::string test(content::kHttpScheme);
  test += content::kStandardSchemeSeparator;
  test += host;
  test += "/";
  return MatchesHost(GURL(test));
}

bool URLPattern::MatchesHost(const GURL& test) const {
  // If the hosts are exactly equal, we have a match.
  if (test.host() == host_)
    return true;

  // If we're matching subdomains, and we have no host in the match pattern,
  // that means that we're matching all hosts, which means we have a match no
  // matter what the test host is.
  if (match_subdomains_ && host_.empty())
    return true;

  // Otherwise, we can only match if our match pattern matches subdomains.
  if (!match_subdomains_)
    return false;

  // We don't do subdomain matching against IP addresses, so we can give up now
  // if the test host is an IP address.
  if (test.HostIsIPAddress())
    return false;

  // Check if the test host is a subdomain of our host.
  if (test.host().length() <= (host_.length() + 1))
    return false;

  if (test.host().compare(test.host().length() - host_.length(),
                          host_.length(), host_) != 0)
    return false;

  return test.host()[test.host().length() - host_.length() - 1] == '.';
}

bool URLPattern::MatchesPath(const std::string& test) const {
  // Make the behaviour of OverlapsWith consistent with MatchesURL, which is
  // need to match hosted apps on e.g. 'google.com' also run on 'google.com/'.
  if (test + "/*" == path_escaped_)
    return true;

  return MatchPattern(test, path_escaped_);
}

const std::string& URLPattern::GetAsString() const {
  if (!spec_.empty())
    return spec_;

  if (match_all_urls_) {
    spec_ = kAllUrlsPattern;
    return spec_;
  }

  bool standard_scheme = IsStandardScheme(scheme_);

  std::string spec = scheme_ +
      (standard_scheme ? content::kStandardSchemeSeparator : ":");

  if (scheme_ != chrome::kFileScheme && standard_scheme) {
    if (match_subdomains_) {
      spec += "*";
      if (!host_.empty())
        spec += ".";
    }

    if (!host_.empty())
      spec += host_;

    if (port_ != "*") {
      spec += ":";
      spec += port_;
    }
  }

  if (!path_.empty())
    spec += path_;

  spec_ = spec;
  return spec_;
}

bool URLPattern::OverlapsWith(const URLPattern& other) const {
  if (match_all_urls() || other.match_all_urls())
    return true;
  return (MatchesAnyScheme(other.GetExplicitSchemes()) ||
          other.MatchesAnyScheme(GetExplicitSchemes()))
      && (MatchesHost(other.host()) || other.MatchesHost(host()))
      && (MatchesPortPattern(other.port()) || other.MatchesPortPattern(port()))
      && (MatchesPath(StripTrailingWildcard(other.path())) ||
          other.MatchesPath(StripTrailingWildcard(path())));
}

bool URLPattern::Contains(const URLPattern& other) const {
  if (match_all_urls())
    return true;
  return MatchesAllSchemes(other.GetExplicitSchemes())
      && MatchesHost(other.host())
      && MatchesPortPattern(other.port())
      && MatchesPath(StripTrailingWildcard(other.path()));
}

bool URLPattern::MatchesAnyScheme(
    const std::vector<std::string>& schemes) const {
  for (std::vector<std::string>::const_iterator i = schemes.begin();
       i != schemes.end(); ++i) {
    if (MatchesScheme(*i))
      return true;
  }

  return false;
}

bool URLPattern::MatchesAllSchemes(
    const std::vector<std::string>& schemes) const {
  for (std::vector<std::string>::const_iterator i = schemes.begin();
       i != schemes.end(); ++i) {
    if (!MatchesScheme(*i))
      return false;
  }

  return true;
}

bool URLPattern::MatchesSecurityOriginHelper(const GURL& test) const {
  // Ignore hostname if scheme is file://.
  if (scheme_ != chrome::kFileScheme && !MatchesHost(test))
    return false;

  if (!MatchesPortPattern(base::IntToString(test.EffectiveIntPort())))
    return false;

  return true;
}

bool URLPattern::MatchesPortPattern(const std::string& port) const {
  return port_ == "*" || port_ == port;
}

std::vector<std::string> URLPattern::GetExplicitSchemes() const {
  std::vector<std::string> result;

  if (scheme_ != "*" && !match_all_urls_ && IsValidScheme(scheme_)) {
    result.push_back(scheme_);
    return result;
  }

  for (size_t i = 0; i < arraysize(kValidSchemes); ++i) {
    if (MatchesScheme(kValidSchemes[i])) {
      result.push_back(kValidSchemes[i]);
    }
  }

  return result;
}

std::vector<URLPattern> URLPattern::ConvertToExplicitSchemes() const {
  std::vector<std::string> explicit_schemes = GetExplicitSchemes();
  std::vector<URLPattern> result;

  for (std::vector<std::string>::const_iterator i = explicit_schemes.begin();
       i != explicit_schemes.end(); ++i) {
    URLPattern temp = *this;
    temp.SetScheme(*i);
    temp.SetMatchAllURLs(false);
    result.push_back(temp);
  }

  return result;
}

// static
const char* URLPattern::GetParseResultString(
    URLPattern::ParseResult parse_result) {
  return kParseResultMessages[parse_result];
}
