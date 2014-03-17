// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for header parsing were borrowed from Firefox:
// http://lxr.mozilla.org/seamonkey/source/netwerk/protocol/http/src/nsHttpResponseHead.cpp
// The rules for parsing content-types were also borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_response_headers.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"

using base::StringPiece;
using base::Time;
using base::TimeDelta;

namespace net {

//-----------------------------------------------------------------------------

namespace {

// These headers are RFC 2616 hop-by-hop headers;
// not to be stored by caches.
const char* const kHopByHopResponseHeaders[] = {
  "connection",
  "proxy-connection",
  "keep-alive",
  "trailer",
  "transfer-encoding",
  "upgrade"
};

// These headers are challenge response headers;
// not to be stored by caches.
const char* const kChallengeResponseHeaders[] = {
  "www-authenticate",
  "proxy-authenticate"
};

// These headers are cookie setting headers;
// not to be stored by caches or disclosed otherwise.
const char* const kCookieResponseHeaders[] = {
  "set-cookie",
  "set-cookie2"
};

// By default, do not cache Strict-Transport-Security or Public-Key-Pins.
// This avoids erroneously re-processing them on page loads from cache ---
// they are defined to be valid only on live and error-free HTTPS
// connections.
const char* const kSecurityStateHeaders[] = {
  "strict-transport-security",
  "public-key-pins"
};

// These response headers are not copied from a 304/206 response to the cached
// response headers.  This list is based on Mozilla's nsHttpResponseHead.cpp.
const char* const kNonUpdatedHeaders[] = {
  "connection",
  "proxy-connection",
  "keep-alive",
  "www-authenticate",
  "proxy-authenticate",
  "trailer",
  "transfer-encoding",
  "upgrade",
  "etag",
  "x-frame-options",
  "x-xss-protection",
};

// Some header prefixes mean "Don't copy this header from a 304 response.".
// Rather than listing all the relevant headers, we can consolidate them into
// this list:
const char* const kNonUpdatedHeaderPrefixes[] = {
  "content-",
  "x-content-",
  "x-webkit-"
};

bool ShouldUpdateHeader(const std::string::const_iterator& name_begin,
                        const std::string::const_iterator& name_end) {
  for (size_t i = 0; i < arraysize(kNonUpdatedHeaders); ++i) {
    if (LowerCaseEqualsASCII(name_begin, name_end, kNonUpdatedHeaders[i]))
      return false;
  }
  for (size_t i = 0; i < arraysize(kNonUpdatedHeaderPrefixes); ++i) {
    if (StartsWithASCII(std::string(name_begin, name_end),
                        kNonUpdatedHeaderPrefixes[i], false))
      return false;
  }
  return true;
}

void CheckDoesNotHaveEmbededNulls(const std::string& str) {
  // Care needs to be taken when adding values to the raw headers string to
  // make sure it does not contain embeded NULLs. Any embeded '\0' may be
  // understood as line terminators and change how header lines get tokenized.
  CHECK(str.find('\0') == std::string::npos);
}

bool ShouldShowHttpHeaderValue(const std::string& header_name) {
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  if (header_name == "Proxy-Authenticate")
    return false;
#endif
  return true;
}

}  // namespace

struct HttpResponseHeaders::ParsedHeader {
  // A header "continuation" contains only a subsequent value for the
  // preceding header.  (Header values are comma separated.)
  bool is_continuation() const { return name_begin == name_end; }

  std::string::const_iterator name_begin;
  std::string::const_iterator name_end;
  std::string::const_iterator value_begin;
  std::string::const_iterator value_end;
};

//-----------------------------------------------------------------------------

HttpResponseHeaders::HttpResponseHeaders(const std::string& raw_input)
    : response_code_(-1) {
  Parse(raw_input);

  // The most important thing to do with this histogram is find out
  // the existence of unusual HTTP status codes.  As it happens
  // right now, there aren't double-constructions of response headers
  // using this constructor, so our counts should also be accurate,
  // without instantiating the histogram in two places.  It is also
  // important that this histogram not collect data in the other
  // constructor, which rebuilds an histogram from a pickle, since
  // that would actually create a double call between the original
  // HttpResponseHeader that was serialized, and initialization of the
  // new object from that pickle.
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.HttpResponseCode",
                                   HttpUtil::MapStatusCodeForHistogram(
                                       response_code_),
                                   // Note the third argument is only
                                   // evaluated once, see macro
                                   // definition for details.
                                   HttpUtil::GetStatusCodesForHistogram());
}

HttpResponseHeaders::HttpResponseHeaders(const Pickle& pickle,
                                         PickleIterator* iter)
    : response_code_(-1) {
  std::string raw_input;
  if (pickle.ReadString(iter, &raw_input))
    Parse(raw_input);
}

void HttpResponseHeaders::Persist(Pickle* pickle, PersistOptions options) {
  if (options == PERSIST_RAW) {
    pickle->WriteString(raw_headers_);
    return;  // Done.
  }

  HeaderSet filter_headers;

  // Construct set of headers to filter out based on options.
  if ((options & PERSIST_SANS_NON_CACHEABLE) == PERSIST_SANS_NON_CACHEABLE)
    AddNonCacheableHeaders(&filter_headers);

  if ((options & PERSIST_SANS_COOKIES) == PERSIST_SANS_COOKIES)
    AddCookieHeaders(&filter_headers);

  if ((options & PERSIST_SANS_CHALLENGES) == PERSIST_SANS_CHALLENGES)
    AddChallengeHeaders(&filter_headers);

  if ((options & PERSIST_SANS_HOP_BY_HOP) == PERSIST_SANS_HOP_BY_HOP)
    AddHopByHopHeaders(&filter_headers);

  if ((options & PERSIST_SANS_RANGES) == PERSIST_SANS_RANGES)
    AddHopContentRangeHeaders(&filter_headers);

  if ((options & PERSIST_SANS_SECURITY_STATE) == PERSIST_SANS_SECURITY_STATE)
    AddSecurityStateHeaders(&filter_headers);

  std::string blob;
  blob.reserve(raw_headers_.size());

  // This copies the status line w/ terminator null.
  // Note raw_headers_ has embedded nulls instead of \n,
  // so this just copies the first header line.
  blob.assign(raw_headers_.c_str(), strlen(raw_headers_.c_str()) + 1);

  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    // Locate the start of the next header.
    size_t k = i;
    while (++k < parsed_.size() && parsed_[k].is_continuation()) {}
    --k;

    std::string header_name(parsed_[i].name_begin, parsed_[i].name_end);
    StringToLowerASCII(&header_name);

    if (filter_headers.find(header_name) == filter_headers.end()) {
      // Make sure there is a null after the value.
      blob.append(parsed_[i].name_begin, parsed_[k].value_end);
      blob.push_back('\0');
    }

    i = k;
  }
  blob.push_back('\0');

  pickle->WriteString(blob);
}

