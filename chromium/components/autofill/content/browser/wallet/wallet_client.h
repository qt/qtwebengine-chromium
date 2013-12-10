// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_CLIENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_CLIENT_H_

#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"  // For base::Closure.
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/core/browser/autofill_manager_delegate.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {
namespace wallet {

class Address;
class FullWallet;
class Instrument;
class WalletClientDelegate;

// WalletClient is responsible for making calls to the Online Wallet backend on
// the user's behalf. The normal flow for using this class is as follows:
// 1) GetWalletItems should be called to retrieve the user's Wallet.
//   a) If the user does not have a Wallet, they must AcceptLegalDocuments and
//      SaveToWallet to set up their account before continuing.
//   b) If the user has not accepted the most recent legal documents for
//      Wallet, they must AcceptLegalDocuments.
// 2) The user then chooses what instrument and shipping address to use for the
//    current transaction.
//   a) If they choose an instrument with a zip code only address, the billing
//      address will need to be updated using SaveToWallet.
//   b) The user may also choose to add a new instrument or address using
//      SaveToWallet.
// 3) Once the user has selected the backing instrument and shipping address
//    for this transaction, a FullWallet with the fronting card is generated
//    using GetFullWallet.
//   a) GetFullWallet may return a Risk challenge for the user. In that case,
//      the user will need to verify who they are by authenticating their
//      chosen backing instrument through AuthenticateInstrument
//
// WalletClient is designed so only one request to Online Wallet can be outgoing
// at any one time. If |HasRequestInProgress()| is true while calling e.g.
// GetWalletItems(), the request will be queued and started later. Queued
// requests start in the order they were received.

class WalletClient : public net::URLFetcherDelegate {
 public:
  // The Risk challenges supported by users of WalletClient.
  enum RiskCapability {
    RELOGIN,
    VERIFY_CVC,
  };

  // The type of error returned by Online Wallet.
  enum ErrorType {
    // Errors to display to users.
    BUYER_ACCOUNT_ERROR,                // Risk deny, unsupported country, or
                                        // account closed.
    BUYER_LEGAL_ADDRESS_NOT_SUPPORTED,  // User's Buyer Legal Address is
                                        // unsupported by Online Wallet.
    UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS,  // User's "know your customer" KYC
                                           // state is not verified (either
                                           // KYC_REFER or KYC_FAIL).
    UNSUPPORTED_MERCHANT,               // Merchant is blacklisted due to
                                        // compliance violation.

    // API errors.
    BAD_REQUEST,              // Request was very malformed or sent to the
                              // wrong endpoint.
    INVALID_PARAMS,           // API call had missing or invalid parameters.
    UNSUPPORTED_API_VERSION,  // The server API version of the request is no
                              // longer supported.

    // Server errors.
    INTERNAL_ERROR,           // Unknown server side error.
    SERVICE_UNAVAILABLE,      // Online Wallet is down.

    // Other errors.
    MALFORMED_RESPONSE,       // The response from Wallet was malformed.
    NETWORK_ERROR,            // The response code of the server was something
                              // other than a 200 or 400.

    UNKNOWN_ERROR,            // Catch all error type.
  };

  struct FullWalletRequest {
   public:
    FullWalletRequest(const std::string& instrument_id,
                      const std::string& address_id,
                      const GURL& source_url,
                      const std::string& google_transaction_id,
                      const std::vector<RiskCapability> risk_capabilities,
                      bool new_wallet_user);
    ~FullWalletRequest();

    // The ID of the backing instrument. Should have been selected by the user
    // in some UI.
    std::string instrument_id;

    // The ID of the shipping address. Should have been selected by the user
    // in some UI.
    std::string address_id;

    // The URL that Online Wallet usage is being initiated on.
    GURL source_url;

    // The transaction ID from GetWalletItems.
    std::string google_transaction_id;

    // The Risk challenges supported by the user of WalletClient
    std::vector<RiskCapability> risk_capabilities;

    // True if the user does not have Wallet profile.
    bool new_wallet_user;

   private:
    DISALLOW_ASSIGN(FullWalletRequest);
  };

  // |context_getter| is reference counted so it has no lifetime or ownership
  // requirements. |delegate| must outlive |this|.
  WalletClient(net::URLRequestContextGetter* context_getter,
               WalletClientDelegate* delegate);

  virtual ~WalletClient();

  // GetWalletItems retrieves the user's online wallet. The WalletItems
  // returned may require additional action such as presenting legal documents
  // to the user to be accepted.
  virtual void GetWalletItems(const GURL& source_url);

  // The GetWalletItems call to the Online Wallet backend may require the user
  // to accept various legal documents before a FullWallet can be generated.
  // The |google_transaction_id| is provided in the response to the
  // GetWalletItems call. If |documents| are empty, |delegate_| will not receive
  // a corresponding |OnDidAcceptLegalDocuments()| call.
  virtual void AcceptLegalDocuments(
      const std::vector<WalletItems::LegalDocument*>& documents,
      const std::string& google_transaction_id,
      const GURL& source_url);

  // Authenticates that |card_verification_number| is for the backing instrument
  // with |instrument_id|. |obfuscated_gaia_id| is used as a key when escrowing
  // |card_verification_number|. |delegate_| is notified when the request is
  // complete. Used to respond to Risk challenges.
  virtual void AuthenticateInstrument(
      const std::string& instrument_id,
      const std::string& card_verification_number);

  // GetFullWallet retrieves the a FullWallet for the user.
  virtual void GetFullWallet(const FullWalletRequest& full_wallet_request);

  // Saves the data in |instrument| and/or |address| to Wallet. |instrument|
  // does not have to be complete if its being used to update an existing
  // instrument, like in the case of expiration date or address only updates.
  virtual void SaveToWallet(scoped_ptr<Instrument> instrument,
                            scoped_ptr<Address> address,
                            const GURL& source_url);

  bool HasRequestInProgress() const;

  // Cancels and clears the current |request_| and |pending_requests_| (if any).
  void CancelRequests();

 private:
  FRIEND_TEST_ALL_PREFIXES(WalletClientTest, PendingRequest);
  FRIEND_TEST_ALL_PREFIXES(WalletClientTest, CancelRequests);

  enum RequestType {
    NO_PENDING_REQUEST,
    ACCEPT_LEGAL_DOCUMENTS,
    AUTHENTICATE_INSTRUMENT,
    GET_FULL_WALLET,
    GET_WALLET_ITEMS,
    SAVE_TO_WALLET,
  };

  // Like AcceptLegalDocuments, but takes a vector of document ids.
  void DoAcceptLegalDocuments(
      const std::vector<std::string>& document_ids,
      const std::string& google_transaction_id,
      const GURL& source_url);

  // Posts |post_body| to |url| with content type |mime_type| and notifies
  // |delegate_| when the request is complete.
  void MakeWalletRequest(const GURL& url,
                         const std::string& post_body,
                         const std::string& mime_type);

  // Performs bookkeeping tasks for any invalid requests.
  void HandleMalformedResponse(RequestType request_type,
                               net::URLFetcher* request);
  void HandleNetworkError(int response_code);
  void HandleWalletError(ErrorType error_type);

  // Start the next pending request (if any).
  void StartNextPendingRequest();

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Logs an UMA metric for each of the |required_actions|.
  void LogRequiredActions(
      const std::vector<RequiredAction>& required_actions) const;

  // Converts |request_type| to an UMA metric.
  AutofillMetrics::WalletApiCallMetric RequestTypeToUmaMetric(
      RequestType request_type) const;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  WalletClientDelegate* const delegate_;  // must outlive |this|.

  // The current request object.
  scoped_ptr<net::URLFetcher> request_;

  // The type of the current request. Must be NO_PENDING_REQUEST for a request
  // to be initiated as only one request may be running at a given time.
  RequestType request_type_;

  // The one time pad used for GetFullWallet encryption.
  std::vector<uint8> one_time_pad_;

  // Requests that are waiting to be run.
  std::queue<base::Closure> pending_requests_;

  // When the current request started. Used to track client side latency.
  base::Time request_started_timestamp_;

  base::WeakPtrFactory<WalletClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_CLIENT_H_
