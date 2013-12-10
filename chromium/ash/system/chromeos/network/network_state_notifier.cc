// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_notifier.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_connect.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace {

const char kNetworkOutOfCreditsNotificationId[] =
    "chrome://settings/internet/out-of-credits";

const int kMinTimeBetweenOutOfCreditsNotifySeconds = 10 * 60;

// Error messages based on |error_name|, not network_state->error().
string16 GetConnectErrorString(const std::string& error_name) {
  if (error_name == NetworkConnectionHandler::kErrorNotFound)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error_name == NetworkConnectionHandler::kErrorConfigureFailed)
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CONFIGURE_FAILED);
  if (error_name == ash::network_connect::kErrorActivateFailed)
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  return string16();
}

void ShowErrorNotification(const std::string& notification_id,
                           const std::string& network_type,
                           const base::string16& title,
                           const base::string16& message,
                           const base::Closure& callback) {
  int icon_id = (network_type == flimflam::kTypeCellular) ?
      IDR_AURA_UBER_TRAY_CELLULAR_NETWORK_FAILED :
      IDR_AURA_UBER_TRAY_NETWORK_FAILED;
  const gfx::Image& icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(icon_id);
  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          notification_id,
          title,
          message,
          icon,
          ash::system_notifier::NOTIFIER_NETWORK_ERROR,
          callback));
}

void ConfigureNetwork(const std::string& service_path) {
  ash::Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
      service_path);
}

}  // namespace

namespace ash {

NetworkStateNotifier::NetworkStateNotifier()
    : did_show_out_of_credits_(false),
      weak_ptr_factory_(this) {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->AddObserver(this, FROM_HERE);
  UpdateDefaultNetwork(handler->DefaultNetwork());
}

NetworkStateNotifier::~NetworkStateNotifier() {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      this, FROM_HERE);
}

void NetworkStateNotifier::DefaultNetworkChanged(const NetworkState* network) {
  if (!UpdateDefaultNetwork(network))
    return;
  // If the default network changes to another network, allow the out of
  // credits notification to be shown again. A delay prevents the notification
  // from being shown too frequently (see below).
  if (network)
    did_show_out_of_credits_ = false;
}

void NetworkStateNotifier::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (network->type() != flimflam::kTypeCellular)
    return;
  UpdateCellularOutOfCredits(network);
  UpdateCellularActivating(network);
}

bool NetworkStateNotifier::UpdateDefaultNetwork(const NetworkState* network) {
  std::string default_network_path;
  if (network)
    default_network_path = network->path();
  if (default_network_path != last_default_network_) {
    last_default_network_ = default_network_path;
    return true;
  }
  return false;
}

void NetworkStateNotifier::UpdateCellularOutOfCredits(
    const NetworkState* cellular) {
  // Only display a notification if we are out of credits and have not already
  // shown a notification (or have since connected to another network type).
  if (!cellular->cellular_out_of_credits() || did_show_out_of_credits_)
    return;

  // Only display a notification if not connected, connecting, or waiting to
  // connect to another network.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* default_network = handler->DefaultNetwork();
  if (default_network && default_network != cellular)
    return;
  if (handler->ConnectingNetworkByType(NetworkTypePattern::NonVirtual()) ||
      NetworkHandler::Get()->network_connection_handler()
          ->HasPendingConnectRequest())
    return;

  did_show_out_of_credits_ = true;
  base::TimeDelta dtime = base::Time::Now() - out_of_credits_notify_time_;
  if (dtime.InSeconds() > kMinTimeBetweenOutOfCreditsNotifySeconds) {
    out_of_credits_notify_time_ = base::Time::Now();
    string16 error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_OUT_OF_CREDITS_BODY,
        UTF8ToUTF16(cellular->name()));
    ShowErrorNotification(
        kNetworkOutOfCreditsNotificationId,
        cellular->type(),
        l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_TITLE),
        error_msg,
        base::Bind(&ConfigureNetwork, cellular->path()));
  }
}

