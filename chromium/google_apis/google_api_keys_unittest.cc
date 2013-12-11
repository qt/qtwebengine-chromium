// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for implementation of google_api_keys namespace.
//
// Because the file deals with a lot of preprocessor defines and
// optionally includes an internal header, the way we test is by
// including the .cc file multiple times with different defines set.
// This is a little unorthodox, but it lets us test the behavior as
// close to unmodified as possible.

#include "google_apis/google_api_keys.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// The Win builders fail (with a linker crash) when trying to link
// unit_tests, and the Android builders complain about multiply
// defined symbols (likely they don't do name decoration as well as
// the Mac and Linux linkers).  Therefore these tests are only built
// and run on Mac and Linux, which should provide plenty of coverage
// since there are no platform-specific bits in this code.
#if defined(OS_LINUX) || defined(OS_MACOSX)

// We need to include everything included by google_api_keys.cc once
// at global scope so that things like STL and classes from base don't
// get defined when we re-include the google_api_keys.cc file
// below. We used to include that file in its entirety here, but that
// can cause problems if the linker decides the version of symbols
// from that file included here is the "right" version.
#include <string>
#include "base/command_line.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringize_macros.h"

// This is the default baked-in value for OAuth IDs and secrets.
static const char kDummyToken[] = "dummytoken";

struct EnvironmentCache {
 public:
  EnvironmentCache() : variable_name(NULL), was_set(false) {}

  const char* variable_name;
  bool was_set;
  std::string value;
};

class GoogleAPIKeysTest : public testing::Test {
 public:
  GoogleAPIKeysTest() : env_(base::Environment::Create()) {
    env_cache_[0].variable_name = "GOOGLE_API_KEY";
    env_cache_[1].variable_name = "GOOGLE_CLIENT_ID_MAIN";
    env_cache_[2].variable_name = "GOOGLE_CLIENT_SECRET_MAIN";
    env_cache_[3].variable_name = "GOOGLE_CLIENT_ID_CLOUD_PRINT";
    env_cache_[4].variable_name = "GOOGLE_CLIENT_SECRET_CLOUD_PRINT";
    env_cache_[5].variable_name = "GOOGLE_CLIENT_ID_REMOTING";
    env_cache_[6].variable_name = "GOOGLE_CLIENT_SECRET_REMOTING";
    env_cache_[7].variable_name = "GOOGLE_CLIENT_ID_REMOTING_HOST";
    env_cache_[8].variable_name = "GOOGLE_CLIENT_SECRET_REMOTING_HOST";
    env_cache_[9].variable_name = "GOOGLE_DEFAULT_CLIENT_ID";
    env_cache_[10].variable_name = "GOOGLE_DEFAULT_CLIENT_SECRET";
  }

  virtual void SetUp() {
    // Unset all environment variables that can affect these tests,
    // for the duration of the tests.
    for (size_t i = 0; i < arraysize(env_cache_); ++i) {
      EnvironmentCache& cache = env_cache_[i];
      cache.was_set = env_->HasVar(cache.variable_name);
      cache.value.clear();
      if (cache.was_set) {
        env_->GetVar(cache.variable_name, &cache.value);
        env_->UnSetVar(cache.variable_name);
      }
    }
  }

  virtual void TearDown() {
    // Restore environment.
    for (size_t i = 0; i < arraysize(env_cache_); ++i) {
      EnvironmentCache& cache = env_cache_[i];
      if (cache.was_set) {
        env_->SetVar(cache.variable_name, cache.value);
      }
    }
  }

 private:
  scoped_ptr<base::Environment> env_;

  // Why 3?  It is for GOOGLE_API_KEY, GOOGLE_DEFAULT_CLIENT_ID and
  // GOOGLE_DEFAULT_CLIENT_SECRET.
  //
  // Why 2 times CLIENT_NUM_ITEMS?  This is the number of different
  // clients in the OAuth2Client enumeration, and for each of these we
  // have both an ID and a secret.
  EnvironmentCache env_cache_[3 + 2 * google_apis::CLIENT_NUM_ITEMS];
};

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_OFFICIAL_GOOGLE_API_KEYS)
// Test official build behavior, since we are in a checkout where this
// is possible.
namespace official_build {

// We start every test by creating a clean environment for the
// preprocessor defines used in google_api_keys.cc
#undef DUMMY_API_TOKEN
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

// Try setting some keys, these should be ignored since it's a build
// with official keys.
#define GOOGLE_API_KEY "bogus api_key"
#define GOOGLE_CLIENT_ID_MAIN "bogus client_id_main"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#include "google_apis/google_api_keys.cc"

}  // namespace official_build

