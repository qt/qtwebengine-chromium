// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_ADDRESS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/phone_number_i18n.h"

namespace base {
class DictionaryValue;
}

namespace autofill {

class AutofillProfile;
class AutofillType;

namespace wallet {

// TODO(ahutter): This address is a lot like
// components/autofill/core/browser/address.h.  There should be a super
// class that both extend from to clean up duplicated code. See
// http://crbug.com/164463.

// Address contains various address fields that have been populated from the
// user's Online Wallet. It is loosely modeled as a subet of the OASIS
// "extensible Address Language" (xAL); see
// http://www.oasis-open.org/committees/ciq/download.shtml.
class Address {
 public:
  // TODO(ahutter): Use additional fields (descriptive_name, is_post_box,
  // is_valid, is_default) when SaveToWallet is implemented.
  // See http://crbug.com/164284.

  Address();

  // Using the raw info in |profile|, create a wallet::Address.
  explicit Address(const AutofillProfile& profile);

  Address(const std::string& country_name_code,
          const base::string16& recipient_name,
          const base::string16& address_line_1,
          const base::string16& address_line_2,
          const base::string16& locality_name,
          const base::string16& administrative_area_name,
          const base::string16& postal_code_number,
          const base::string16& phone_number,
          const std::string& object_id);

  ~Address();

  // Returns an empty scoped_ptr if input is invalid or a valid address that is
  // selectable for Google Wallet use. Does not require "id" in |dictionary|.
  // IDs are not required for billing addresses.
  static scoped_ptr<Address> CreateAddress(
      const base::DictionaryValue& dictionary);

  // TODO(ahutter): Make obvious in the function name that this public method
  // only works for shipping address and assumes existance of "postal_address".
  // Builds an Address from |dictionary|, which must have an "id" field. This
  // function is designed for use with shipping addresses. The function may fail
  // and return an empty pointer if its input is invalid.
  static scoped_ptr<Address> CreateAddressWithID(
      const base::DictionaryValue& dictionary);

  // Returns an empty scoped_ptr if input in invalid or a valid address that
  // can only be used for displaying to the user.
  static scoped_ptr<Address> CreateDisplayAddress(
      const base::DictionaryValue& dictionary);

  // If an address is being upgraded, it will be sent to the server in a
  // different format and with a few additional fields set, most importantly
  // |object_id_|.
  scoped_ptr<base::DictionaryValue> ToDictionaryWithID() const;

  // Newly created addresses will not have an associated |object_id_| and are
  // sent to the server in a slightly different format.
  scoped_ptr<base::DictionaryValue> ToDictionaryWithoutID() const;

  // Returns a string that summarizes this address, suitable for display to
  // the user.
  base::string16 DisplayName() const;

  // Returns a string that could be used as a sub-label, suitable for display
  // to the user together with DisplayName().
  base::string16 DisplayNameDetail() const;

  // Returns the phone number as a string that is suitable for display to the
  // user.
  base::string16 DisplayPhoneNumber() const;

  // Returns data appropriate for |type|.
  base::string16 GetInfo(const AutofillType& type,
                         const std::string& app_locale) const;

  const std::string& country_name_code() const { return country_name_code_; }
  const base::string16& recipient_name() const { return recipient_name_; }
  const base::string16& address_line_1() const { return address_line_1_; }
  const base::string16& address_line_2() const { return address_line_2_; }
  const base::string16& locality_name() const { return locality_name_; }
  const base::string16& administrative_area_name() const {
    return administrative_area_name_;
  }
  const base::string16& postal_code_number() const {
    return postal_code_number_;
  }
  const base::string16& phone_number() const { return phone_number_; }
  const std::string& object_id() const { return object_id_; }
  bool is_complete_address() const {
    return is_complete_address_;
  }

  void set_country_name_code(const std::string& country_name_code) {
    country_name_code_ = country_name_code;
  }
  void set_recipient_name(const base::string16& recipient_name) {
    recipient_name_ = recipient_name;
  }
  void set_address_line_1(const base::string16& address_line_1) {
    address_line_1_ = address_line_1;
  }
  void set_address_line_2(const base::string16& address_line_2) {
    address_line_2_ = address_line_2;
  }
  void set_locality_name(const base::string16& locality_name) {
    locality_name_ = locality_name;
  }
  void set_administrative_area_name(
      const base::string16& administrative_area_name) {
    administrative_area_name_ = administrative_area_name;
  }
  void set_postal_code_number(const base::string16& postal_code_number) {
    postal_code_number_ = postal_code_number;
  }
  void SetPhoneNumber(const base::string16& phone_number);
  void set_object_id(const std::string& object_id) {
    object_id_ = object_id;
  }
  void set_is_complete_address(bool is_complete_address) {
    is_complete_address_ = is_complete_address;
  }

  // Tests if this address exact matches |other|. |object_id| is ignored.
  bool EqualsIgnoreID(const Address& other) const;

  // Tests if this address exact matches |other| including |object_id|.
  bool operator==(const Address& other) const;
  bool operator!=(const Address& other) const;

 private:
  // |country_name_code_| should be an ISO 3166-1-alpha-2 (two letter codes, as
  // used in DNS). For example, "GB".
  std::string country_name_code_;

  // The recipient's name. For example "John Doe".
  base::string16 recipient_name_;

  // |address_line_1| and |address_line_2| correspond to the "AddressLine"
  // elements in xAL, which are used to hold unstructured text.
  base::string16 address_line_1_;
  base::string16 address_line_2_;

  // Locality.  This is something of a fuzzy term, but it generally refers to
  // the city/town portion of an address.  In regions of the world where
  // localities are not well defined or do not fit into this structure well
  // (for example, Japan and China), leave locality_name empty and use
  // |address_line_2|.
  // Examples: US city, IT comune, UK post town.
  base::string16 locality_name_;

  // Top-level administrative subdivision of this country.
  // Examples: US state, IT region, UK constituent nation, JP prefecture.
  // Note: this must be in short form, e.g. TX rather than Texas.
  base::string16 administrative_area_name_;

  // Despite the name, |postal_code_number_| values are frequently alphanumeric.
  // Examples: "94043", "SW1W", "SW1W 9TQ".
  base::string16 postal_code_number_;

  // A valid international phone number. If |phone_number_| is a user provided
  // value, it should have been validated using libphonenumber by clients of
  // this class before being set; see http://code.google.com/p/libphonenumber/.
  base::string16 phone_number_;

  // The parsed phone number.
  i18n::PhoneObject phone_object_;

  // Externalized Online Wallet id for this address.
  std::string object_id_;

  // Server's understanding of this address as complete address or not.
  bool is_complete_address_;

  // This class is intentionally copyable.
  DISALLOW_ASSIGN(Address);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_ADDRESS_H_
