// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_error.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

using base::string16;
using base::DictionaryValue;

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// ExtensionError

// Static JSON keys.
const char ExtensionError::kExtensionIdKey[] = "extensionId";
const char ExtensionError::kFromIncognitoKey[] = "fromIncognito";
const char ExtensionError::kLevelKey[] = "level";
const char ExtensionError::kMessageKey[] = "message";
const char ExtensionError::kSourceKey[] = "source";
const char ExtensionError::kTypeKey[] = "type";

ExtensionError::ExtensionError(Type type,
                               const std::string& extension_id,
                               bool from_incognito,
                               logging::LogSeverity level,
                               const string16& source,
                               const string16& message)
    : type_(type),
      extension_id_(extension_id),
      from_incognito_(from_incognito),
      level_(level),
      source_(source),
      message_(message),
      occurrences_(1u) {
}

ExtensionError::~ExtensionError() {
}

scoped_ptr<DictionaryValue> ExtensionError::ToValue() const {
  // TODO(rdevlin.cronin): Use ValueBuilder when it's moved from
  // chrome/common/extensions.
  scoped_ptr<DictionaryValue> value(new DictionaryValue);
  value->SetInteger(kTypeKey, static_cast<int>(type_));
  value->SetString(kExtensionIdKey, extension_id_);
  value->SetBoolean(kFromIncognitoKey, from_incognito_);
  value->SetInteger(kLevelKey, static_cast<int>(level_));
  value->SetString(kSourceKey, source_);
  value->SetString(kMessageKey, message_);

  return value.Pass();
}

std::string ExtensionError::PrintForTest() const {
  return std::string("Extension Error:") +
         "\n  OTR:     " + std::string(from_incognito_ ? "true" : "false") +
         "\n  Level:   " + base::IntToString(static_cast<int>(level_)) +
         "\n  Source:  " + base::UTF16ToUTF8(source_) +
         "\n  Message: " + base::UTF16ToUTF8(message_) +
         "\n  ID:      " + extension_id_;
}

bool ExtensionError::IsEqual(const ExtensionError* rhs) const {
  // We don't check |source_| or |level_| here, since they are constant for
  // manifest errors. Check them in RuntimeError::IsEqualImpl() instead.
  return type_ == rhs->type_ &&
         extension_id_ == rhs->extension_id_ &&
         message_ == rhs->message_ &&
         IsEqualImpl(rhs);
}

////////////////////////////////////////////////////////////////////////////////
// ManifestError

// Static JSON keys.
const char ManifestError::kManifestKeyKey[] = "manifestKey";
const char ManifestError::kManifestSpecificKey[] = "manifestSpecific";

ManifestError::ManifestError(const std::string& extension_id,
                             const string16& message,
                             const string16& manifest_key,
                             const string16& manifest_specific)
    : ExtensionError(ExtensionError::MANIFEST_ERROR,
                     extension_id,
                     false,  // extensions can't be installed while incognito.
                     logging::LOG_WARNING,  // All manifest errors are warnings.
                     base::FilePath(kManifestFilename).AsUTF16Unsafe(),
                     message),
      manifest_key_(manifest_key),
      manifest_specific_(manifest_specific) {
}

ManifestError::~ManifestError() {
}

scoped_ptr<DictionaryValue> ManifestError::ToValue() const {
  scoped_ptr<DictionaryValue> value = ExtensionError::ToValue();
  if (!manifest_key_.empty())
    value->SetString(kManifestKeyKey, manifest_key_);
  if (!manifest_specific_.empty())
    value->SetString(kManifestSpecificKey, manifest_specific_);
  return value.Pass();
}

std::string ManifestError::PrintForTest() const {
  return ExtensionError::PrintForTest() +
         "\n  Type:    ManifestError";
}