TEST_F(GoogleAPIKeysTest, OfficialKeys) {
  namespace testcase = official_build::google_apis;

  EXPECT_TRUE(testcase::HasKeysConfigured());

  std::string api_key = testcase::g_api_key_cache.Get().api_key();
  std::string id_main = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_MAIN);
  std::string secret_main = testcase::g_api_key_cache.Get().GetClientSecret(
      testcase::CLIENT_MAIN);
  std::string id_cloud_print =
      testcase::g_api_key_cache.Get().GetClientID(
          testcase::CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_CLOUD_PRINT);
  std::string id_remoting = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING);
  std::string secret_remoting =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING);
  std::string id_remoting_host = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING_HOST);

  EXPECT_NE(0u, api_key.size());
  EXPECT_NE(DUMMY_API_TOKEN, api_key);
  EXPECT_NE("bogus api_key", api_key);
  EXPECT_NE(kDummyToken, api_key);

  EXPECT_NE(0u, id_main.size());
  EXPECT_NE(DUMMY_API_TOKEN, id_main);
  EXPECT_NE("bogus client_id_main", id_main);
  EXPECT_NE(kDummyToken, id_main);

  EXPECT_NE(0u, secret_main.size());
  EXPECT_NE(DUMMY_API_TOKEN, secret_main);
  EXPECT_NE(kDummyToken, secret_main);

  EXPECT_NE(0u, id_cloud_print.size());
  EXPECT_NE(DUMMY_API_TOKEN, id_cloud_print);
  EXPECT_NE(kDummyToken, id_cloud_print);

  EXPECT_NE(0u, secret_cloud_print.size());
  EXPECT_NE(DUMMY_API_TOKEN, secret_cloud_print);
  EXPECT_NE(kDummyToken, secret_cloud_print);

  EXPECT_NE(0u, id_remoting.size());
  EXPECT_NE(DUMMY_API_TOKEN, id_remoting);
  EXPECT_NE(kDummyToken, id_remoting);

  EXPECT_NE(0u, secret_remoting.size());
  EXPECT_NE(DUMMY_API_TOKEN, secret_remoting);
  EXPECT_NE(kDummyToken, secret_remoting);

  EXPECT_NE(0u, id_remoting_host.size());
  EXPECT_NE(DUMMY_API_TOKEN, id_remoting_host);
  EXPECT_NE(kDummyToken, id_remoting_host);

  EXPECT_NE(0u, secret_remoting_host.size());
  EXPECT_NE(DUMMY_API_TOKEN, secret_remoting_host);
  EXPECT_NE(kDummyToken, secret_remoting_host);
}
#endif  // defined(GOOGLE_CHROME_BUILD) || defined(USE_OFFICIAL_GOOGLE_API_KEYS)

// After this test, for the remainder of this compilation unit, we
// need official keys to not be used.
#undef GOOGLE_CHROME_BUILD
#undef USE_OFFICIAL_GOOGLE_API_KEYS

// Test the set of keys temporarily baked into Chromium by default.
namespace default_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in google_api_keys.cc
#undef DUMMY_API_TOKEN
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#include "google_apis/google_api_keys.cc"

}  // namespace default_keys

TEST_F(GoogleAPIKeysTest, DefaultKeys) {
  namespace testcase = default_keys::google_apis;

  EXPECT_FALSE(testcase::HasKeysConfigured());

  std::string api_key = testcase::g_api_key_cache.Get().api_key();
  std::string id_main = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_MAIN);
  std::string secret_main = testcase::g_api_key_cache.Get().GetClientSecret(
      testcase::CLIENT_MAIN);
  std::string id_cloud_print =
      testcase::g_api_key_cache.Get().GetClientID(
          testcase::CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_CLOUD_PRINT);
  std::string id_remoting = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING);
  std::string secret_remoting =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING);
  std::string id_remoting_host = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING_HOST);

  EXPECT_EQ(kDummyToken, api_key);
  EXPECT_EQ(kDummyToken, id_main);
  EXPECT_EQ(kDummyToken, secret_main);
  EXPECT_EQ(kDummyToken, id_cloud_print);
  EXPECT_EQ(kDummyToken, secret_cloud_print);
  EXPECT_EQ(kDummyToken, id_remoting);
  EXPECT_EQ(kDummyToken, secret_remoting);
  EXPECT_EQ(kDummyToken, id_remoting_host);
  EXPECT_EQ(kDummyToken, secret_remoting_host);
}

