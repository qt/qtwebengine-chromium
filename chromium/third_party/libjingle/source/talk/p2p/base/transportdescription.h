/*
 * libjingle
 * Copyright 2012, The Libjingle Authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_P2P_BASE_TRANSPORTDESCRIPTION_H_
#define TALK_P2P_BASE_TRANSPORTDESCRIPTION_H_

#include <algorithm>
#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/sslfingerprint.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/constants.h"

namespace cricket {

// SEC_ENABLED and SEC_REQUIRED should only be used if the session
// was negotiated over TLS, to protect the inline crypto material
// exchange.
// SEC_DISABLED: No crypto in outgoing offer, ignore any supplied crypto.
// SEC_ENABLED:  Crypto in outgoing offer and answer (if supplied in offer).
// SEC_REQUIRED: Crypto in outgoing offer and answer. Fail any offer with absent
//               or unsupported crypto.
enum SecurePolicy {
  SEC_DISABLED,
  SEC_ENABLED,
  SEC_REQUIRED
};

// The transport protocol we've elected to use.
enum TransportProtocol {
  ICEPROTO_GOOGLE,  // Google version of ICE protocol.
  ICEPROTO_HYBRID,  // ICE, but can fall back to the Google version.
  ICEPROTO_RFC5245  // Standard RFC 5245 version of ICE.
};
// The old name for TransportProtocol.
// TODO(juberti): remove this.
typedef TransportProtocol IceProtocolType;

// Whether our side of the call is driving the negotiation, or the other side.
enum IceRole {
  ICEROLE_CONTROLLING = 0,
  ICEROLE_CONTROLLED,
  ICEROLE_UNKNOWN
};

// ICE RFC 5245 implementation type.
enum IceMode {
  ICEMODE_FULL,  // As defined in http://tools.ietf.org/html/rfc5245#section-4.1
  ICEMODE_LITE   // As defined in http://tools.ietf.org/html/rfc5245#section-4.2
};

// RFC 4145 - http://tools.ietf.org/html/rfc4145#section-4
// 'active':  The endpoint will initiate an outgoing connection.
// 'passive': The endpoint will accept an incoming connection.
// 'actpass': The endpoint is willing to accept an incoming
//            connection or to initiate an outgoing connection.
enum ConnectionRole {
  CONNECTIONROLE_NONE = 0,
  CONNECTIONROLE_ACTIVE,
  CONNECTIONROLE_PASSIVE,
  CONNECTIONROLE_ACTPASS,
  CONNECTIONROLE_HOLDCONN,
};

extern const char CONNECTIONROLE_ACTIVE_STR[];
extern const char CONNECTIONROLE_PASSIVE_STR[];
extern const char CONNECTIONROLE_ACTPASS_STR[];
extern const char CONNECTIONROLE_HOLDCONN_STR[];

bool StringToConnectionRole(const std::string& role_str, ConnectionRole* role);
bool ConnectionRoleToString(const ConnectionRole& role, std::string* role_str);

typedef std::vector<Candidate> Candidates;

struct TransportDescription {
  TransportDescription() : ice_mode(ICEMODE_FULL) {}

  TransportDescription(const std::string& transport_type,
                       const std::vector<std::string>& transport_options,
                       const std::string& ice_ufrag,
                       const std::string& ice_pwd,
                       IceMode ice_mode,
                       ConnectionRole role,
                       const talk_base::SSLFingerprint* identity_fingerprint,
                       const Candidates& candidates)
      : transport_type(transport_type),
        transport_options(transport_options),
        ice_ufrag(ice_ufrag),
        ice_pwd(ice_pwd),
        ice_mode(ice_mode),
        connection_role(role),
        identity_fingerprint(CopyFingerprint(identity_fingerprint)),
        candidates(candidates) {}
  TransportDescription(const std::string& transport_type,
                       const std::string& ice_ufrag,
                       const std::string& ice_pwd)
      : transport_type(transport_type),
        ice_ufrag(ice_ufrag),
        ice_pwd(ice_pwd),
        ice_mode(ICEMODE_FULL),
        connection_role(CONNECTIONROLE_NONE) {}
  TransportDescription(const TransportDescription& from)
      : transport_type(from.transport_type),
        transport_options(from.transport_options),
        ice_ufrag(from.ice_ufrag),
        ice_pwd(from.ice_pwd),
        ice_mode(from.ice_mode),
        connection_role(from.connection_role),
        identity_fingerprint(CopyFingerprint(from.identity_fingerprint.get())),
        candidates(from.candidates) {}

  TransportDescription& operator=(const TransportDescription& from) {
    // Self-assignment
    if (this == &from)
      return *this;

    transport_type = from.transport_type;
    transport_options = from.transport_options;
    ice_ufrag = from.ice_ufrag;
    ice_pwd = from.ice_pwd;
    ice_mode = from.ice_mode;
    connection_role = from.connection_role;

    identity_fingerprint.reset(CopyFingerprint(
        from.identity_fingerprint.get()));
    candidates = from.candidates;
    return *this;
  }

  bool HasOption(const std::string& option) const {
    return (std::find(transport_options.begin(), transport_options.end(),
                      option) != transport_options.end());
  }
  void AddOption(const std::string& option) {
    transport_options.push_back(option);
  }
  bool secure() const { return identity_fingerprint != NULL; }

  static talk_base::SSLFingerprint* CopyFingerprint(
      const talk_base::SSLFingerprint* from) {
    if (!from)
      return NULL;

    return new talk_base::SSLFingerprint(*from);
  }

  std::string transport_type;  // xmlns of <transport>
  std::vector<std::string> transport_options;
  std::string ice_ufrag;
  std::string ice_pwd;
  IceMode ice_mode;
  ConnectionRole connection_role;

  talk_base::scoped_ptr<talk_base::SSLFingerprint> identity_fingerprint;
  Candidates candidates;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORTDESCRIPTION_H_
