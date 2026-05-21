// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
#define CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_

#include <string>
#include <string_view>

#include "cast/common/channel/proto/cast_channel.pb.h"

namespace Json {
class Value;
}

namespace openscreen::cast {

// Reserved message namespaces for internal messages.
inline constexpr char kCastInternalNamespacePrefix[] =
    "urn:x-cast:com.google.cast.";
inline constexpr char kTransportNamespacePrefix[] =
    "urn:x-cast:com.google.cast.tp.";
inline constexpr char kAuthNamespace[] =
    "urn:x-cast:com.google.cast.tp.deviceauth";
inline constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";
inline constexpr char kConnectionNamespace[] =
    "urn:x-cast:com.google.cast.tp.connection";
inline constexpr char kReceiverNamespace[] =
    "urn:x-cast:com.google.cast.receiver";
inline constexpr char kBroadcastNamespace[] =
    "urn:x-cast:com.google.cast.broadcast";
inline constexpr char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";
static constexpr char kSetupNamespace[] = "urn:x-cast:com.google.cast.setup";
static constexpr char kDiscoveryNamespace[] =
    "urn:x-cast:com.google.cast.receiver.discovery";

// Sender and receiver IDs to use for platform messages.
inline constexpr char kPlatformSenderId[] = "sender-0";
inline constexpr char kPlatformReceiverId[] = "receiver-0";

inline constexpr char kBroadcastId[] = "*";

inline constexpr proto::CastMessage_ProtocolVersion
    kDefaultOutgoingMessageVersion =
        proto::CastMessage_ProtocolVersion_CASTV2_1_0;

// Default device capabilities reported in DeviceInfo messages.
// This value is a bitmask representing:
// CAP_VIDEO_OUT (0x1) | CAP_AUDIO_OUT (0x4) | CAP_MASTER_OR_FIXED_VOLUME
// (0x800) | CAP_ATTENUATION_OR_FIXED_VOLUME (0x1000)
// See
// https://developers.google.com/android/reference/com/google/android/gms/cast/CastDevice#constants
constexpr int kDefaultDeviceCapabilities = 6149;

// JSON message key strings.
inline constexpr char kMessageKeyType[] = "type";
inline constexpr char kMessageKeyProtocolVersion[] = "protocolVersion";
inline constexpr char kMessageKeyProtocolVersionList[] = "protocolVersionList";
inline constexpr char kMessageKeyReasonCode[] = "reasonCode";
inline constexpr char kMessageKeyAppId[] = "appId";
inline constexpr char kMessageKeyRequestId[] = "requestId";
inline constexpr char kMessageKeyResponseType[] = "responseType";
inline constexpr char kMessageKeyTransportId[] = "transportId";
inline constexpr char kMessageKeySessionId[] = "sessionId";

// JSON message field values.
inline constexpr char kMessageTypeConnect[] = "CONNECT";
inline constexpr char kMessageTypeClose[] = "CLOSE";
inline constexpr char kMessageTypeConnected[] = "CONNECTED";
inline constexpr char kMessageValueAppAvailable[] = "APP_AVAILABLE";
inline constexpr char kMessageValueAppUnavailable[] = "APP_UNAVAILABLE";

// JSON message key strings specific to CONNECT messages.
inline constexpr char kMessageKeyBrowserVersion[] = "browserVersion";
inline constexpr char kMessageKeyConnType[] = "connType";
inline constexpr char kMessageKeyConnectionType[] = "connectionType";
inline constexpr char kMessageKeyUserAgent[] = "userAgent";
inline constexpr char kMessageKeyOrigin[] = "origin";
inline constexpr char kMessageKeyPlatform[] = "platform";
inline constexpr char kMessageKeySdkType[] = "skdType";
inline constexpr char kMessageKeySenderInfo[] = "senderInfo";
inline constexpr char kMessageKeyVersion[] = "version";

// JSON message key strings specific to application control messages.
inline constexpr char kMessageKeyAvailability[] = "availability";
inline constexpr char kMessageKeyAppParams[] = "appParams";
inline constexpr char kMessageKeyApplications[] = "applications";
inline constexpr char kMessageKeyControlType[] = "controlType";
inline constexpr char kMessageKeyDisplayName[] = "displayName";
inline constexpr char kMessageKeyIsIdleScreen[] = "isIdleScreen";
inline constexpr char kMessageKeyLaunchedFromCloud[] = "launchedFromCloud";
inline constexpr char kMessageKeyLevel[] = "level";
inline constexpr char kMessageKeyMuted[] = "muted";
inline constexpr char kMessageKeyName[] = "name";
inline constexpr char kMessageKeyNamespaces[] = "namespaces";
inline constexpr char kMessageKeyReason[] = "reason";
inline constexpr char kMessageKeyStatus[] = "status";
inline constexpr char kMessageKeyStepInterval[] = "stepInterval";
inline constexpr char kMessageKeyUniversalAppId[] = "universalAppId";
inline constexpr char kMessageKeyUserEq[] = "userEq";
inline constexpr char kMessageKeyVolume[] = "volume";
inline constexpr char kMessageKeyLaunchRequestId[] = "launchRequestId";

// JSON message field value strings specific to application control messages.
inline constexpr char kMessageValueAttenuation[] = "attenuation";
inline constexpr char kMessageValueBadParameter[] = "BAD_PARAMETER";
inline constexpr char kMessageValueInvalidSessionId[] = "INVALID_SESSION_ID";
inline constexpr char kMessageValueInvalidCommand[] = "INVALID_COMMAND";
inline constexpr char kMessageValueNotFound[] = "NOT_FOUND";
inline constexpr char kMessageValueSystemError[] = "SYSTEM_ERROR";
inline constexpr char kMessageValueUserAllowed[] = "USER_ALLOWED";

// JSON message key strings specific to DEVICE_INFO messages.
inline constexpr char kMessageKeyControlNotifications[] =
    "controlNotifications";
inline constexpr char kMessageKeyDeviceCapabilities[] = "deviceCapabilities";
inline constexpr char kMessageKeyDeviceId[] = "deviceId";
inline constexpr char kMessageKeyDeviceModel[] = "deviceModel";
inline constexpr char kMessageKeyFriendlyName[] = "friendlyName";

// JSON message key strings specific to eureka_info messages.
inline constexpr char kMessageKeyEurekaInfoRequestId[] = "request_id";
inline constexpr char kMessageKeyData[] = "data";
inline constexpr char kMessageKeyDeviceInfo[] = "device_info";
inline constexpr char kMessageKeyManufacturer[] = "manufacturer";
inline constexpr char kMessageKeyProductName[] = "product_name";
inline constexpr char kMessageKeySsdpUdn[] = "ssdp_udn";
inline constexpr char kMessageKeyBuildInfo[] = "build_info";
inline constexpr char kMessageKeyBuildType[] = "build_type";
inline constexpr char kMessageKeyCastBuildRevision[] = "cast_build_revision";
inline constexpr char kMessageKeySystemBuildNumber[] = "system_build_number";
inline constexpr char kMessageKeyResponseCode[] = "response_code";
inline constexpr char kMessageKeyResponseString[] = "response_string";

// Represents known types of cast messages, intended to be used on the wire in
// JSON messages as a serialized string. Values are subject to change and should
// not be depended upon statically.
//
// For more information, see the section on "Messages and Namespaces" in
// ../../docs/streaming_session_protocol.md.
enum class CastMessageType {
  kUnknown = 0,