void HttpResponseHeaders::Update(const HttpResponseHeaders& new_headers) {
  DCHECK(new_headers.response_code() == 304 ||
         new_headers.response_code() == 206);

  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(raw_headers_.c_str());
  new_raw_headers.push_back('\0');

  HeaderSet updated_headers;

  // NOTE: we write the new headers then the old headers for convenience.  The
  // order should not matter.

  // Figure out which headers we want to take from new_headers:
  for (size_t i = 0; i < new_headers.parsed_.size(); ++i) {
    const HeaderList& new_parsed = new_headers.parsed_;

    DCHECK(!new_parsed[i].is_continuation());

    // Locate the start of the next header.
    size_t k = i;
    while (++k < new_parsed.size() && new_parsed[k].is_continuation()) {}
    --k;

    const std::string::const_iterator& name_begin = new_parsed[i].name_begin;
    const std::string::const_iterator& name_end = new_parsed[i].name_end;
    if (ShouldUpdateHeader(name_begin, name_end)) {
      std::string name(name_begin, name_end);
      StringToLowerASCII(&name);
      updated_headers.insert(name);

      // Preserve this header line in the merged result, making sure there is
      // a null after the value.
      new_raw_headers.append(name_begin, new_parsed[k].value_end);
      new_raw_headers.push_back('\0');
    }

    i = k;
  }

  // Now, build the new raw headers.
  MergeWithHeaders(new_raw_headers, updated_headers);
}

void HttpResponseHeaders::MergeWithHeaders(const std::string& raw_headers,
                                           const HeaderSet& headers_to_remove) {
  std::string new_raw_headers(raw_headers);
  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    // Locate the start of the next header.
    size_t k = i;
    while (++k < parsed_.size() && parsed_[k].is_continuation()) {}
    --k;

    std::string name(parsed_[i].name_begin, parsed_[i].name_end);
    StringToLowerASCII(&name);
    if (headers_to_remove.find(name) == headers_to_remove.end()) {
      // It's ok to preserve this header in the final result.
      new_raw_headers.append(parsed_[i].name_begin, parsed_[k].value_end);
      new_raw_headers.push_back('\0');
    }

    i = k;
  }
  new_raw_headers.push_back('\0');

  // Make this object hold the new data.
  raw_headers_.clear();
  parsed_.clear();
  Parse(new_raw_headers);
}

void HttpResponseHeaders::RemoveHeader(const std::string& name) {
  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(raw_headers_.c_str());
  new_raw_headers.push_back('\0');

  std::string lowercase_name(name);
  StringToLowerASCII(&lowercase_name);
  HeaderSet to_remove;
  to_remove.insert(lowercase_name);
  MergeWithHeaders(new_raw_headers, to_remove);
}

void HttpResponseHeaders::RemoveHeaderLine(const std::string& name,
                                           const std::string& value) {
  std::string name_lowercase(name);
  StringToLowerASCII(&name_lowercase);

  std::string new_raw_headers(GetStatusLine());
  new_raw_headers.push_back('\0');

  new_raw_headers.reserve(raw_headers_.size());

  void* iter = NULL;
  std::string old_header_name;
  std::string old_header_value;
  while (EnumerateHeaderLines(&iter, &old_header_name, &old_header_value)) {
    std::string old_header_name_lowercase(name);
    StringToLowerASCII(&old_header_name_lowercase);

    if (name_lowercase == old_header_name_lowercase &&
        value == old_header_value)
      continue;

    new_raw_headers.append(old_header_name);
    new_raw_headers.push_back(':');
    new_raw_headers.push_back(' ');
    new_raw_headers.append(old_header_value);
    new_raw_headers.push_back('\0');
  }
  new_raw_headers.push_back('\0');

  // Make this object hold the new data.
  raw_headers_.clear();
  parsed_.clear();
  Parse(new_raw_headers);
}

void HttpResponseHeaders::AddHeader(const std::string& header) {
  CheckDoesNotHaveEmbededNulls(header);
  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 2]);
  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 1]);
  // Don't copy the last null.
  std::string new_raw_headers(raw_headers_, 0, raw_headers_.size() - 1);
  new_raw_headers.append(header);
  new_raw_headers.push_back('\0');
  new_raw_headers.push_back('\0');

  // Make this object hold the new data.
  raw_headers_.clear();
  parsed_.clear();
  Parse(new_raw_headers);
}

void HttpResponseHeaders::ReplaceStatusLine(const std::string& new_status) {
  CheckDoesNotHaveEmbededNulls(new_status);
  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(new_status);
  new_raw_headers.push_back('\0');

  HeaderSet empty_to_remove;
  MergeWithHeaders(new_raw_headers, empty_to_remove);
}