// Override a couple of keys, leave the rest default.
namespace override_some_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in google_api_keys.cc
#undef DUMMY_API_TOKEN
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY override"
#define GOOGLE_CLIENT_ID_REMOTING "CLIENT_ID_REMOTING override"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#include "google_apis/google_api_keys.cc"

}  // namespace override_some_keys

TEST_F(GoogleAPIKeysTest, OverrideSomeKeys) {
  namespace testcase = override_some_keys::google_apis;

  EXPECT_FALSE(testcase::HasKeysConfigured());

  std::string api_key = testcase::g_api_key_cache.Get().api_key();
  std::string id_main = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_MAIN);
  std::string secret_main = testcase::g_api_key_cache.Get().GetClientSecret(
      testcase::CLIENT_MAIN);
  std::string id_cloud_print =
      testcase::g_api_key_cache.Get().GetClientID(
          testcase::CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_CLOUD_PRINT);
  std::string id_remoting = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING);
  std::string secret_remoting =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING);
  std::string id_remoting_host = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING_HOST);

  EXPECT_EQ("API_KEY override", api_key);
  EXPECT_EQ(kDummyToken, id_main);
  EXPECT_EQ(kDummyToken, secret_main);
  EXPECT_EQ(kDummyToken, id_cloud_print);
  EXPECT_EQ(kDummyToken, secret_cloud_print);
  EXPECT_EQ("CLIENT_ID_REMOTING override", id_remoting);
  EXPECT_EQ(kDummyToken, secret_remoting);
  EXPECT_EQ(kDummyToken, id_remoting_host);
  EXPECT_EQ(kDummyToken, secret_remoting_host);
}

// Override all keys.
namespace override_all_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in google_api_keys.cc
#undef DUMMY_API_TOKEN
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_CLOUD_PRINT "ID_CLOUD_PRINT"
#define GOOGLE_CLIENT_SECRET_CLOUD_PRINT "SECRET_CLOUD_PRINT"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#include "google_apis/google_api_keys.cc"

}  // namespace override_all_keys

TEST_F(GoogleAPIKeysTest, OverrideAllKeys) {
  namespace testcase = override_all_keys::google_apis;

  EXPECT_TRUE(testcase::HasKeysConfigured());

  std::string api_key = testcase::g_api_key_cache.Get().api_key();
  std::string id_main = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_MAIN);
  std::string secret_main = testcase::g_api_key_cache.Get().GetClientSecret(
      testcase::CLIENT_MAIN);
  std::string id_cloud_print =
      testcase::g_api_key_cache.Get().GetClientID(
          testcase::CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_CLOUD_PRINT);
  std::string id_remoting = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING);
  std::string secret_remoting =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING);
  std::string id_remoting_host = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING_HOST);

  EXPECT_EQ("API_KEY", api_key);
  EXPECT_EQ("ID_MAIN", id_main);
  EXPECT_EQ("SECRET_MAIN", secret_main);
  EXPECT_EQ("ID_CLOUD_PRINT", id_cloud_print);
  EXPECT_EQ("SECRET_CLOUD_PRINT", secret_cloud_print);
  EXPECT_EQ("ID_REMOTING", id_remoting);
  EXPECT_EQ("SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("SECRET_REMOTING_HOST", secret_remoting_host);
}

