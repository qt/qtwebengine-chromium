// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {

static const char kForceValueFalse[] = "false";
static const char kForceValueTrue[] = "true";
static const char kResponseTypeValueNone[] = "none";
static const char kResponseTypeValueToken[] = "token";

static const char kOAuth2IssueTokenBodyFormat[] =
    "force=%s"
    "&response_type=%s"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s";
static const char kIssueAdviceKey[] = "issueAdvice";
static const char kIssueAdviceValueAuto[] = "auto";
static const char kIssueAdviceValueConsent[] = "consent";
static const char kAccessTokenKey[] = "token";
static const char kConsentKey[] = "consent";
static const char kExpiresInKey[] = "expiresIn";
static const char kScopesKey[] = "scopes";
static const char kDescriptionKey[] = "description";
static const char kDetailKey[] = "detail";
static const char kDetailSeparators[] = "\n";
static const char kError[] = "error";
static const char kMessage[] = "message";

static GoogleServiceAuthError CreateAuthError(const net::URLFetcher* source) {
  URLRequestStatus status = source->GetStatus();
  if (status.status() == URLRequestStatus::CANCELED) {
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  }
  if (status.status() == URLRequestStatus::FAILED) {
    DLOG(WARNING) << "Server returned error: errno " << status.error();
    return GoogleServiceAuthError::FromConnectionError(status.error());
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  scoped_ptr<Value> value(base::JSONReader::Read(response_body));
  DictionaryValue* response;
  if (!value.get() || !value->GetAsDictionary(&response)) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        base::StringPrintf(
            "Not able to parse a JSON object from a service response. "
            "HTTP Status of the response is: %d", source->GetResponseCode()));
  }
  DictionaryValue* error;
  if (!response->GetDictionary(kError, &error)) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find a detailed error in a service response.");
  }
  std::string message;
  if (!error->GetString(kMessage, &message)) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an error message within a service error.");
  }
  return GoogleServiceAuthError::FromServiceError(message);
}

}  // namespace

IssueAdviceInfoEntry::IssueAdviceInfoEntry() {}
IssueAdviceInfoEntry::~IssueAdviceInfoEntry() {}

bool IssueAdviceInfoEntry::operator ==(const IssueAdviceInfoEntry& rhs) const {
  return description == rhs.description && details == rhs.details;
}

OAuth2MintTokenFlow::Parameters::Parameters() : mode(MODE_ISSUE_ADVICE) {}

OAuth2MintTokenFlow::Parameters::Parameters(
    const std::string& at,
    const std::string& eid,
    const std::string& cid,
    const std::vector<std::string>& scopes_arg,
    Mode mode_arg)
    : access_token(at),
      extension_id(eid),
      client_id(cid),
      scopes(scopes_arg),
      mode(mode_arg) {
}

OAuth2MintTokenFlow::Parameters::~Parameters() {}

OAuth2MintTokenFlow::OAuth2MintTokenFlow(URLRequestContextGetter* context,
                                         Delegate* delegate,
                                         const Parameters& parameters)
    : OAuth2ApiCallFlow(context,
                        std::string(),
                        parameters.access_token,
                        std::vector<std::string>()),
      delegate_(delegate),
      parameters_(parameters),
      weak_factory_(this) {}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::ReportSuccess(const std::string& access_token,
                                        int time_to_live) {
  if (delegate_)
    delegate_->OnMintTokenSuccess(access_token, time_to_live);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  if (delegate_)
    delegate_->OnIssueAdviceSuccess(issue_advice);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  if (delegate_)
    delegate_->OnMintTokenFailure(error);

  // |this| may already be deleted.
}

GURL OAuth2MintTokenFlow::CreateApiCallUrl() {
  return GaiaUrls::GetInstance()->oauth2_issue_token_url();
}

