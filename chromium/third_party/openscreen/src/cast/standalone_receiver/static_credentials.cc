// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/static_credentials.h"

#include <openssl/mem.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cast/standalone_receiver/private_key_der.h"
#include "platform/base/tls_credentials.h"
#include "util/crypto/certificate_utils.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr int kThreeDaysInSeconds = 3 * 24 * 60 * 60;
constexpr auto kCertificateDuration = std::chrono::seconds(kThreeDaysInSeconds);

}  // namespace

StaticCredentialsProvider::StaticCredentialsProvider() = default;
StaticCredentialsProvider::StaticCredentialsProvider(
    DeviceCredentials device_creds,
    std::vector<uint8_t> tls_cert_der)
    : device_creds(std::move(device_creds)),
      tls_cert_der(std::move(tls_cert_der)) {}

StaticCredentialsProvider::StaticCredentialsProvider(
    StaticCredentialsProvider&&) = default;
StaticCredentialsProvider& StaticCredentialsProvider::operator=(
    StaticCredentialsProvider&&) = default;
StaticCredentialsProvider::~StaticCredentialsProvider() = default;

ErrorOr<GeneratedCredentials> GenerateCredentials(
    absl::string_view device_certificate_id) {
  GeneratedCredentials credentials;

  bssl::UniquePtr<EVP_PKEY> root_key = GenerateRsaKeyPair();
  bssl::UniquePtr<EVP_PKEY> intermediate_key = GenerateRsaKeyPair();
  bssl::UniquePtr<EVP_PKEY> device_key = GenerateRsaKeyPair();
  OSP_CHECK(root_key);
  OSP_CHECK(intermediate_key);
  OSP_CHECK(device_key);

  ErrorOr<bssl::UniquePtr<X509>> root_cert_or_error =
      CreateSelfSignedX509Certificate("Cast Root CA", kCertificateDuration,
                                      *root_key, GetWallTimeSinceUnixEpoch(),
                                      true);
  OSP_CHECK(root_cert_or_error);
  bssl::UniquePtr<X509> root_cert = std::move(root_cert_or_error.value());

  ErrorOr<bssl::UniquePtr<X509>> intermediate_cert_or_error =
      CreateSelfSignedX509Certificate(
          "Cast Intermediate", kCertificateDuration, *intermediate_key,
          GetWallTimeSinceUnixEpoch(), true, root_cert.get(), root_key.get());
  OSP_CHECK(intermediate_cert_or_error);
  bssl::UniquePtr<X509> intermediate_cert =
      std::move(intermediate_cert_or_error.value());

  ErrorOr<bssl::UniquePtr<X509>> device_cert_or_error =
      CreateSelfSignedX509Certificate(
          device_certificate_id, kCertificateDuration, *device_key,
          GetWallTimeSinceUnixEpoch(), false, intermediate_cert.get(),
          intermediate_key.get());
  OSP_CHECK(device_cert_or_error);
  bssl::UniquePtr<X509> device_cert = std::move(device_cert_or_error.value());

  // NOTE: Device cert chain plumbing + serialization.
  DeviceCredentials device_creds;
  device_creds.private_key = std::move(device_key);

  int cert_length = i2d_X509(device_cert.get(), nullptr);
  std::string cert_serial(cert_length, 0);
  uint8_t* out = reinterpret_cast<uint8_t*>(&cert_serial[0]);
  i2d_X509(device_cert.get(), &out);
  device_creds.certs.emplace_back(std::move(cert_serial));

  cert_length = i2d_X509(intermediate_cert.get(), nullptr);
  cert_serial.resize(cert_length);
  out = reinterpret_cast<uint8_t*>(&cert_serial[0]);
  i2d_X509(intermediate_cert.get(), &out);
  device_creds.certs.emplace_back(std::move(cert_serial));

  cert_length = i2d_X509(root_cert.get(), nullptr);
  std::vector<uint8_t> trust_anchor_der(cert_length);
  out = &trust_anchor_der[0];
  i2d_X509(root_cert.get(), &out);

  // NOTE: TLS key pair + certificate generation.
  bssl::UniquePtr<EVP_PKEY> tls_key = GenerateRsaKeyPair();
  OSP_CHECK_EQ(EVP_PKEY_id(tls_key.get()), EVP_PKEY_RSA);
  ErrorOr<bssl::UniquePtr<X509>> tls_cert_or_error =
      CreateSelfSignedX509Certificate("Test Device TLS", kCertificateDuration,
                                      *tls_key, GetWallTimeSinceUnixEpoch());
  OSP_CHECK(tls_cert_or_error);
  bssl::UniquePtr<X509> tls_cert = std::move(tls_cert_or_error.value());

  // NOTE: TLS private key serialization.
  RSA* rsa_key = EVP_PKEY_get0_RSA(tls_key.get());
  size_t pkey_len = 0;
  uint8_t* pkey_bytes = nullptr;
  OSP_CHECK(RSA_private_key_to_bytes(&pkey_bytes, &pkey_len, rsa_key));
  OSP_CHECK_GT(pkey_len, 0u);
  std::vector<uint8_t> tls_key_serial(pkey_bytes, pkey_bytes + pkey_len);
  OPENSSL_free(pkey_bytes);

  // NOTE: TLS public key serialization.
  pkey_len = 0;
  pkey_bytes = nullptr;
  OSP_CHECK(RSA_public_key_to_bytes(&pkey_bytes, &pkey_len, rsa_key));
  OSP_CHECK_GT(pkey_len, 0u);
  std::vector<uint8_t> tls_pub_serial(pkey_bytes, pkey_bytes + pkey_len);
  OPENSSL_free(pkey_bytes);

  // NOTE: TLS cert serialization.
  cert_length = 0;
  cert_length = i2d_X509(tls_cert.get(), nullptr);
  OSP_CHECK_GT(cert_length, 0);
  std::vector<uint8_t> tls_cert_serial(cert_length);
  out = &tls_cert_serial[0];
  i2d_X509(tls_cert.get(), &out);

  return GeneratedCredentials{
      std::make_unique<StaticCredentialsProvider>(std::move(device_creds),
                                                  tls_cert_serial),
      TlsCredentials{std::move(tls_key_serial), std::move(tls_pub_serial),
                     std::move(tls_cert_serial)},
      std::move(trust_anchor_der)};
}

}  // namespace cast
}  // namespace openscreen