// Override all keys using both preprocessor defines and environment
// variables.  The environment variables should win.
namespace override_all_keys_env {

// We start every test by creating a clean environment for the
// preprocessor defines used in google_api_keys.cc
#undef DUMMY_API_TOKEN
#undef GOOGLE_API_KEY
#undef GOOGLE_CLIENT_ID_MAIN
#undef GOOGLE_CLIENT_SECRET_MAIN
#undef GOOGLE_CLIENT_ID_CLOUD_PRINT
#undef GOOGLE_CLIENT_SECRET_CLOUD_PRINT
#undef GOOGLE_CLIENT_ID_REMOTING
#undef GOOGLE_CLIENT_SECRET_REMOTING
#undef GOOGLE_CLIENT_ID_REMOTING_HOST
#undef GOOGLE_CLIENT_SECRET_REMOTING_HOST
#undef GOOGLE_DEFAULT_CLIENT_ID
#undef GOOGLE_DEFAULT_CLIENT_SECRET

#define GOOGLE_API_KEY "API_KEY"
#define GOOGLE_CLIENT_ID_MAIN "ID_MAIN"
#define GOOGLE_CLIENT_SECRET_MAIN "SECRET_MAIN"
#define GOOGLE_CLIENT_ID_CLOUD_PRINT "ID_CLOUD_PRINT"
#define GOOGLE_CLIENT_SECRET_CLOUD_PRINT "SECRET_CLOUD_PRINT"
#define GOOGLE_CLIENT_ID_REMOTING "ID_REMOTING"
#define GOOGLE_CLIENT_SECRET_REMOTING "SECRET_REMOTING"
#define GOOGLE_CLIENT_ID_REMOTING_HOST "ID_REMOTING_HOST"
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST "SECRET_REMOTING_HOST"

// Undef include guard so things get defined again, within this namespace.
#undef GOOGLE_APIS_GOOGLE_API_KEYS_H_
#undef GOOGLE_APIS_INTERNAL_GOOGLE_CHROME_API_KEYS_
#include "google_apis/google_api_keys.cc"

}  // namespace override_all_keys_env

TEST_F(GoogleAPIKeysTest, OverrideAllKeysUsingEnvironment) {
  namespace testcase = override_all_keys_env::google_apis;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("GOOGLE_API_KEY", "env-API_KEY");
  env->SetVar("GOOGLE_CLIENT_ID_MAIN", "env-ID_MAIN");
  env->SetVar("GOOGLE_CLIENT_ID_CLOUD_PRINT", "env-ID_CLOUD_PRINT");
  env->SetVar("GOOGLE_CLIENT_ID_REMOTING", "env-ID_REMOTING");
  env->SetVar("GOOGLE_CLIENT_ID_REMOTING_HOST", "env-ID_REMOTING_HOST");
  env->SetVar("GOOGLE_CLIENT_SECRET_MAIN", "env-SECRET_MAIN");
  env->SetVar("GOOGLE_CLIENT_SECRET_CLOUD_PRINT", "env-SECRET_CLOUD_PRINT");
  env->SetVar("GOOGLE_CLIENT_SECRET_REMOTING", "env-SECRET_REMOTING");
  env->SetVar("GOOGLE_CLIENT_SECRET_REMOTING_HOST", "env-SECRET_REMOTING_HOST");

  EXPECT_TRUE(testcase::HasKeysConfigured());

  // It's important that the first call to Get() only happen after the
  // environment variables have been set.
  std::string api_key = testcase::g_api_key_cache.Get().api_key();
  std::string id_main = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_MAIN);
  std::string secret_main = testcase::g_api_key_cache.Get().GetClientSecret(
      testcase::CLIENT_MAIN);
  std::string id_cloud_print =
      testcase::g_api_key_cache.Get().GetClientID(
          testcase::CLIENT_CLOUD_PRINT);
  std::string secret_cloud_print =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_CLOUD_PRINT);
  std::string id_remoting = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING);
  std::string secret_remoting =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING);
  std::string id_remoting_host = testcase::g_api_key_cache.Get().GetClientID(
      testcase::CLIENT_REMOTING_HOST);
  std::string secret_remoting_host =
      testcase::g_api_key_cache.Get().GetClientSecret(
          testcase::CLIENT_REMOTING_HOST);

  EXPECT_EQ("env-API_KEY", api_key);
  EXPECT_EQ("env-ID_MAIN", id_main);
  EXPECT_EQ("env-SECRET_MAIN", secret_main);
  EXPECT_EQ("env-ID_CLOUD_PRINT", id_cloud_print);
  EXPECT_EQ("env-SECRET_CLOUD_PRINT", secret_cloud_print);
  EXPECT_EQ("env-ID_REMOTING", id_remoting);
  EXPECT_EQ("env-SECRET_REMOTING", secret_remoting);
  EXPECT_EQ("env-ID_REMOTING_HOST", id_remoting_host);
  EXPECT_EQ("env-SECRET_REMOTING_HOST", secret_remoting_host);
}

#endif  // defined(OS_LINUX) || defined(OS_MACOSX)
