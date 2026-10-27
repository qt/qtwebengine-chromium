// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_connection_factory_posix.h"

#include <openssl/ssl.h>

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {

class TlsConnectionFactoryPosixTest : public ::testing::Test {
 protected:
  class MockClient : public TlsConnectionFactory::Client {
   public:
    MOCK_METHOD(void,
                OnAccepted,
                (TlsConnectionFactory * factory,
                 std::vector<uint8_t> der_x509_peer_cert,
                 std::unique_ptr<TlsConnection> connection),
                (override));
    MOCK_METHOD(void,
                OnConnected,
                (TlsConnectionFactory * factory,
                 std::vector<uint8_t> der_x509_peer_cert,
                 std::unique_ptr<TlsConnection> connection),
                (override));
    MOCK_METHOD(void,
                OnConnectionFailed,
                (TlsConnectionFactory * factory,
                 const IPEndpoint& remote_address),
                (override));
    MOCK_METHOD(void,
                OnError,
                (TlsConnectionFactory * factory, const Error& error),
                (override));
  };

  TlsConnectionFactoryPosixTest()
      : clock_(Clock::now()),
        task_runner_(clock_),
        factory_(client_, task_runner_, nullptr, &FakeClock::now) {
    factory_.EnsureInitialized();
    ssl_ = bssl::UniquePtr<SSL>(SSL_new(factory_.ssl_context_.get()));
    OSP_CHECK(ssl_);
  }

  bssl::UniquePtr<SSL_SESSION> CreateSession(uint32_t timeout_secs) {
    bssl::UniquePtr<SSL_SESSION> session(SSL_SESSION_new(GetSslCtx()));
    OSP_CHECK(session);
    SSL_SESSION_set_timeout(session.get(), timeout_secs);
    return session;
  }

  // Fixture helpers to access private factory members (since friendship is not
  // inherited by TEST_F subclasses)
  void SaveSession(const IPEndpoint& remote,
                   bssl::UniquePtr<SSL_SESSION> session) {
    factory_.SaveSession(remote, std::move(session));
  }

  bool LookupAndSetupSession(const IPEndpoint& remote_address, SSL* ssl) {
    return factory_.LookupAndSetupSession(remote_address, ssl);
  }

  size_t GetCacheSize() const { return factory_.sessions_.size(); }

  IPEndpoint GetFrontEndpoint() const {
    return factory_.sessions_.front().first;
  }

  IPEndpoint GetBackEndpoint() const { return factory_.sessions_.back().first; }

  SSL_CTX* GetSslCtx() { return factory_.ssl_context_.get(); }

  FakeClock clock_;
  FakeTaskRunner task_runner_;
  testing::StrictMock<MockClient> client_;
  TlsConnectionFactoryPosix factory_;
  bssl::UniquePtr<SSL> ssl_;
};

TEST_F(TlsConnectionFactoryPosixTest, InsertAndLookup) {
  IPEndpoint remote{{127, 0, 0, 1}, 1234};
  auto session = CreateSession(100);
  SSL_SESSION* session_ptr = session.get();

  SaveSession(remote, std::move(session));
  EXPECT_EQ(GetCacheSize(), 1u);

  // Lookup should succeed and setup the session
  EXPECT_TRUE(LookupAndSetupSession(remote, ssl_.get()));
  EXPECT_EQ(SSL_get_session(ssl_.get()), session_ptr);

  // Single-use: it should be removed immediately
  EXPECT_EQ(GetCacheSize(), 0u);

  // Second lookup should fail
  EXPECT_FALSE(LookupAndSetupSession(remote, ssl_.get()));
}

TEST_F(TlsConnectionFactoryPosixTest, Expiration) {
  IPEndpoint remote{{127, 0, 0, 1}, 1234};
  auto session = CreateSession(10);  // 10 seconds lifetime
  SSL_SESSION* session_ptr = session.get();

  SaveSession(remote, std::move(session));
  EXPECT_EQ(GetCacheSize(), 1u);

  // Advance clock by 5 seconds (not expired yet)
  clock_.Advance(std::chrono::seconds(5));

  // Lookup should succeed
  bssl::UniquePtr<SSL> ssl2(SSL_new(GetSslCtx()));
  EXPECT_TRUE(LookupAndSetupSession(remote, ssl2.get()));
  EXPECT_EQ(SSL_get_session(ssl2.get()), session_ptr);
  EXPECT_EQ(GetCacheSize(), 0u);  // Removed due to single-use

  // Re-insert
  session = CreateSession(10);
  session_ptr = session.get();
  SaveSession(remote, std::move(session));

  // Advance clock by 11 seconds (expired)
  clock_.Advance(std::chrono::seconds(11));

  // Lookup should fail and remove it
  bssl::UniquePtr<SSL> ssl3(SSL_new(GetSslCtx()));
  EXPECT_FALSE(LookupAndSetupSession(remote, ssl3.get()));
  EXPECT_EQ(GetCacheSize(), 0u);
}

TEST_F(TlsConnectionFactoryPosixTest, LRUEviction) {
  // Max size is 10 (kSslSessionCacheSize)
  for (int i = 0; i < 10; ++i) {
    IPEndpoint remote{{127, 0, 0, 1}, static_cast<uint16_t>(1000 + i)};
    SaveSession(remote, CreateSession(100));
  }
  EXPECT_EQ(GetCacheSize(), 10u);

  // Oldest is index 0
  IPEndpoint oldest_remote{{127, 0, 0, 1}, 1000};
  EXPECT_EQ(GetFrontEndpoint(), oldest_remote);

  // Insert 11th session, should evict oldest
  IPEndpoint newest_remote{{127, 0, 0, 1}, 1010};
  SaveSession(newest_remote, CreateSession(100));
  EXPECT_EQ(GetCacheSize(), 10u);

  // Verify oldest is gone
  EXPECT_FALSE(LookupAndSetupSession(oldest_remote, ssl_.get()));

  // Verify second oldest is now oldest
  IPEndpoint second_oldest_remote{{127, 0, 0, 1}, 1001};
  EXPECT_EQ(GetFrontEndpoint(), second_oldest_remote);
}

TEST_F(TlsConnectionFactoryPosixTest, LRUUpdateRecency) {
  for (int i = 0; i < 5; ++i) {
    IPEndpoint remote{{127, 0, 0, 1}, static_cast<uint16_t>(1000 + i)};
    SaveSession(remote, CreateSession(100));
  }
  EXPECT_EQ(GetCacheSize(), 5u);

  // Oldest is 1000
  IPEndpoint oldest_remote{{127, 0, 0, 1}, 1000};
  EXPECT_EQ(GetFrontEndpoint(), oldest_remote);

  // Re-insert 1000 to update its recency (move to back)
  auto session = CreateSession(100);
  SaveSession(oldest_remote, std::move(session));
  EXPECT_EQ(GetCacheSize(), 5u);
  EXPECT_EQ(GetBackEndpoint(), oldest_remote);

  // New oldest should be 1001
  IPEndpoint new_oldest_remote{{127, 0, 0, 1}, 1001};
  EXPECT_EQ(GetFrontEndpoint(), new_oldest_remote);
}

TEST_F(TlsConnectionFactoryPosixTest, CleanupExpiredOnInsert) {
  // Insert 5 sessions with 10s lifetime
  for (int i = 0; i < 5; ++i) {
    IPEndpoint remote{{127, 0, 0, 1}, static_cast<uint16_t>(1000 + i)};
    SaveSession(remote, CreateSession(10));
  }
  EXPECT_EQ(GetCacheSize(), 5u);

  // Advance clock past lifetime
  clock_.Advance(std::chrono::seconds(11));

  // Insert new session, should trigger cleanup and remove all expired
  IPEndpoint new_remote{{127, 0, 0, 1}, 2000};
  SaveSession(new_remote, CreateSession(100));

  // Only the new session should remain
  EXPECT_EQ(GetCacheSize(), 1u);
  EXPECT_EQ(GetFrontEndpoint(), new_remote);
}

}  // namespace openscreen
