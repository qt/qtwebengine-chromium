// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi/wifi_service.h"

#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "components/onc/onc_constants.h"

namespace wifi {

WiFiService::NetworkProperties::NetworkProperties()
    : connection_state(onc::connection_state::kNotConnected),
      security(onc::wifi::kNone),
      signal_strength(0),
      auto_connect(false),
      frequency(WiFiService::kFrequencyUnknown) {}

WiFiService::NetworkProperties::~NetworkProperties() {}

scoped_ptr<base::DictionaryValue> WiFiService::NetworkProperties::ToValue(
    bool network_list) const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  value->SetString(onc::network_config::kGUID, guid);
  value->SetString(onc::network_config::kName, name);
  value->SetString(onc::network_config::kConnectionState, connection_state);
  value->SetString(onc::network_config::kType, type);

  if (type == onc::network_type::kWiFi) {
    scoped_ptr<base::DictionaryValue> wifi(new base::DictionaryValue());
    wifi->SetString(onc::wifi::kSecurity, security);
    wifi->SetInteger(onc::wifi::kSignalStrength, signal_strength);

    // Network list expects subset of data.
    if (!network_list) {
      if (frequency != WiFiService::kFrequencyUnknown)
        wifi->SetInteger(onc::wifi::kFrequency, frequency);
      scoped_ptr<base::ListValue> frequency_list(new base::ListValue());
      for (FrequencyList::const_iterator it = this->frequency_list.begin();
           it != this->frequency_list.end();
           ++it) {
        frequency_list->AppendInteger(*it);
      }
      if (!frequency_list->empty())
        wifi->Set(onc::wifi::kFrequencyList, frequency_list.release());
      if (!bssid.empty())
        wifi->SetString(onc::wifi::kBSSID, bssid);
      wifi->SetString(onc::wifi::kSSID, ssid);
    }
    value->Set(onc::network_type::kWiFi, wifi.release());
  } else {
    // Add properites from json extra if present.
    if (!json_extra.empty()) {
      Value* value_extra = base::JSONReader::Read(json_extra);
      value->Set(type, value_extra);
    }
  }
  return value.Pass();
}

bool WiFiService::NetworkProperties::UpdateFromValue(
    const base::DictionaryValue& value) {
  const base::DictionaryValue* wifi = NULL;
  std::string network_type;
  // Get network type and make sure that it is WiFi (if specified).
  if (value.GetString(onc::network_config::kType, &network_type)) {
    if (network_type != onc::network_type::kWiFi)
      return false;
    type = network_type;
  }
  if (value.GetDictionary(onc::network_type::kWiFi, &wifi)) {
    wifi->GetString(onc::wifi::kSecurity, &security);
    wifi->GetString(onc::wifi::kSSID, &ssid);
    wifi->GetString(onc::wifi::kPassphrase, &password);
    wifi->GetBoolean(onc::wifi::kAutoConnect, &auto_connect);
    return true;
  }
  return false;
}

std::string WiFiService::NetworkProperties::MacAddressAsString(
    const uint8 mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX:XX:XX:XX:XX:XX.
  static const char* const kMacFormatString = "%02x:%02x:%02x:%02x:%02x:%02x";
  return base::StringPrintf(kMacFormatString,
                            mac_as_int[0],
                            mac_as_int[1],
                            mac_as_int[2],
                            mac_as_int[3],
                            mac_as_int[4],
                            mac_as_int[5]);
}

bool WiFiService::NetworkProperties::OrderByType(const NetworkProperties& l,
                                                 const NetworkProperties& r) {
  if (l.connection_state != r.connection_state)
    return l.connection_state < r.connection_state;
  // This sorting order is needed only for browser_tests, which expect this
  // network type sort order: ethernet < wifi < vpn < cellular.
  if (l.type == r.type)
    return l.guid < r.guid;
  if (l.type == onc::network_type::kEthernet)
    return true;
  if (r.type == onc::network_type::kEthernet)
    return false;
  return l.type > r.type;
}

}  // namespace wifi