void HttpResponseHeaders::Parse(const std::string& raw_input) {
  raw_headers_.reserve(raw_input.size());

  // ParseStatusLine adds a normalized status line to raw_headers_
  std::string::const_iterator line_begin = raw_input.begin();
  std::string::const_iterator line_end =
      std::find(line_begin, raw_input.end(), '\0');
  // has_headers = true, if there is any data following the status line.
  // Used by ParseStatusLine() to decide if a HTTP/0.9 is really a HTTP/1.0.
  bool has_headers = (line_end != raw_input.end() &&
                      (line_end + 1) != raw_input.end() &&
                      *(line_end + 1) != '\0');
  ParseStatusLine(line_begin, line_end, has_headers);
  raw_headers_.push_back('\0');  // Terminate status line with a null.

  if (line_end == raw_input.end()) {
    raw_headers_.push_back('\0');  // Ensure the headers end with a double null.

    DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 2]);
    DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 1]);
    return;
  }

  // Including a terminating null byte.
  size_t status_line_len = raw_headers_.size();

  // Now, we add the rest of the raw headers to raw_headers_, and begin parsing
  // it (to populate our parsed_ vector).
  raw_headers_.append(line_end + 1, raw_input.end());

  // Ensure the headers end with a double null.
  while (raw_headers_.size() < 2 ||
         raw_headers_[raw_headers_.size() - 2] != '\0' ||
         raw_headers_[raw_headers_.size() - 1] != '\0') {
    raw_headers_.push_back('\0');
  }

  // Adjust to point at the null byte following the status line
  line_end = raw_headers_.begin() + status_line_len - 1;

  HttpUtil::HeadersIterator headers(line_end + 1, raw_headers_.end(),
                                    std::string(1, '\0'));
  while (headers.GetNext()) {
    AddHeader(headers.name_begin(),
              headers.name_end(),
              headers.values_begin(),
              headers.values_end());
  }

  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 2]);
  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 1]);
}

// Append all of our headers to the final output string.
void HttpResponseHeaders::GetNormalizedHeaders(std::string* output) const {
  // copy up to the null byte.  this just copies the status line.
  output->assign(raw_headers_.c_str());

  // headers may appear multiple times (not necessarily in succession) in the
  // header data, so we build a map from header name to generated header lines.
  // to preserve the order of the original headers, the actual values are kept
  // in a separate list.  finally, the list of headers is flattened to form
  // the normalized block of headers.
  //
  // NOTE: We take special care to preserve the whitespace around any commas
  // that may occur in the original response headers.  Because our consumer may
  // be a web app, we cannot be certain of the semantics of commas despite the
  // fact that RFC 2616 says that they should be regarded as value separators.
  //
  typedef base::hash_map<std::string, size_t> HeadersMap;
  HeadersMap headers_map;
  HeadersMap::iterator iter = headers_map.end();

  std::vector<std::string> headers;

  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    std::string name(parsed_[i].name_begin, parsed_[i].name_end);
    std::string lower_name = StringToLowerASCII(name);

    iter = headers_map.find(lower_name);
    if (iter == headers_map.end()) {
      iter = headers_map.insert(
          HeadersMap::value_type(lower_name, headers.size())).first;
      headers.push_back(name + ": ");
    } else {
      headers[iter->second].append(", ");
    }

    std::string::const_iterator value_begin = parsed_[i].value_begin;
    std::string::const_iterator value_end = parsed_[i].value_end;
    while (++i < parsed_.size() && parsed_[i].is_continuation())
      value_end = parsed_[i].value_end;
    --i;

    headers[iter->second].append(value_begin, value_end);
  }

  for (size_t i = 0; i < headers.size(); ++i) {
    output->push_back('\n');
    output->append(headers[i]);
  }

  output->push_back('\n');
}

bool HttpResponseHeaders::GetNormalizedHeader(const std::string& name,
                                              std::string* value) const {
  // If you hit this assertion, please use EnumerateHeader instead!
  DCHECK(!HttpUtil::IsNonCoalescingHeader(name));

  value->clear();

  bool found = false;
  size_t i = 0;
  while (i < parsed_.size()) {
    i = FindHeader(i, name);
    if (i == std::string::npos)
      break;

    found = true;

    if (!value->empty())
      value->append(", ");

    std::string::const_iterator value_begin = parsed_[i].value_begin;
    std::string::const_iterator value_end = parsed_[i].value_end;
    while (++i < parsed_.size() && parsed_[i].is_continuation())
      value_end = parsed_[i].value_end;
    value->append(value_begin, value_end);
  }

  return found;
}

std::string HttpResponseHeaders::GetStatusLine() const {
  // copy up to the null byte.
  return std::string(raw_headers_.c_str());
}

std::string HttpResponseHeaders::GetStatusText() const {
  // GetStatusLine() is already normalized, so it has the format:
  // <http_version> SP <response_code> SP <status_text>
  std::string status_text = GetStatusLine();
  std::string::const_iterator begin = status_text.begin();
  std::string::const_iterator end = status_text.end();
  for (int i = 0; i < 2; ++i)
    begin = std::find(begin, end, ' ') + 1;
  return std::string(begin, end);
}

bool HttpResponseHeaders::EnumerateHeaderLines(void** iter,
                                               std::string* name,
                                               std::string* value) const {
  size_t i = reinterpret_cast<size_t>(*iter);
  if (i == parsed_.size())
    return false;

  DCHECK(!parsed_[i].is_continuation());

  name->assign(parsed_[i].name_begin, parsed_[i].name_end);

  std::string::const_iterator value_begin = parsed_[i].value_begin;
  std::string::const_iterator value_end = parsed_[i].value_end;
  while (++i < parsed_.size() && parsed_[i].is_continuation())
    value_end = parsed_[i].value_end;

  value->assign(value_begin, value_end);

  *iter = reinterpret_cast<void*>(i);
  return true;
}

bool HttpResponseHeaders::EnumerateHeader(void** iter,
                                          const base::StringPiece& name,
                                          std::string* value) const {
  size_t i;
  if (!iter || !*iter) {
    i = FindHeader(0, name);
  } else {
    i = reinterpret_cast<size_t>(*iter);
    if (i >= parsed_.size()) {
      i = std::string::npos;
    } else if (!parsed_[i].is_continuation()) {
      i = FindHeader(i, name);
    }
  }

  if (i == std::string::npos) {
    value->clear();
    return false;
  }

  if (iter)
    *iter = reinterpret_cast<void*>(i + 1);
  value->assign(parsed_[i].value_begin, parsed_[i].value_end);
  return true;
}

