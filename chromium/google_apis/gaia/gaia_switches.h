// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
#define GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_

namespace switches {

// Supplies custom client login to OAuth2 URL for testing purposes.
extern const char kClientLoginToOAuth2Url[];

// Specifies the path for GAIA authentication URL. The default value is
// "https://accounts.google.com".
extern const char kGaiaUrl[];

// Specifies the backend server used for Google API calls.
// The default value is "https://www.googleapis.com".
extern const char kGoogleApisUrl[];

// Specifies the backend server used for lso authentication calls.
// "https://accounts.google.com".
extern const char kLsoUrl[];

// Specifies custom OAuth1 login scope for testing purposes.
extern const char kOAuth1LoginScope[];

// Overrides OAuth wrap bridge user info scope.
extern const char kOAuthWrapBridgeUserInfoScope[];

}  // namespace switches

#endif  // GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