bool ManifestError::IsEqualImpl(const ExtensionError* rhs) const {
  // If two manifest errors have the same extension id and message (which are
  // both checked in ExtensionError::IsEqual), then they are equal.
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// RuntimeError

// Static JSON keys.
const char RuntimeError::kColumnNumberKey[] = "columnNumber";
const char RuntimeError::kContextUrlKey[] = "contextUrl";
const char RuntimeError::kFunctionNameKey[] = "functionName";
const char RuntimeError::kLineNumberKey[] = "lineNumber";
const char RuntimeError::kStackTraceKey[] = "stackTrace";
const char RuntimeError::kUrlKey[] = "url";

RuntimeError::RuntimeError(const std::string& extension_id,
                           bool from_incognito,
                           const string16& source,
                           const string16& message,
                           const StackTrace& stack_trace,
                           const GURL& context_url,
                           logging::LogSeverity level)
    : ExtensionError(ExtensionError::RUNTIME_ERROR,
                     !extension_id.empty() ? extension_id : GURL(source).host(),
                     from_incognito,
                     level,
                     source,
                     message),
      context_url_(context_url),
      stack_trace_(stack_trace) {
  CleanUpInit();
}

RuntimeError::~RuntimeError() {
}

scoped_ptr<DictionaryValue> RuntimeError::ToValue() const {
  scoped_ptr<DictionaryValue> value = ExtensionError::ToValue();
  value->SetString(kContextUrlKey, context_url_.spec());

  ListValue* trace_value = new ListValue;
  for (StackTrace::const_iterator iter = stack_trace_.begin();
       iter != stack_trace_.end(); ++iter) {
    DictionaryValue* frame_value = new DictionaryValue;
    frame_value->SetInteger(kLineNumberKey, iter->line_number);
    frame_value->SetInteger(kColumnNumberKey, iter->column_number);
    frame_value->SetString(kUrlKey, iter->source);
    frame_value->SetString(kFunctionNameKey, iter->function);
    trace_value->Append(frame_value);
  }

  value->Set(kStackTraceKey, trace_value);

  return value.Pass();
}

std::string RuntimeError::PrintForTest() const {
  std::string result = ExtensionError::PrintForTest() +
         "\n  Type:    RuntimeError"
         "\n  Context: " + context_url_.spec() +
         "\n  Stack Trace: ";
  for (StackTrace::const_iterator iter = stack_trace_.begin();
       iter != stack_trace_.end(); ++iter) {
    result += "\n    {"
              "\n      Line:     " + base::IntToString(iter->line_number) +
              "\n      Column:   " + base::IntToString(iter->column_number) +
              "\n      URL:      " + base::UTF16ToUTF8(iter->source) +
              "\n      Function: " + base::UTF16ToUTF8(iter->function) +
              "\n    }";
  }
  return result;
}

bool RuntimeError::IsEqualImpl(const ExtensionError* rhs) const {
  const RuntimeError* error = static_cast<const RuntimeError*>(rhs);

  // Only look at the first frame of a stack trace to save time and group
  // nearly-identical errors. The most recent error is kept, so there's no risk
  // of displaying an old and inaccurate stack trace.
  return level_ == error->level_ &&
         source_ == error->source_ &&
         context_url_ == error->context_url_ &&
         stack_trace_.size() == error->stack_trace_.size() &&
         (stack_trace_.empty() || stack_trace_[0] == error->stack_trace_[0]);
}

void RuntimeError::CleanUpInit() {
  // If the error came from a generated background page, the "context" is empty
  // because there's no visible URL. We should set context to be the generated
  // background page in this case.
  GURL source_url = GURL(source_);
  if (context_url_.is_empty() &&
      source_url.path() ==
          std::string("/") + kGeneratedBackgroundPageFilename) {
    context_url_ = source_url;
  }

  // In some instances (due to the fact that we're reusing error reporting from
  // other systems), the source won't match up with the final entry in the stack
  // trace. (For instance, in a browser action error, the source is the page -
  // sometimes the background page - but the error is thrown from the script.)
  // Make the source match the stack trace, since that is more likely the cause
  // of the error.
  if (!stack_trace_.empty() && source_ != stack_trace_[0].source)
    source_ = stack_trace_[0].source;
}

}  // namespace extensions