  // Heartbeat messages.
  kPing = 1,
  kPong = 2,

  // RPC control/status messages used by Media Remoting. These occur at high
  // frequency, up to dozens per second at times, and should not be logged.
  kRpc = 3,

  // Virtual connection open and close requests.
  kConnect = 4,
  kCloseConnection = 5,

  // Application broadcast / precache.
  kBroadcast = 6,

  // Session launch request.
  kLaunch = 7,

  // Launch request succeeded.
  kLaunchStatus = 8,

  // Indicates that the launch request failed.
  kLaunchError = 9,

  // Session stop request.
  kStop = 10,

  // Sent by the receiver whenever an invalid request is received.
  kInvalidRequest = 11,

  // Request and reply for information about what applications are available.
  kGetAppAvailability = 12,

  // Reply with information about the current state of the receiver.
  kReceiverStatus = 13,
  kMediaStatus = 14,

  // Used for starting and negotiating a streaming session.
  kOffer = 15,
  kAnswer = 16,
  kGetCapabilities = 17,
  kCapabilitiesResponse = 18,
  kGetStatus = 19,
  kStatusResponse = 20,

  // The following values are part of the protocol but are not currently
  // supported anywhere.
  kInvalidPlayerState = 21,
  kLoadCancelled = 22,
  kLoadFailed = 23,
  kMultizoneStatus = 24,
  kPresentation = 25,

  // Necessary for setup, request for device information.
  kGetDeviceInfo = 26,
  kEurekaInfo = 27,

  // Max value should be updated to the highest value of the enum.
  kMaxValue = kEurekaInfo,
};

enum class AppAvailabilityResult {
  kAvailable,
  kUnavailable,
  kUnknown,
};

std::string ToString(AppAvailabilityResult availability);

const char* CastMessageTypeToString(CastMessageType type);

inline bool IsAuthMessage(const proto::CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

inline bool IsTransportNamespace(std::string_view namespace_) {
  return (namespace_.size() > (sizeof(kTransportNamespacePrefix) - 1)) &&
         (namespace_.find_first_of(kTransportNamespacePrefix) == 0);
}

proto::CastMessage MakeSimpleUTF8Message(const std::string& namespace_,
                                         std::string payload);

proto::CastMessage MakeConnectMessage(const std::string& source_id,
                                      const std::string& destination_id);
proto::CastMessage MakeCloseMessage(const std::string& source_id,
                                    const std::string& destination_id);

// Returns a session/transport ID string that is unique within this application
// instance, having the format "prefix-12345". For example, calling this with a
// `prefix` of "sender" will result in a string like "sender-12345".
std::string MakeUniqueSessionId(const char* prefix);

// Returns true if the type field in `object` is set to the given `type`.
bool HasType(const Json::Value& object, CastMessageType type);

// Serializes a given cast message to a string.
std::string ToString(const proto::CastMessage& message);

// Helper to get the actual message payload out of a cast message.
const std::string& GetPayload(const proto::CastMessage& message);

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