bool HttpResponseHeaders::HasHeaderValue(const base::StringPiece& name,
                                         const base::StringPiece& value) const {
  // The value has to be an exact match.  This is important since
  // 'cache-control: no-cache' != 'cache-control: no-cache="foo"'
  void* iter = NULL;
  std::string temp;
  while (EnumerateHeader(&iter, name, &temp)) {
    if (value.size() == temp.size() &&
        std::equal(temp.begin(), temp.end(), value.begin(),
                   base::CaseInsensitiveCompare<char>()))
      return true;
  }
  return false;
}

bool HttpResponseHeaders::HasHeader(const base::StringPiece& name) const {
  return FindHeader(0, name) != std::string::npos;
}

HttpResponseHeaders::HttpResponseHeaders() : response_code_(-1) {
}

HttpResponseHeaders::~HttpResponseHeaders() {
}

// Note: this implementation implicitly assumes that line_end points at a valid
// sentinel character (such as '\0').
// static
HttpVersion HttpResponseHeaders::ParseVersion(
    std::string::const_iterator line_begin,
    std::string::const_iterator line_end) {
  std::string::const_iterator p = line_begin;

  // RFC2616 sec 3.1: HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
  // TODO: (1*DIGIT apparently means one or more digits, but we only handle 1).
  // TODO: handle leading zeros, which is allowed by the rfc1616 sec 3.1.

  if ((line_end - p < 4) || !LowerCaseEqualsASCII(p, p + 4, "http")) {
    DVLOG(1) << "missing status line";
    return HttpVersion();
  }

  p += 4;

  if (p >= line_end || *p != '/') {
    DVLOG(1) << "missing version";
    return HttpVersion();
  }

  std::string::const_iterator dot = std::find(p, line_end, '.');
  if (dot == line_end) {
    DVLOG(1) << "malformed version";
    return HttpVersion();
  }

  ++p;  // from / to first digit.
  ++dot;  // from . to second digit.

  if (!(*p >= '0' && *p <= '9' && *dot >= '0' && *dot <= '9')) {
    DVLOG(1) << "malformed version number";
    return HttpVersion();
  }

  uint16 major = *p - '0';
  uint16 minor = *dot - '0';

  return HttpVersion(major, minor);
}

// Note: this implementation implicitly assumes that line_end points at a valid
// sentinel character (such as '\0').
void HttpResponseHeaders::ParseStatusLine(
    std::string::const_iterator line_begin,
    std::string::const_iterator line_end,
    bool has_headers) {
  // Extract the version number
  parsed_http_version_ = ParseVersion(line_begin, line_end);

  // Clamp the version number to one of: {0.9, 1.0, 1.1}
  if (parsed_http_version_ == HttpVersion(0, 9) && !has_headers) {
    http_version_ = HttpVersion(0, 9);
    raw_headers_ = "HTTP/0.9";
  } else if (parsed_http_version_ >= HttpVersion(1, 1)) {
    http_version_ = HttpVersion(1, 1);
    raw_headers_ = "HTTP/1.1";
  } else {
    // Treat everything else like HTTP 1.0
    http_version_ = HttpVersion(1, 0);
    raw_headers_ = "HTTP/1.0";
  }
  if (parsed_http_version_ != http_version_) {
    DVLOG(1) << "assuming HTTP/" << http_version_.major_value() << "."
             << http_version_.minor_value();
  }

  // TODO(eroman): this doesn't make sense if ParseVersion failed.
  std::string::const_iterator p = std::find(line_begin, line_end, ' ');

  if (p == line_end) {
    DVLOG(1) << "missing response status; assuming 200 OK";
    raw_headers_.append(" 200 OK");
    response_code_ = 200;
    return;
  }

  // Skip whitespace.
  while (*p == ' ')
    ++p;

  std::string::const_iterator code = p;
  while (*p >= '0' && *p <= '9')
    ++p;

  if (p == code) {
    DVLOG(1) << "missing response status number; assuming 200";
    raw_headers_.append(" 200 OK");
    response_code_ = 200;
    return;
  }
  raw_headers_.push_back(' ');
  raw_headers_.append(code, p);
  raw_headers_.push_back(' ');
  base::StringToInt(StringPiece(code, p), &response_code_);

  // Skip whitespace.
  while (*p == ' ')
    ++p;

  // Trim trailing whitespace.
  while (line_end > p && line_end[-1] == ' ')
    --line_end;

  if (p == line_end) {
    DVLOG(1) << "missing response status text; assuming OK";
    // Not super critical what we put here. Just use "OK"
    // even if it isn't descriptive of response_code_.
    raw_headers_.append("OK");
  } else {
    raw_headers_.append(p, line_end);
  }
}

size_t HttpResponseHeaders::FindHeader(size_t from,
                                       const base::StringPiece& search) const {
  for (size_t i = from; i < parsed_.size(); ++i) {
    if (parsed_[i].is_continuation())
      continue;
    const std::string::const_iterator& name_begin = parsed_[i].name_begin;
    const std::string::const_iterator& name_end = parsed_[i].name_end;
    if (static_cast<size_t>(name_end - name_begin) == search.size() &&
        std::equal(name_begin, name_end, search.begin(),
                   base::CaseInsensitiveCompare<char>()))
      return i;
  }

  return std::string::npos;
}

void HttpResponseHeaders::AddHeader(std::string::const_iterator name_begin,
                                    std::string::const_iterator name_end,
                                    std::string::const_iterator values_begin,
                                    std::string::const_iterator values_end) {
  // If the header can be coalesced, then we should split it up.
  if (values_begin == values_end ||
      HttpUtil::IsNonCoalescingHeader(name_begin, name_end)) {
    AddToParsed(name_begin, name_end, values_begin, values_end);
  } else {
    HttpUtil::ValuesIterator it(values_begin, values_end, ',');
    while (it.GetNext()) {
      AddToParsed(name_begin, name_end, it.value_begin(), it.value_end());
      // clobber these so that subsequent values are treated as continuations
      name_begin = name_end = raw_headers_.end();
    }
  }
}