std::string OAuth2MintTokenFlow::CreateApiCallBody() {
  const char* force_value =
      (parameters_.mode == MODE_MINT_TOKEN_FORCE ||
       parameters_.mode == MODE_RECORD_GRANT)
          ? kForceValueTrue : kForceValueFalse;
  const char* response_type_value =
      (parameters_.mode == MODE_MINT_TOKEN_NO_FORCE ||
       parameters_.mode == MODE_MINT_TOKEN_FORCE)
          ? kResponseTypeValueToken : kResponseTypeValueNone;
  return base::StringPrintf(
      kOAuth2IssueTokenBodyFormat,
      net::EscapeUrlEncodedData(force_value, true).c_str(),
      net::EscapeUrlEncodedData(response_type_value, true).c_str(),
      net::EscapeUrlEncodedData(
          JoinString(parameters_.scopes, ' '), true).c_str(),
      net::EscapeUrlEncodedData(parameters_.client_id, true).c_str(),
      net::EscapeUrlEncodedData(parameters_.extension_id, true).c_str());
}

void OAuth2MintTokenFlow::ProcessApiCallSuccess(
    const net::URLFetcher* source) {
  std::string response_body;
  source->GetResponseAsString(&response_body);
  scoped_ptr<base::Value> value(base::JSONReader::Read(response_body));
  base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict)) {
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse a JSON object from a service response."));
    return;
  }

  std::string issue_advice_value;
  if (!dict->GetString(kIssueAdviceKey, &issue_advice_value)) {
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an issueAdvice in a service response."));
    return;
  }
  if (issue_advice_value == kIssueAdviceValueConsent) {
    IssueAdviceInfo issue_advice;
    if (ParseIssueAdviceResponse(dict, &issue_advice))
      ReportIssueAdviceSuccess(issue_advice);
    else
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of consent "
          "from a service response."));
  } else {
    std::string access_token;
    int time_to_live;
    if (ParseMintTokenResponse(dict, &access_token, &time_to_live))
      ReportSuccess(access_token, time_to_live);
    else
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of access token "
          "from a service response."));
  }

  // |this| may be deleted!
}

void OAuth2MintTokenFlow::ProcessApiCallFailure(
    const net::URLFetcher* source) {
  ReportFailure(CreateAuthError(source));
}
void OAuth2MintTokenFlow::ProcessNewAccessToken(
    const std::string& access_token) {
  // We don't currently store new access tokens. We generate one every time.
  // So we have nothing to do here.
  return;
}
void OAuth2MintTokenFlow::ProcessMintAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  ReportFailure(error);
}

// static
bool OAuth2MintTokenFlow::ParseMintTokenResponse(
    const base::DictionaryValue* dict, std::string* access_token,
    int* time_to_live) {
  CHECK(dict);
  CHECK(access_token);
  CHECK(time_to_live);
  std::string ttl_string;
  return dict->GetString(kExpiresInKey, &ttl_string) &&
      base::StringToInt(ttl_string, time_to_live) &&
      dict->GetString(kAccessTokenKey, access_token);
}

// static
bool OAuth2MintTokenFlow::ParseIssueAdviceResponse(
    const base::DictionaryValue* dict, IssueAdviceInfo* issue_advice) {
  CHECK(dict);
  CHECK(issue_advice);

  const base::DictionaryValue* consent_dict = NULL;
  if (!dict->GetDictionary(kConsentKey, &consent_dict))
    return false;

  const base::ListValue* scopes_list = NULL;
  if (!consent_dict->GetList(kScopesKey, &scopes_list))
    return false;

  bool success = true;
  for (size_t index = 0; index < scopes_list->GetSize(); ++index) {
    const base::DictionaryValue* scopes_entry = NULL;
    IssueAdviceInfoEntry entry;
    string16 detail;
    if (!scopes_list->GetDictionary(index, &scopes_entry) ||
        !scopes_entry->GetString(kDescriptionKey, &entry.description) ||
        !scopes_entry->GetString(kDetailKey, &detail)) {
      success = false;
      break;
    }

    TrimWhitespace(entry.description, TRIM_ALL, &entry.description);
    static const string16 detail_separators = ASCIIToUTF16(kDetailSeparators);
    Tokenize(detail, detail_separators, &entry.details);
    for (size_t i = 0; i < entry.details.size(); i++)
      TrimWhitespace(entry.details[i], TRIM_ALL, &entry.details[i]);
    issue_advice->push_back(entry);
  }

  if (!success)
    issue_advice->clear();

  return success;
}
