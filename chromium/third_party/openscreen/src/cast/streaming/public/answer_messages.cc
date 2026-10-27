// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/answer_messages.h"

#include <string_view>
#include <utility>

#include "cast/streaming/public/constants.h"
#include "platform/base/error.h"
#include "util/enum_name_table.h"
#include "util/json/json_helpers.h"
#include "util/osp_logging.h"
#include "util/string_parse.h"
#include "util/string_util.h"
#include "util/stringprintf.h"

namespace openscreen::cast {

namespace {

/// Constraint properties.
// Audio constraints. See properties below.
constexpr char kAudio[] = "audio";
// Video constraints. See properties below.
constexpr char kVideo[] = "video";

// An optional field representing the minimum bits per second. If not specified
// by the receiver, the sender will use kDefaultAudioMinBitRate and
// kDefaultVideoMinBitRate, which represent the true operational minimum.
constexpr char kMinBitRate[] = "minBitRate";

// Maximum encoded bits per second. This is the lower of (1) the max capability
// of the decoder, or (2) the max data transfer rate.
constexpr char kMaxBitRate[] = "maxBitRate";
// Maximum supported end-to-end latency, in milliseconds. Proportional to the
// size of the data buffers in the receiver.
constexpr char kMaxDelay[] = "maxDelay";

/// Video constraint properties.
// Maximum pixel rate (width * height * framerate). Is often less than
// multiplying the fields in maxDimensions. This field is used to set the
// maximum processing rate.
constexpr char kMaxPixelsPerSecond[] = "maxPixelsPerSecond";
// Minimum dimensions. If omitted, the sender will assume a reasonable minimum
// with the same aspect ratio as maxDimensions, as close to 320*180 as possible.
// Should reflect the true operational minimum.
constexpr char kMinResolution[] = "minResolution";
// Maximum dimensions, not necessarily ideal dimensions.
constexpr char kMaxDimensions[] = "maxDimensions";

/// Audio constraint properties.
// Maximum supported sampling frequency (not necessarily ideal).
constexpr char kMaxSampleRate[] = "maxSampleRate";
// Maximum number of audio channels (1 is mono, 2 is stereo, etc.).
constexpr char kMaxChannels[] = "maxChannels";

/// Display description properties
// If this optional field is included in the ANSWER message, the receiver is
// attached to a fixed display that has the given dimensions and frame rate
// configuration. These may exceed, be the same, or be less than the values in
// constraints. If undefined, we assume the display is not fixed (e.g. a Google
// Hangouts UI panel).
constexpr char kDimensions[] = "dimensions";
// An optional field. When missing and dimensions are specified, the sender
// will assume square pixels and the dimensions imply the aspect ratio of the
// fixed display. WHen present and dimensions are also specified, implies the
// pixels are not square.
constexpr char kAspectRatio[] = "aspectRatio";
// The delimeter used for the aspect ratio format ("A:B").
constexpr char kAspectRatioDelimiter = ':';
// Sets the aspect ratio constraints. Value must be either "sender" or
// "receiver", see kScalingSender and kScalingReceiver below.
constexpr char kScaling[] = "scaling";
// scaling = "sender" means that the sender must provide video frames of a fixed
// aspect ratio. In this case, the dimensions object must be passed or an error
// case will occur.
constexpr char kScalingSender[] = "sender";
// scaling = "receiver" means that the sender may send arbitrarily sized frames,
// and the receiver will handle scaling and letterboxing as necessary.
constexpr char kScalingReceiver[] = "receiver";

/// Answer properties.
// A number specifying the UDP port used for all streams in this session.
// Must have a value between kUdpPortMin and kUdpPortMax.
constexpr char kUdpPort[] = "udpPort";
constexpr int kUdpPortMin = 1;
constexpr int kUdpPortMax = 65535;
// Numbers specifying the indexes chosen from the offer message.
constexpr char kSendIndexes[] = "sendIndexes";
// uint32_t values specifying the RTP SSRC values used to send the RTCP feedback
// of the stream indicated in kSendIndexes.
constexpr char kSsrcs[] = "ssrcs";
// Provides detailed maximum and minimum capabilities of the receiver for
// processing the selected streams. The sender may alter video resolution and
// frame rate throughout the session, and the constraints here determine how
// much data volume is allowed.
constexpr char kConstraints[] = "constraints";
// Provides details about the display on the receiver.
constexpr char kDisplay[] = "display";
// std::optional array of numbers specifying the indexes of streams that will
// send event logs through RTCP.
constexpr char kReceiverRtcpEventLog[] = "receiverRtcpEventLog";
// Optional array of numbers specifying the indexes of streams that will use
// DSCP values specified in the OFFER message for RTCP packets.
constexpr char kReceiverRtcpDscp[] = "receiverRtcpDscp";
// If this optional field is present the receiver supports the specific
// RTP extensions (such as adaptive playout delay).
constexpr char kRtpExtensions[] = "rtpExtensions";

EnumNameTable<AspectRatioConstraint, 2> kAspectRatioConstraintNames{
    {{kScalingReceiver, AspectRatioConstraint::kVariable},
     {kScalingSender, AspectRatioConstraint::kFixed}}};

Json::Value AspectRatioConstraintToJson(AspectRatioConstraint aspect_ratio) {
  return Json::Value(GetEnumName(kAspectRatioConstraintNames, aspect_ratio)
                         .value(kScalingSender));
}

std::optional<AspectRatioConstraint> TryParseAspectRatioConstraint(
    const Json::Value& value) {
  std::string aspect_ratio;
  if (!json::TryParseString(value, &aspect_ratio)) {
    return std::nullopt;
  }

  ErrorOr<AspectRatioConstraint> constraint =
      GetEnum(kAspectRatioConstraintNames, aspect_ratio);
  if (constraint.is_error()) {
    return std::nullopt;
  }
  return constraint.value();
}

template <typename T>
ErrorOr<std::optional<T>> ParseOptional(const Json::Value& value) {
  if (!value) {
    return std::optional<T>{};
  }
  auto out = T::TryParse(value);
  if (out.is_error()) {
    return out.error();
  }
  return std::optional<T>{std::move(out.value())};
}

}  // namespace

// static
ErrorOr<AspectRatio> AspectRatio::TryParse(const Json::Value& value) {
  std::string parsed_value;
  if (!json::TryParseString(value, &parsed_value)) {
    return Error(Error::Code::kJsonParseError, "Invalid aspect ratio string");
  }

  std::vector<std::string_view> fields =
      string_util::Split(parsed_value, kAspectRatioDelimiter);
  if (fields.size() != 2) {
    return Error(Error::Code::kJsonParseError, "Invalid aspect ratio format");
  }

  AspectRatio out;
  if (!string_parse::ParseAsciiNumber(fields[0], out.width) ||
      !string_parse::ParseAsciiNumber(fields[1], out.height)) {
    return Error(Error::Code::kJsonParseError, "Invalid aspect ratio values");
  }
  if (!out.IsValid()) {
    return Error(Error::Code::kJsonParseError, "Invalid aspect ratio");
  }
  return out;
}

bool AspectRatio::IsValid() const {
  return width > 0 && height > 0;
}

// static
ErrorOr<AudioConstraints> AudioConstraints::TryParse(const Json::Value& root) {
  if (!root.isObject()) {
    return Error(Error::Code::kJsonParseError,
                 "Audio constraints is not a JSON object");
  }

  AudioConstraints out;
  if (!json::TryParseInt(root[kMaxSampleRate], &out.max_sample_rate) ||
      !json::TryParseInt(root[kMaxChannels], &out.max_channels) ||
      !json::TryParseInt(root[kMaxBitRate], &out.max_bit_rate)) {
    return Error(Error::Code::kJsonParseError, "Invalid audio constraints");
  }

  std::chrono::milliseconds max_delay;
  if (json::TryParseMilliseconds(root[kMaxDelay], &max_delay)) {
    out.max_delay = max_delay;
  }

  if (!json::TryParseInt(root[kMinBitRate], &out.min_bit_rate)) {
    out.min_bit_rate = kDefaultAudioMinBitRate;
  }
  if (!out.IsValid()) {
    return Error(Error::Code::kJsonParseError, "Invalid audio constraints");
  }
  return out;
}

Json::Value AudioConstraints::ToJson() const {
  OSP_CHECK(IsValid());
  Json::Value root;
  root[kMaxSampleRate] = max_sample_rate;
  root[kMaxChannels] = max_channels;
  root[kMinBitRate] = min_bit_rate;
  root[kMaxBitRate] = max_bit_rate;
  if (max_delay.has_value()) {
    root[kMaxDelay] = Json::Value::Int64(max_delay->count());
  }
  return root;
}

bool AudioConstraints::IsValid() const {
  return max_sample_rate > 0 && max_channels > 0 && min_bit_rate > 0 &&
         max_bit_rate >= min_bit_rate;
}

// static
ErrorOr<VideoConstraints> VideoConstraints::TryParse(const Json::Value& root) {
  if (!root.isObject()) {
    return Error(Error::Code::kJsonParseError,
                 "Video constraints is not a JSON object");
  }

  VideoConstraints out;

  auto max_dimensions = Dimensions::TryParse(root[kMaxDimensions]);
  if (max_dimensions.is_error()) {
    return max_dimensions.error();
  }
  out.max_dimensions = std::move(max_dimensions.value());

  if (!json::TryParseInt(root[kMaxBitRate], &out.max_bit_rate)) {
    return Error(Error::Code::kJsonParseError,
                 "Invalid video constraints: missing maxBitRate");
  }

  auto min_resolution = ParseOptional<Dimensions>(root[kMinResolution]);
  if (min_resolution.is_error()) {
    return min_resolution.error();
  }
  out.min_resolution = std::move(min_resolution.value());

  std::chrono::milliseconds max_delay;
  if (json::TryParseMilliseconds(root[kMaxDelay], &max_delay)) {
    out.max_delay = max_delay;
  }

  double max_pixels_per_second;
  if (json::TryParseDouble(root[kMaxPixelsPerSecond], &max_pixels_per_second)) {
    out.max_pixels_per_second = max_pixels_per_second;
  }

  if (!json::TryParseInt(root[kMinBitRate], &out.min_bit_rate)) {
    out.min_bit_rate = kDefaultVideoMinBitRate;
  }
  if (!out.IsValid()) {
    return Error(Error::Code::kJsonParseError, "Invalid video constraints");
  }
  return out;
}

bool VideoConstraints::IsValid() const {
  return max_pixels_per_second > 0 && min_bit_rate > 0 &&
         max_bit_rate > min_bit_rate &&
         (!max_delay.has_value() || max_delay->count() > 0) &&
         max_dimensions.IsValid() &&
         (!min_resolution.has_value() || min_resolution->IsValid()) &&
         max_dimensions.frame_rate.numerator() > 0;
}

Json::Value VideoConstraints::ToJson() const {
  OSP_CHECK(IsValid());
  Json::Value root;
  root[kMaxDimensions] = max_dimensions.ToJson();
  root[kMinBitRate] = min_bit_rate;
  root[kMaxBitRate] = max_bit_rate;
  if (max_pixels_per_second.has_value()) {
    root[kMaxPixelsPerSecond] = max_pixels_per_second.value();
  }

  if (min_resolution.has_value()) {
    root[kMinResolution] = min_resolution->ToJson();
  }

  if (max_delay.has_value()) {
    root[kMaxDelay] = Json::Value::Int64(max_delay->count());
  }
  return root;
}

// static
ErrorOr<Constraints> Constraints::TryParse(const Json::Value& root) {
  if (!root.isObject()) {
    return Error(Error::Code::kJsonParseError,
                 "Constraints is not a JSON object");
  }

  Constraints out;

  auto audio = AudioConstraints::TryParse(root[kAudio]);
  if (audio.is_error()) {
    return audio.error();
  }
  out.audio = std::move(audio.value());

  auto video = VideoConstraints::TryParse(root[kVideo]);
  if (video.is_error()) {
    return video.error();
  }
  out.video = std::move(video.value());

  if (!out.IsValid()) {
    return Error(Error::Code::kJsonParseError, "Invalid constraints");
  }
  return out;
}

bool Constraints::IsValid() const {
  return audio.IsValid() && video.IsValid();
}

Json::Value Constraints::ToJson() const {
  OSP_CHECK(IsValid());
  Json::Value root;
  root[kAudio] = audio.ToJson();
  root[kVideo] = video.ToJson();
  return root;
}

// static
ErrorOr<DisplayDescription> DisplayDescription::TryParse(
    const Json::Value& root) {
  if (!root.isObject()) {
    return Error(Error::Code::kJsonParseError,
                 "Display description is not a JSON object");
  }

  DisplayDescription out;

  auto dimensions = ParseOptional<Dimensions>(root[kDimensions]);
  if (dimensions.is_error()) {
    return dimensions.error();
  }
  out.dimensions = std::move(dimensions.value());

  auto aspect_ratio = ParseOptional<AspectRatio>(root[kAspectRatio]);
  if (aspect_ratio.is_error()) {
    return aspect_ratio.error();
  }
  out.aspect_ratio = std::move(aspect_ratio.value());

  auto constraint = TryParseAspectRatioConstraint(root[kScaling]);
  if (constraint.has_value()) {
    out.aspect_ratio_constraint = constraint.value();
  } else {
    out.aspect_ratio_constraint = std::nullopt;
  }

  if (!out.IsValid()) {
    return Error(Error::Code::kJsonParseError, "Invalid display description");
  }
  return out;
}

bool DisplayDescription::IsValid() const {
  // At least one of the properties must be set, and if a property is set
  // it must be valid.
  if (aspect_ratio.has_value() && !aspect_ratio->IsValid()) {
    return false;
  }

  if (dimensions.has_value() && !dimensions->IsValid()) {
    return false;
  }

  // Sender behavior is undefined if the aspect ratio is fixed but no
  // dimensions or aspect ratio are provided.
  if (aspect_ratio_constraint.has_value() &&
      (aspect_ratio_constraint.value() == AspectRatioConstraint::kFixed) &&
      !dimensions.has_value() && !aspect_ratio.has_value()) {
    return false;
  }
  return aspect_ratio.has_value() || dimensions.has_value() ||
         aspect_ratio_constraint.has_value();
}

Json::Value DisplayDescription::ToJson() const {
  OSP_CHECK(IsValid());
  Json::Value root;
  if (aspect_ratio.has_value()) {
    root[kAspectRatio] =
        StringFormat("{}{}{}", aspect_ratio->width, kAspectRatioDelimiter,
                     aspect_ratio->height);
  }
  if (dimensions.has_value()) {
    root[kDimensions] = dimensions->ToJson();
  }
  if (aspect_ratio_constraint.has_value()) {
    root[kScaling] =
        AspectRatioConstraintToJson(aspect_ratio_constraint.value());
  }
  return root;
}

ErrorOr<Answer> Answer::TryParse(const Json::Value& root) {
  if (!root.isObject()) {
    return Error(Error::Code::kJsonParseError, "Answer is not a JSON object");
  }

  Answer out;
  if (!json::TryParseInt(root[kUdpPort], &out.udp_port) ||
      !json::TryParseIntArray(root[kSendIndexes], &out.send_indexes) ||
      !json::TryParseUintArray(root[kSsrcs], &out.ssrcs)) {
    return Error(Error::Code::kJsonParseError,
                 "Invalid answer: missing or invalid mandatory fields");
  }

  auto constraints = ParseOptional<Constraints>(root[kConstraints]);
  if (constraints.is_error()) {
    return constraints.error();
  }
  out.constraints = std::move(constraints.value());

  auto display = ParseOptional<DisplayDescription>(root[kDisplay]);
  if (display.is_error()) {
    return display.error();
  }
  out.display = std::move(display.value());

  // These functions set to empty array if not present, so we can ignore
  // the return value for optional values.
  json::TryParseIntArray(root[kReceiverRtcpEventLog],
                         &out.receiver_rtcp_event_log);
  json::TryParseIntArray(root[kReceiverRtcpDscp], &out.receiver_rtcp_dscp);
  json::TryParseNestedStringArray(root[kRtpExtensions], &out.rtp_extensions);

  if (!out.IsValid()) {
    return Error(Error::Code::kJsonParseError, "Invalid answer");
  }
  return out;
}

bool Answer::IsValid() const {
  if (ssrcs.empty() || send_indexes.empty()) {
    return false;
  }

  // We don't know what the indexes used in the offer were here, so we just
  // sanity check.
  for (const int index : send_indexes) {
    if (index < 0) {
      return false;
    }
  }
  if (constraints.has_value() && !constraints->IsValid()) {
    return false;
  }
  if (display.has_value() && !display->IsValid()) {
    return false;
  }
  return kUdpPortMin <= udp_port && udp_port <= kUdpPortMax;
}

Json::Value Answer::ToJson() const {
  OSP_CHECK(IsValid());
  Json::Value root;
  if (constraints.has_value()) {
    root[kConstraints] = constraints->ToJson();
  }
  if (display.has_value()) {
    root[kDisplay] = display->ToJson();
  }
  root[kUdpPort] = udp_port;
  root[kSendIndexes] = json::PrimitiveVectorToJson(send_indexes);
  root[kSsrcs] = json::PrimitiveVectorToJson(ssrcs);
  // Some sender do not handle empty array properly, so we omit these fields
  // if they are empty.
  if (!receiver_rtcp_event_log.empty()) {
    root[kReceiverRtcpEventLog] =
        json::PrimitiveVectorToJson(receiver_rtcp_event_log);
  }
  if (!receiver_rtcp_dscp.empty()) {
    root[kReceiverRtcpDscp] = json::PrimitiveVectorToJson(receiver_rtcp_dscp);
  }
  if (!rtp_extensions.empty()) {
    root[kRtpExtensions] = json::NestedStringArrayToJson(rtp_extensions);
  }
  return root;
}

}  // namespace openscreen::cast