void NetworkStateNotifier::UpdateCellularActivating(
    const NetworkState* cellular) {
  // Keep track of any activating cellular network.
  std::string activation_state = cellular->activation_state();
  if (activation_state == flimflam::kActivationStateActivating) {
    cellular_activating_.insert(cellular->path());
    return;
  }
  // Only display a notification if this network was activating and is now
  // activated.
  if (!cellular_activating_.count(cellular->path()) ||
      activation_state != flimflam::kActivationStateActivated)
    return;

  cellular_activating_.erase(cellular->path());
  int icon_id;
  if (cellular->network_technology() == flimflam::kNetworkTechnologyLte)
    icon_id = IDR_AURA_UBER_TRAY_NOTIFICATION_LTE;
  else
    icon_id = IDR_AURA_UBER_TRAY_NOTIFICATION_3G;
  const gfx::Image& icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(icon_id);
  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          ash::network_connect::kNetworkActivateNotificationId,
          l10n_util::GetStringUTF16(IDS_NETWORK_CELLULAR_ACTIVATED_TITLE),
          l10n_util::GetStringFUTF16(IDS_NETWORK_CELLULAR_ACTIVATED,
                                     UTF8ToUTF16((cellular->name()))),
          icon,
          system_notifier::NOTIFIER_NETWORK,
          base::Bind(&ash::network_connect::ShowNetworkSettings,
                     cellular->path())));
}

void NetworkStateNotifier::ShowNetworkConnectError(
    const std::string& error_name,
    const std::string& shill_error,
    const std::string& service_path) {
  if (service_path.empty()) {
    base::DictionaryValue shill_properties;
    ShowConnectErrorNotification(error_name, shill_error, service_path,
                                 shill_properties);
    return;
  }
  // Get the up-to-date properties for the network and display the error.
  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&NetworkStateNotifier::ConnectErrorPropertiesSucceeded,
                 weak_ptr_factory_.GetWeakPtr(), error_name, shill_error),
      base::Bind(&NetworkStateNotifier::ConnectErrorPropertiesFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_name, shill_error,
                 service_path));
}

void NetworkStateNotifier::ConnectErrorPropertiesSucceeded(
    const std::string& error_name,
    const std::string& shill_error,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  ShowConnectErrorNotification(error_name, shill_error, service_path,
                               shill_properties);
}

void NetworkStateNotifier::ConnectErrorPropertiesFailed(
    const std::string& error_name,
    const std::string& shill_error,
    const std::string& service_path,
    const std::string& shill_connect_error,
    scoped_ptr<base::DictionaryValue> shill_error_data) {
  base::DictionaryValue shill_properties;
  ShowConnectErrorNotification(error_name, shill_error, service_path,
                               shill_properties);
}

void NetworkStateNotifier::ShowConnectErrorNotification(
    const std::string& error_name,
    const std::string& shill_error,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  string16 error = GetConnectErrorString(error_name);
  if (error.empty()) {
    // Service.Error gets cleared shortly after State transitions to Failure,
    // so rely on |shill_error| unless empty.
    std::string network_error = shill_error;
    if (network_error.empty()) {
      shill_properties.GetStringWithoutPathExpansion(
          flimflam::kErrorProperty, &network_error);
    }
    error = network_connect::ErrorString(network_error);
    if (error.empty())
      error = l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  }
  NET_LOG_ERROR("Connect error notification: " + UTF16ToUTF8(error),
                service_path);

  std::string network_name =
      chromeos::shill_property_util::GetNameFromProperties(service_path,
                                                           shill_properties);
  std::string network_error_details;
  shill_properties.GetStringWithoutPathExpansion(
        shill::kErrorDetailsProperty, &network_error_details);

  string16 error_msg;
  if (!network_error_details.empty()) {
    // network_name should't be empty if network_error_details is set.
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_SERVER_MESSAGE,
        UTF8ToUTF16(network_name), error,
        UTF8ToUTF16(network_error_details));
  } else if (network_name.empty()) {
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_NO_NAME, error);
  } else {
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE,
        UTF8ToUTF16(network_name), error);
  }

  std::string network_type;
  shill_properties.GetStringWithoutPathExpansion(
      flimflam::kTypeProperty, &network_type);

  ShowErrorNotification(
      network_connect::kNetworkConnectNotificationId,
      network_type,
      l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE),
      error_msg,
      base::Bind(&network_connect::ShowNetworkSettings, service_path));
}

}  // namespace ash