void HttpResponseHeaders::AddToParsed(std::string::const_iterator name_begin,
                                      std::string::const_iterator name_end,
                                      std::string::const_iterator value_begin,
                                      std::string::const_iterator value_end) {
  ParsedHeader header;
  header.name_begin = name_begin;
  header.name_end = name_end;
  header.value_begin = value_begin;
  header.value_end = value_end;
  parsed_.push_back(header);
}

void HttpResponseHeaders::AddNonCacheableHeaders(HeaderSet* result) const {
  // Add server specified transients.  Any 'cache-control: no-cache="foo,bar"'
  // headers present in the response specify additional headers that we should
  // not store in the cache.
  const char kCacheControl[] = "cache-control";
  const char kPrefix[] = "no-cache=\"";
  const size_t kPrefixLen = sizeof(kPrefix) - 1;

  std::string value;
  void* iter = NULL;
  while (EnumerateHeader(&iter, kCacheControl, &value)) {
    // If the value is smaller than the prefix and a terminal quote, skip
    // it.
    if (value.size() <= kPrefixLen ||
        value.compare(0, kPrefixLen, kPrefix) != 0) {
      continue;
    }
    // if it doesn't end with a quote, then treat as malformed
    if (value[value.size()-1] != '\"')
      continue;

    // process the value as a comma-separated list of items. Each
    // item can be wrapped by linear white space.
    std::string::const_iterator item = value.begin() + kPrefixLen;
    std::string::const_iterator end = value.end() - 1;
    while (item != end) {
      // Find the comma to compute the length of the current item,
      // and the position of the next one.
      std::string::const_iterator item_next = std::find(item, end, ',');
      std::string::const_iterator item_end = end;
      if (item_next != end) {
        // Skip over comma for next position.
        item_end = item_next;
        item_next++;
      }
      // trim off leading and trailing whitespace in this item.
      HttpUtil::TrimLWS(&item, &item_end);

      // assuming the header is not empty, lowercase and insert into set
      if (item_end > item) {
        std::string name(&*item, item_end - item);
        StringToLowerASCII(&name);
        result->insert(name);
      }

      // Continue to next item.
      item = item_next;
    }
  }
}

void HttpResponseHeaders::AddHopByHopHeaders(HeaderSet* result) {
  for (size_t i = 0; i < arraysize(kHopByHopResponseHeaders); ++i)
    result->insert(std::string(kHopByHopResponseHeaders[i]));
}

void HttpResponseHeaders::AddCookieHeaders(HeaderSet* result) {
  for (size_t i = 0; i < arraysize(kCookieResponseHeaders); ++i)
    result->insert(std::string(kCookieResponseHeaders[i]));
}

void HttpResponseHeaders::AddChallengeHeaders(HeaderSet* result) {
  for (size_t i = 0; i < arraysize(kChallengeResponseHeaders); ++i)
    result->insert(std::string(kChallengeResponseHeaders[i]));
}

void HttpResponseHeaders::AddHopContentRangeHeaders(HeaderSet* result) {
  result->insert("content-range");
}

void HttpResponseHeaders::AddSecurityStateHeaders(HeaderSet* result) {
  for (size_t i = 0; i < arraysize(kSecurityStateHeaders); ++i)
    result->insert(std::string(kSecurityStateHeaders[i]));
}

void HttpResponseHeaders::GetMimeTypeAndCharset(std::string* mime_type,
                                                std::string* charset) const {
  mime_type->clear();
  charset->clear();

  std::string name = "content-type";
  std::string value;

  bool had_charset = false;

  void* iter = NULL;
  while (EnumerateHeader(&iter, name, &value))
    HttpUtil::ParseContentType(value, mime_type, charset, &had_charset, NULL);
}

bool HttpResponseHeaders::GetMimeType(std::string* mime_type) const {
  std::string unused;
  GetMimeTypeAndCharset(mime_type, &unused);
  return !mime_type->empty();
}

bool HttpResponseHeaders::GetCharset(std::string* charset) const {
  std::string unused;
  GetMimeTypeAndCharset(&unused, charset);
  return !charset->empty();
}

bool HttpResponseHeaders::IsRedirect(std::string* location) const {
  if (!IsRedirectResponseCode(response_code_))
    return false;

  // If we lack a Location header, then we can't treat this as a redirect.
  // We assume that the first non-empty location value is the target URL that
  // we want to follow.  TODO(darin): Is this consistent with other browsers?
  size_t i = std::string::npos;
  do {
    i = FindHeader(++i, "location");
    if (i == std::string::npos)
      return false;
    // If the location value is empty, then it doesn't count.
  } while (parsed_[i].value_begin == parsed_[i].value_end);

  if (location) {
    // Escape any non-ASCII characters to preserve them.  The server should
    // only be returning ASCII here, but for compat we need to do this.
    *location = EscapeNonASCII(
        std::string(parsed_[i].value_begin, parsed_[i].value_end));
  }

  return true;
}

// static
bool HttpResponseHeaders::IsRedirectResponseCode(int response_code) {
  // Users probably want to see 300 (multiple choice) pages, so we don't count
  // them as redirects that need to be followed.
  return (response_code == 301 ||
          response_code == 302 ||
          response_code == 303 ||
          response_code == 307);
}

// From RFC 2616 section 13.2.4:
//
// The calculation to determine if a response has expired is quite simple:
//
//   response_is_fresh = (freshness_lifetime > current_age)
//
// Of course, there are other factors that can force a response to always be
// validated or re-fetched.
//
bool HttpResponseHeaders::RequiresValidation(const Time& request_time,
                                             const Time& response_time,
                                             const Time& current_time) const {
  TimeDelta lifetime =
      GetFreshnessLifetime(response_time);
  if (lifetime == TimeDelta())
    return true;

  return lifetime <= GetCurrentAge(request_time, response_time, current_time);
}

