/*
 * libjingle
 * Copyright 2013 Google Inc. All rights reserved.
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

#include "talk/p2p/base/transportdescription.h"

#include "talk/p2p/base/constants.h"

namespace cricket {

bool StringToConnectionRole(const std::string& role_str, ConnectionRole* role) {
  const char* const roles[] = {
      CONNECTIONROLE_ACTIVE_STR,
      CONNECTIONROLE_PASSIVE_STR,
      CONNECTIONROLE_ACTPASS_STR,
      CONNECTIONROLE_HOLDCONN_STR
  };

  for (size_t i = 0; i < ARRAY_SIZE(roles); ++i) {
    if (stricmp(roles[i], role_str.c_str()) == 0) {
      *role = static_cast<ConnectionRole>(CONNECTIONROLE_ACTIVE + i);
      return true;
    }
  }
  return false;
}

bool ConnectionRoleToString(const ConnectionRole& role, std::string* role_str) {
  switch (role) {
    case cricket::CONNECTIONROLE_ACTIVE:
      *role_str = cricket::CONNECTIONROLE_ACTIVE_STR;
      break;
    case cricket::CONNECTIONROLE_ACTPASS:
      *role_str = cricket::CONNECTIONROLE_ACTPASS_STR;
      break;
    case cricket::CONNECTIONROLE_PASSIVE:
      *role_str = cricket::CONNECTIONROLE_PASSIVE_STR;
      break;
    case cricket::CONNECTIONROLE_HOLDCONN:
      *role_str = cricket::CONNECTIONROLE_HOLDCONN_STR;
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace cricket

