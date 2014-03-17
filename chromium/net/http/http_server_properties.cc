// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace net {

const char kAlternateProtocolHeader[] = "Alternate-Protocol";

namespace {

// The order of these strings much match the order of the enum definition
// for AlternateProtocol.
const char* const kAlternateProtocolStrings[] = {
  "npn-spdy/2",
  "npn-spdy/3",
  "npn-spdy/3.1",
  "npn-spdy/4a2",
  "npn-HTTP-draft-04/2.0",
  "quic"
};
const char kBrokenAlternateProtocol[] = "Broken";

COMPILE_ASSERT(
    arraysize(kAlternateProtocolStrings) == NUM_VALID_ALTERNATE_PROTOCOLS,
    kAlternateProtocolStringsSize_kNumValidAlternateProtocols_not_equal);

}  // namespace

bool IsAlternateProtocolValid(AlternateProtocol protocol) {
  return protocol >= ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION &&
      protocol <= ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION;
}

const char* AlternateProtocolToString(AlternateProtocol protocol) {
  switch (protocol) {
    case DEPRECATED_NPN_SPDY_2:
    case NPN_SPDY_3:
    case NPN_SPDY_3_1:
    case NPN_SPDY_4A2:
    case NPN_HTTP2_DRAFT_04:
    case QUIC:
      DCHECK(IsAlternateProtocolValid(protocol));
      return kAlternateProtocolStrings[
          protocol - ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION];
    case ALTERNATE_PROTOCOL_BROKEN:
      return kBrokenAlternateProtocol;
    case UNINITIALIZED_ALTERNATE_PROTOCOL:
      return "Uninitialized";
  }
  NOTREACHED();
  return "";
}

AlternateProtocol AlternateProtocolFromString(const std::string& str) {
  for (int i = ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION;
       i <= ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION; ++i) {
    AlternateProtocol protocol = static_cast<AlternateProtocol>(i);
    if (str == AlternateProtocolToString(protocol))
      return protocol;
  }
  if (str == kBrokenAlternateProtocol)
    return ALTERNATE_PROTOCOL_BROKEN;
  return UNINITIALIZED_ALTERNATE_PROTOCOL;
}

AlternateProtocol AlternateProtocolFromNextProto(NextProto next_proto) {
  switch (next_proto) {
    case kProtoDeprecatedSPDY2:
      return DEPRECATED_NPN_SPDY_2;
    case kProtoSPDY3:
      return NPN_SPDY_3;
    case kProtoSPDY31:
      return NPN_SPDY_3_1;
    case kProtoSPDY4a2:
      return NPN_SPDY_4A2;
    case kProtoHTTP2Draft04:
      return NPN_HTTP2_DRAFT_04;
    case kProtoQUIC1SPDY3:
      return QUIC;

    case kProtoUnknown:
    case kProtoHTTP11:
      break;
  }

  NOTREACHED() << "Invalid NextProto: " << next_proto;
  return UNINITIALIZED_ALTERNATE_PROTOCOL;
}

std::string PortAlternateProtocolPair::ToString() const {
  return base::StringPrintf("%d:%s", port,
                            AlternateProtocolToString(protocol));
}

}  // namespace net
