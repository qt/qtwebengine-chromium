// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_STDSTRING_H_
#define URL_URL_CANON_STDSTRING_H_

// This header file defines a canonicalizer output method class for STL
// strings. Because the canonicalizer tries not to be dependent on the STL,
// we have segregated it here.

#include <string>

#include "url/url_canon.h"

namespace url_canon {

// Write into a std::string given in the constructor. This object does not own
// the string itself, and the user must ensure that the string stays alive
// throughout the lifetime of this object.
//
// The given string will be appended to; any existing data in the string will
// be preserved. The caller should reserve() the amount of data in the string
// they expect to be written. We will resize if necessary, but that's slow.
//
// Note that when canonicalization is complete, the string will likely have
// unused space at the end because we make the string very big to start out
// with (by |initial_size|). This ends up being important because resize
// operations are slow, and because the base class needs to write directly
// into the buffer.
//
// Therefore, the user should call Complete() before using the string that
// this class wrote into.
class StdStringCanonOutput : public CanonOutput {
 public:
  StdStringCanonOutput(std::string* str)
      : CanonOutput(),
        str_(str) {
    cur_len_ = static_cast<int>(str_->size());  // Append to existing data.
    str_->resize(str_->capacity());
    buffer_ = str_->empty() ? NULL : &(*str_)[0];
    buffer_len_ = static_cast<int>(str_->size());
  }
  virtual ~StdStringCanonOutput() {
    // Nothing to do, we don't own the string.
  }

  // Must be called after writing has completed but before the string is used.
  void Complete() {
    str_->resize(cur_len_);
    buffer_len_ = cur_len_;
  }

  virtual void Resize(int sz) {
    str_->resize(sz);
    buffer_ = str_->empty() ? NULL : &(*str_)[0];
    buffer_len_ = sz;
  }

 protected:
  std::string* str_;
};

// An extension of the Replacements class that allows the setters to use
// standard strings.
//
// The strings passed as arguments are not copied and must remain valid until
// this class goes out of scope.
template<typename STR>
class StdStringReplacements :
    public url_canon::Replacements<typename STR::value_type> {
 public:
  void SetSchemeStr(const STR& s) {
    this->SetScheme(s.data(),
                    url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetUsernameStr(const STR& s) {
    this->SetUsername(s.data(),
                      url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetPasswordStr(const STR& s) {
    this->SetPassword(s.data(),
                      url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetHostStr(const STR& s) {
    this->SetHost(s.data(),
                  url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetPortStr(const STR& s) {
    this->SetPort(s.data(),
                  url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetPathStr(const STR& s) {
    this->SetPath(s.data(),
                  url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetQueryStr(const STR& s) {
    this->SetQuery(s.data(),
                   url_parse::Component(0, static_cast<int>(s.length())));
  }
  void SetRefStr(const STR& s) {
    this->SetRef(s.data(),
                 url_parse::Component(0, static_cast<int>(s.length())));
  }
};

}  // namespace url_canon

#endif  // URL_URL_CANON_STDSTRING_H_