// From RFC 2616 section 13.2.4:
//
// The max-age directive takes priority over Expires, so if max-age is present
// in a response, the calculation is simply:
//
//   freshness_lifetime = max_age_value
//
// Otherwise, if Expires is present in the response, the calculation is:
//
//   freshness_lifetime = expires_value - date_value
//
// Note that neither of these calculations is vulnerable to clock skew, since
// all of the information comes from the origin server.
//
// Also, if the response does have a Last-Modified time, the heuristic
// expiration value SHOULD be no more than some fraction of the interval since
// that time. A typical setting of this fraction might be 10%:
//
//   freshness_lifetime = (date_value - last_modified_value) * 0.10
//
TimeDelta HttpResponseHeaders::GetFreshnessLifetime(
    const Time& response_time) const {
  // Check for headers that force a response to never be fresh.  For backwards
  // compat, we treat "Pragma: no-cache" as a synonym for "Cache-Control:
  // no-cache" even though RFC 2616 does not specify it.
  if (HasHeaderValue("cache-control", "no-cache") ||
      HasHeaderValue("cache-control", "no-store") ||
      HasHeaderValue("pragma", "no-cache") ||
      HasHeaderValue("vary", "*"))  // see RFC 2616 section 13.6
    return TimeDelta();  // not fresh

  // NOTE: "Cache-Control: max-age" overrides Expires, so we only check the
  // Expires header after checking for max-age in GetFreshnessLifetime.  This
  // is important since "Expires: <date in the past>" means not fresh, but
  // it should not trump a max-age value.

  TimeDelta max_age_value;
  if (GetMaxAgeValue(&max_age_value))
    return max_age_value;

  // If there is no Date header, then assume that the server response was
  // generated at the time when we received the response.
  Time date_value;
  if (!GetDateValue(&date_value))
    date_value = response_time;

  Time expires_value;
  if (GetExpiresValue(&expires_value)) {
    // The expires value can be a date in the past!
    if (expires_value > date_value)
      return expires_value - date_value;

    return TimeDelta();  // not fresh
  }

  // From RFC 2616 section 13.4:
  //
  //   A response received with a status code of 200, 203, 206, 300, 301 or 410
  //   MAY be stored by a cache and used in reply to a subsequent request,
  //   subject to the expiration mechanism, unless a cache-control directive
  //   prohibits caching.
  //   ...
  //   A response received with any other status code (e.g. status codes 302
  //   and 307) MUST NOT be returned in a reply to a subsequent request unless
  //   there are cache-control directives or another header(s) that explicitly
  //   allow it.
  //
  // From RFC 2616 section 14.9.4:
  //
  //   When the must-revalidate directive is present in a response received by
  //   a cache, that cache MUST NOT use the entry after it becomes stale to
  //   respond to a subsequent request without first revalidating it with the
  //   origin server. (I.e., the cache MUST do an end-to-end revalidation every
  //   time, if, based solely on the origin server's Expires or max-age value,
  //   the cached response is stale.)
  //
  if ((response_code_ == 200 || response_code_ == 203 ||
       response_code_ == 206) &&
      !HasHeaderValue("cache-control", "must-revalidate")) {
    // TODO(darin): Implement a smarter heuristic.
    Time last_modified_value;
    if (GetLastModifiedValue(&last_modified_value)) {
      // The last-modified value can be a date in the past!
      if (last_modified_value <= date_value)
        return (date_value - last_modified_value) / 10;
    }
  }

  // These responses are implicitly fresh (unless otherwise overruled):
  if (response_code_ == 300 || response_code_ == 301 || response_code_ == 410)
    return TimeDelta::FromMicroseconds(kint64max);

  return TimeDelta();  // not fresh
}

// From RFC 2616 section 13.2.3:
//
// Summary of age calculation algorithm, when a cache receives a response:
//
//   /*
//    * age_value
//    *      is the value of Age: header received by the cache with
//    *              this response.
//    * date_value
//    *      is the value of the origin server's Date: header
//    * request_time
//    *      is the (local) time when the cache made the request
//    *              that resulted in this cached response
//    * response_time
//    *      is the (local) time when the cache received the
//    *              response
//    * now
//    *      is the current (local) time
//    */
//   apparent_age = max(0, response_time - date_value);
//   corrected_received_age = max(apparent_age, age_value);
//   response_delay = response_time - request_time;
//   corrected_initial_age = corrected_received_age + response_delay;
//   resident_time = now - response_time;
//   current_age   = corrected_initial_age + resident_time;
//
TimeDelta HttpResponseHeaders::GetCurrentAge(const Time& request_time,
                                             const Time& response_time,
                                             const Time& current_time) const {
  // If there is no Date header, then assume that the server response was
  // generated at the time when we received the response.
  Time date_value;
  if (!GetDateValue(&date_value))
    date_value = response_time;

  // If there is no Age header, then assume age is zero.  GetAgeValue does not
  // modify its out param if the value does not exist.
  TimeDelta age_value;
  GetAgeValue(&age_value);

  TimeDelta apparent_age = std::max(TimeDelta(), response_time - date_value);
  TimeDelta corrected_received_age = std::max(apparent_age, age_value);
  TimeDelta response_delay = response_time - request_time;
  TimeDelta corrected_initial_age = corrected_received_age + response_delay;
  TimeDelta resident_time = current_time - response_time;
  TimeDelta current_age = corrected_initial_age + resident_time;

  return current_age;
}

bool HttpResponseHeaders::GetMaxAgeValue(TimeDelta* result) const {
  std::string name = "cache-control";
  std::string value;

  const char kMaxAgePrefix[] = "max-age=";
  const size_t kMaxAgePrefixLen = arraysize(kMaxAgePrefix) - 1;

  void* iter = NULL;
  while (EnumerateHeader(&iter, name, &value)) {
    if (value.size() > kMaxAgePrefixLen) {
      if (LowerCaseEqualsASCII(value.begin(),
                               value.begin() + kMaxAgePrefixLen,
                               kMaxAgePrefix)) {
        int64 seconds;
        base::StringToInt64(StringPiece(value.begin() + kMaxAgePrefixLen,
                                        value.end()),
                            &seconds);
        *result = TimeDelta::FromSeconds(seconds);
        return true;
      }
    }
  }

  return false;
}

bool HttpResponseHeaders::GetAgeValue(TimeDelta* result) const {
  std::string value;
  if (!EnumerateHeader(NULL, "Age", &value))
    return false;

  int64 seconds;
  base::StringToInt64(value, &seconds);
  *result = TimeDelta::FromSeconds(seconds);
  return true;
}

bool HttpResponseHeaders::GetDateValue(Time* result) const {
  return GetTimeValuedHeader("Date", result);
}

bool HttpResponseHeaders::GetLastModifiedValue(Time* result) const {
  return GetTimeValuedHeader("Last-Modified", result);
}

bool HttpResponseHeaders::GetExpiresValue(Time* result) const {
  return GetTimeValuedHeader("Expires", result);
}

bool HttpResponseHeaders::GetTimeValuedHeader(const std::string& name,
                                              Time* result) const {
  std::string value;
  if (!EnumerateHeader(NULL, name, &value))
    return false;

  // When parsing HTTP dates it's beneficial to default to GMT because:
  // 1. RFC2616 3.3.1 says times should always be specified in GMT
  // 2. Only counter-example incorrectly appended "UTC" (crbug.com/153759)
  // 3. When adjusting cookie expiration times for clock skew
  //    (crbug.com/135131) this better matches our cookie expiration
  //    time parser which ignores timezone specifiers and assumes GMT.
  // 4. This is exactly what Firefox does.
  // TODO(pauljensen): The ideal solution would be to return false if the
  // timezone could not be understood so as to avoid makeing other calculations
  // based on an incorrect time.  This would require modifying the time
  // library or duplicating the code. (http://crbug.com/158327)
  return Time::FromUTCString(value.c_str(), result);
}

bool HttpResponseHeaders::IsKeepAlive() const {
  if (http_version_ < HttpVersion(1, 0))
    return false;

  // NOTE: It is perhaps risky to assume that a Proxy-Connection header is
  // meaningful when we don't know that this response was from a proxy, but
  // Mozilla also does this, so we'll do the same.
  std::string connection_val;
  if (!EnumerateHeader(NULL, "connection", &connection_val))
    EnumerateHeader(NULL, "proxy-connection", &connection_val);

  bool keep_alive;

  if (http_version_ == HttpVersion(1, 0)) {
    // HTTP/1.0 responses default to NOT keep-alive
    keep_alive = LowerCaseEqualsASCII(connection_val, "keep-alive");
  } else {
    // HTTP/1.1 responses default to keep-alive
    keep_alive = !LowerCaseEqualsASCII(connection_val, "close");
  }

  return keep_alive;
}

bool HttpResponseHeaders::HasStrongValidators() const {
  std::string etag_header;
  EnumerateHeader(NULL, "etag", &etag_header);
  std::string last_modified_header;
  EnumerateHeader(NULL, "Last-Modified", &last_modified_header);
  std::string date_header;
  EnumerateHeader(NULL, "Date", &date_header);
  return HttpUtil::HasStrongValidators(GetHttpVersion(),
                                       etag_header,
                                       last_modified_header,
                                       date_header);
}

// From RFC 2616:
// Content-Length = "Content-Length" ":" 1*DIGIT
int64 HttpResponseHeaders::GetContentLength() const {
  return GetInt64HeaderValue("content-length");
}

int64 HttpResponseHeaders::GetInt64HeaderValue(
    const std::string& header) const {
  void* iter = NULL;
  std::string content_length_val;
  if (!EnumerateHeader(&iter, header, &content_length_val))
    return -1;

  if (content_length_val.empty())
    return -1;

  if (content_length_val[0] == '+')
    return -1;

  int64 result;
  bool ok = base::StringToInt64(content_length_val, &result);
  if (!ok || result < 0)
    return -1;

  return result;
}

// From RFC 2616 14.16:
// content-range-spec =
//     bytes-unit SP byte-range-resp-spec "/" ( instance-length | "*" )
// byte-range-resp-spec = (first-byte-pos "-" last-byte-pos) | "*"
// instance-length = 1*DIGIT
// bytes-unit = "bytes"
bool HttpResponseHeaders::GetContentRange(int64* first_byte_position,
                                          int64* last_byte_position,
                                          int64* instance_length) const {
  void* iter = NULL;
  std::string content_range_spec;
  *first_byte_position = *last_byte_position = *instance_length = -1;
  if (!EnumerateHeader(&iter, "content-range", &content_range_spec))
    return false;

  // If the header value is empty, we have an invalid header.
  if (content_range_spec.empty())
    return false;

  size_t space_position = content_range_spec.find(' ');
  if (space_position == std::string::npos)
    return false;

  // Invalid header if it doesn't contain "bytes-unit".
  std::string::const_iterator content_range_spec_begin =
      content_range_spec.begin();
  std::string::const_iterator content_range_spec_end =
      content_range_spec.begin() + space_position;
  HttpUtil::TrimLWS(&content_range_spec_begin, &content_range_spec_end);
  if (!LowerCaseEqualsASCII(content_range_spec_begin,
                            content_range_spec_end,
                            "bytes")) {
    return false;
  }

  size_t slash_position = content_range_spec.find('/', space_position + 1);
  if (slash_position == std::string::npos)
    return false;

  // Obtain the part behind the space and before slash.
  std::string::const_iterator byte_range_resp_spec_begin =
      content_range_spec.begin() + space_position + 1;
  std::string::const_iterator byte_range_resp_spec_end =
      content_range_spec.begin() + slash_position;
  HttpUtil::TrimLWS(&byte_range_resp_spec_begin, &byte_range_resp_spec_end);

  // Parse the byte-range-resp-spec part.
  std::string byte_range_resp_spec(byte_range_resp_spec_begin,
                                   byte_range_resp_spec_end);
  // If byte-range-resp-spec != "*".
  if (!LowerCaseEqualsASCII(byte_range_resp_spec, "*")) {
    size_t minus_position = byte_range_resp_spec.find('-');
    if (minus_position != std::string::npos) {
      // Obtain first-byte-pos.
      std::string::const_iterator first_byte_pos_begin =
          byte_range_resp_spec.begin();
      std::string::const_iterator first_byte_pos_end =
          byte_range_resp_spec.begin() + minus_position;
      HttpUtil::TrimLWS(&first_byte_pos_begin, &first_byte_pos_end);

      bool ok = base::StringToInt64(StringPiece(first_byte_pos_begin,
                                                first_byte_pos_end),
                                    first_byte_position);

      // Obtain last-byte-pos.
      std::string::const_iterator last_byte_pos_begin =
          byte_range_resp_spec.begin() + minus_position + 1;
      std::string::const_iterator last_byte_pos_end =
          byte_range_resp_spec.end();
      HttpUtil::TrimLWS(&last_byte_pos_begin, &last_byte_pos_end);

      ok &= base::StringToInt64(StringPiece(last_byte_pos_begin,
                                            last_byte_pos_end),
                                last_byte_position);
      if (!ok) {
        *first_byte_position = *last_byte_position = -1;
        return false;
      }
      if (*first_byte_position < 0 || *last_byte_position < 0 ||
          *first_byte_position > *last_byte_position)
        return false;
    } else {
      return false;
    }
  }

  // Parse the instance-length part.
  // If instance-length == "*".
  std::string::const_iterator instance_length_begin =
      content_range_spec.begin() + slash_position + 1;
  std::string::const_iterator instance_length_end =
      content_range_spec.end();
  HttpUtil::TrimLWS(&instance_length_begin, &instance_length_end);

  if (LowerCaseEqualsASCII(instance_length_begin, instance_length_end, "*")) {
    return false;
  } else if (!base::StringToInt64(StringPiece(instance_length_begin,
                                              instance_length_end),
                                  instance_length)) {
    *instance_length = -1;
    return false;
  }

  // We have all the values; let's verify that they make sense for a 206
  // response.
  if (*first_byte_position < 0 || *last_byte_position < 0 ||
      *instance_length < 0 || *instance_length - 1 < *last_byte_position)
    return false;

  return true;
}

base::Value* HttpResponseHeaders::NetLogCallback(
    NetLog::LogLevel /* log_level */) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::ListValue* headers = new base::ListValue();
  headers->Append(new base::StringValue(GetStatusLine()));
  void* iterator = NULL;
  std::string name;
  std::string value;
  while (EnumerateHeaderLines(&iterator, &name, &value)) {
    headers->Append(
      new base::StringValue(
          base::StringPrintf("%s: %s",
                             name.c_str(),
                             (ShouldShowHttpHeaderValue(name) ?
                                 value.c_str() : "[elided]"))));
  }
  dict->Set("headers", headers);
  return dict;
}

// static
bool HttpResponseHeaders::FromNetLogParam(
    const base::Value* event_param,
    scoped_refptr<HttpResponseHeaders>* http_response_headers) {
  *http_response_headers = NULL;

  const base::DictionaryValue* dict = NULL;
  const base::ListValue* header_list = NULL;

  if (!event_param ||
      !event_param->GetAsDictionary(&dict) ||
      !dict->GetList("headers", &header_list)) {
    return false;
  }

  std::string raw_headers;
  for (base::ListValue::const_iterator it = header_list->begin();
       it != header_list->end();
       ++it) {
    std::string header_line;
    if (!(*it)->GetAsString(&header_line))
      return false;

    raw_headers.append(header_line);
    raw_headers.push_back('\0');
  }
  raw_headers.push_back('\0');
  *http_response_headers = new HttpResponseHeaders(raw_headers);
  return true;
}

bool HttpResponseHeaders::IsChunkEncoded() const {
  // Ignore spurious chunked responses from HTTP/1.0 servers and proxies.
  return GetHttpVersion() >= HttpVersion(1, 1) &&
      HasHeaderValue("Transfer-Encoding", "chunked");
}

#if defined(SPDY_PROXY_AUTH_ORIGIN)
bool HttpResponseHeaders::GetChromeProxyBypassDuration(
    const std::string& action_prefix,
    base::TimeDelta* duration) const {
  void* iter = NULL;
  std::string value;
  std::string name = "chrome-proxy";

  while (EnumerateHeader(&iter, name, &value)) {
    if (value.size() > action_prefix.size()) {
      if (LowerCaseEqualsASCII(value.begin(),
                               value.begin() + action_prefix.size(),
                               action_prefix.c_str())) {
        int64 seconds;
        if (!base::StringToInt64(
                StringPiece(value.begin() + action_prefix.size(), value.end()),
                &seconds) || seconds < 0) {
          continue;  // In case there is a well formed instruction.
        }
        *duration = TimeDelta::FromSeconds(seconds);
        return true;
      }
    }
  }
  return false;
}

bool HttpResponseHeaders::GetChromeProxyInfo(
    ChromeProxyInfo* proxy_info) const {
  DCHECK(proxy_info);
  proxy_info->bypass_all = false;
  proxy_info->bypass_duration = base::TimeDelta();

  // Support header of the form Chrome-Proxy: bypass|block=<duration>, where
  // <duration> is the number of seconds to wait before retrying
  // the proxy. If the duration is 0, then the default proxy retry delay
  // (specified in |ProxyList::UpdateRetryInfoOnFallback|) will be used.
  // 'bypass' instructs Chrome to bypass the currently connected Chrome proxy,
  // whereas 'block' instructs Chrome to bypass all available Chrome proxies.

  // 'block' takes precedence over 'bypass', so look for it first.
  // TODO(bengr): Reduce checks for 'block' and 'bypass' to a single loop.
  if (GetChromeProxyBypassDuration("block=", &proxy_info->bypass_duration)) {
    proxy_info->bypass_all = true;
    return true;
  }

  // Next, look for 'bypass'.
  if (GetChromeProxyBypassDuration("bypass=", &proxy_info->bypass_duration))
    return true;

  return false;
}
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

}  // namespace net
