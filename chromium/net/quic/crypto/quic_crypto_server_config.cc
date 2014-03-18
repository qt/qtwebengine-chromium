// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_server_config.h"

#include <stdlib.h>
#include <algorithm>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/hkdf.h"
#include "crypto/secure_hash.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/aes_128_gcm_12_decrypter.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/cert_compressor.h"
#include "net/quic/crypto/channel_id.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_server_config_protobuf.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/curve25519_key_exchange.h"
#include "net/quic/crypto/ephemeral_key_source.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/local_strike_register_client.h"
#include "net/quic/crypto/p256_key_exchange.h"
#include "net/quic/crypto/proof_source.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/crypto/source_address_token.h"
#include "net/quic/crypto/strike_register.h"
#include "net/quic/crypto/strike_register_client.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using crypto::SecureHash;
using std::map;
using std::string;
using std::vector;

namespace net {

// ClientHelloInfo contains information about a client hello message that is
// only kept for as long as it's being processed.
struct ClientHelloInfo {
  ClientHelloInfo(const IPEndPoint& in_client_ip, QuicWallTime in_now)
      : client_ip(in_client_ip),
        now(in_now),
        valid_source_address_token(false),
        client_nonce_well_formed(false),
        unique(false) {}

  // Inputs to EvaluateClientHello.
  const IPEndPoint client_ip;
  const QuicWallTime now;

  // Outputs from EvaluateClientHello.
  bool valid_source_address_token;
  bool client_nonce_well_formed;
  bool unique;
  StringPiece sni;
  StringPiece client_nonce;
  StringPiece server_nonce;
};

struct ValidateClientHelloResultCallback::Result {
  Result(const CryptoHandshakeMessage& in_client_hello,
         IPEndPoint in_client_ip,
         QuicWallTime in_now)
      : client_hello(in_client_hello),
        info(in_client_ip, in_now),
        error_code(QUIC_NO_ERROR) {
  }

  CryptoHandshakeMessage client_hello;
  ClientHelloInfo info;
  QuicErrorCode error_code;
  string error_details;
};

class ValidateClientHelloHelper {
 public:
  ValidateClientHelloHelper(ValidateClientHelloResultCallback::Result* result,
                            ValidateClientHelloResultCallback* done_cb)
      : result_(result), done_cb_(done_cb) {
  }

  ~ValidateClientHelloHelper() {
    if (done_cb_ != NULL) {
      LOG(DFATAL) <<
          "Deleting ValidateClientHelloHelper with a pending callback.";
    }
  }

  void ValidationComplete(QuicErrorCode error_code, const char* error_details) {
    result_->error_code = error_code;
    result_->error_details = error_details;
    done_cb_->Run(result_);
    DetachCallback();
  }

  void StartedAsyncCallback() {
    DetachCallback();
  }

 private:
  void DetachCallback() {
    if (done_cb_ == NULL) {
      LOG(DFATAL) << "Callback already detached.";
    }
    done_cb_ = NULL;
  }

  ValidateClientHelloResultCallback::Result* result_;
  ValidateClientHelloResultCallback* done_cb_;

  DISALLOW_COPY_AND_ASSIGN(ValidateClientHelloHelper);
};

class VerifyNonceIsValidAndUniqueCallback
    : public StrikeRegisterClient::ResultCallback {
 public:
  VerifyNonceIsValidAndUniqueCallback(
      ValidateClientHelloResultCallback::Result* result,
      ValidateClientHelloResultCallback* done_cb)
      : result_(result), done_cb_(done_cb) {
  }

 protected:
  virtual void RunImpl(bool nonce_is_valid_and_unique) OVERRIDE {
    DVLOG(1) << "Using client nonce, unique: " << nonce_is_valid_and_unique;
    result_->info.unique = nonce_is_valid_and_unique;
    done_cb_->Run(result_);
  }

 private:
  ValidateClientHelloResultCallback::Result* result_;
  ValidateClientHelloResultCallback* done_cb_;

  DISALLOW_COPY_AND_ASSIGN(VerifyNonceIsValidAndUniqueCallback);
};

// static
const char QuicCryptoServerConfig::TESTING[] = "secret string for testing";


ValidateClientHelloResultCallback::ValidateClientHelloResultCallback() {
}

ValidateClientHelloResultCallback::~ValidateClientHelloResultCallback() {
}

void ValidateClientHelloResultCallback::Run(const Result* result) {
  RunImpl(result->client_hello, *result);
  delete result;
  delete this;
}

QuicCryptoServerConfig::ConfigOptions::ConfigOptions()
    : expiry_time(QuicWallTime::Zero()),
      channel_id_enabled(false),
      p256(false) {}

QuicCryptoServerConfig::QuicCryptoServerConfig(
    StringPiece source_address_token_secret,
    QuicRandom* rand)
    : replay_protection_(true),
      configs_lock_(),
      primary_config_(NULL),
      next_config_promotion_time_(QuicWallTime::Zero()),
      server_nonce_strike_register_lock_(),
      strike_register_no_startup_period_(false),
      strike_register_max_entries_(1 << 10),
      strike_register_window_secs_(600),
      source_address_token_future_secs_(3600),
      source_address_token_lifetime_secs_(86400),
      server_nonce_strike_register_max_entries_(1 << 10),
      server_nonce_strike_register_window_secs_(120) {
  crypto::HKDF hkdf(source_address_token_secret, StringPiece() /* no salt */,
                    "QUIC source address token key",
                    CryptoSecretBoxer::GetKeySize(),
                    0 /* no fixed IV needed */);
  source_address_token_boxer_.SetKey(hkdf.server_write_key());

  // Generate a random key and orbit for server nonces.
  rand->RandBytes(server_nonce_orbit_, sizeof(server_nonce_orbit_));
  const size_t key_size = server_nonce_boxer_.GetKeySize();
  scoped_ptr<uint8[]> key_bytes(new uint8[key_size]);
  rand->RandBytes(key_bytes.get(), key_size);

  server_nonce_boxer_.SetKey(
      StringPiece(reinterpret_cast<char*>(key_bytes.get()), key_size));
}

QuicCryptoServerConfig::~QuicCryptoServerConfig() {
  primary_config_ = NULL;
}

// static
QuicServerConfigProtobuf* QuicCryptoServerConfig::DefaultConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const ConfigOptions& options) {
  CryptoHandshakeMessage msg;

  const string curve25519_private_key =
      Curve25519KeyExchange::NewPrivateKey(rand);
  scoped_ptr<Curve25519KeyExchange> curve25519(
      Curve25519KeyExchange::New(curve25519_private_key));
  StringPiece curve25519_public_value = curve25519->public_value();

  string encoded_public_values;
  // First three bytes encode the length of the public value.
  encoded_public_values.push_back(curve25519_public_value.size());
  encoded_public_values.push_back(curve25519_public_value.size() >> 8);
  encoded_public_values.push_back(curve25519_public_value.size() >> 16);
  encoded_public_values.append(curve25519_public_value.data(),
                               curve25519_public_value.size());

  string p256_private_key;
  if (options.p256) {
    p256_private_key = P256KeyExchange::NewPrivateKey();
    scoped_ptr<P256KeyExchange> p256(P256KeyExchange::New(p256_private_key));
    StringPiece p256_public_value = p256->public_value();

    encoded_public_values.push_back(p256_public_value.size());
    encoded_public_values.push_back(p256_public_value.size() >> 8);
    encoded_public_values.push_back(p256_public_value.size() >> 16);
    encoded_public_values.append(p256_public_value.data(),
                                 p256_public_value.size());
  }

  msg.set_tag(kSCFG);
  if (options.p256) {
    msg.SetTaglist(kKEXS, kC255, kP256, 0);
  } else {
    msg.SetTaglist(kKEXS, kC255, 0);
  }
  msg.SetTaglist(kAEAD, kAESG, 0);
  // TODO(rch): Remove once we remove QUIC_VERSION_12.
  msg.SetValue(kVERS, static_cast<uint16>(0));
  msg.SetStringPiece(kPUBS, encoded_public_values);

  if (options.expiry_time.IsZero()) {
    const QuicWallTime now = clock->WallNow();
    const QuicWallTime expiry = now.Add(QuicTime::Delta::FromSeconds(
        60 * 60 * 24 * 180 /* 180 days, ~six months */));
    const uint64 expiry_seconds = expiry.ToUNIXSeconds();
    msg.SetValue(kEXPY, expiry_seconds);
  } else {
    msg.SetValue(kEXPY, options.expiry_time.ToUNIXSeconds());
  }

  char orbit_bytes[kOrbitSize];
  if (options.orbit.size() == sizeof(orbit_bytes)) {
    memcpy(orbit_bytes, options.orbit.data(), sizeof(orbit_bytes));
  } else {
    DCHECK(options.orbit.empty());
    rand->RandBytes(orbit_bytes, sizeof(orbit_bytes));
  }
  msg.SetStringPiece(kORBT, StringPiece(orbit_bytes, sizeof(orbit_bytes)));

  if (options.channel_id_enabled) {
    msg.SetTaglist(kPDMD, kCHID, 0);
  }

  if (options.id.empty()) {
    // We need to ensure that the SCID changes whenever the server config does
    // thus we make it a hash of the rest of the server config.
    scoped_ptr<QuicData> serialized(
        CryptoFramer::ConstructHandshakeMessage(msg));
    scoped_ptr<SecureHash> hash(SecureHash::Create(SecureHash::SHA256));
    hash->Update(serialized->data(), serialized->length());

    char scid_bytes[16];
    hash->Finish(scid_bytes, sizeof(scid_bytes));
    msg.SetStringPiece(kSCID, StringPiece(scid_bytes, sizeof(scid_bytes)));
  } else {
    msg.SetStringPiece(kSCID, options.id);
  }
  // Don't put new tags below this point. The SCID generation should hash over
  // everything but itself and so extra tags should be added prior to the
  // preceeding if block.

  scoped_ptr<QuicData> serialized(CryptoFramer::ConstructHandshakeMessage(msg));

  scoped_ptr<QuicServerConfigProtobuf> config(new QuicServerConfigProtobuf);
  config->set_config(serialized->AsStringPiece());
  QuicServerConfigProtobuf::PrivateKey* curve25519_key = config->add_key();
  curve25519_key->set_tag(kC255);
  curve25519_key->set_private_key(curve25519_private_key);

  if (options.p256) {
    QuicServerConfigProtobuf::PrivateKey* p256_key = config->add_key();
    p256_key->set_tag(kP256);
    p256_key->set_private_key(p256_private_key);
  }

  return config.release();
}

CryptoHandshakeMessage* QuicCryptoServerConfig::AddConfig(
    QuicServerConfigProtobuf* protobuf,
    const QuicWallTime now) {
  scoped_ptr<CryptoHandshakeMessage> msg(
      CryptoFramer::ParseMessage(protobuf->config()));

  if (!msg.get()) {
    LOG(WARNING) << "Failed to parse server config message";
    return NULL;
  }

  scoped_refptr<Config> config(ParseConfigProtobuf(protobuf));
  if (!config.get()) {
    LOG(WARNING) << "Failed to parse server config message";
    return NULL;
  }

  {
    base::AutoLock locked(configs_lock_);
    if (configs_.find(config->id) != configs_.end()) {
      LOG(WARNING) << "Failed to add config because another with the same "
                      "server config id already exists: "
                   << base::HexEncode(config->id.data(), config->id.size());
      return NULL;
    }

    configs_[config->id] = config;
    SelectNewPrimaryConfig(now);
    DCHECK(primary_config_.get());
  }

  return msg.release();
}

CryptoHandshakeMessage* QuicCryptoServerConfig::AddDefaultConfig(
    QuicRandom* rand,
    const QuicClock* clock,
    const ConfigOptions& options) {
  scoped_ptr<QuicServerConfigProtobuf> config(
      DefaultConfig(rand, clock, options));
  return AddConfig(config.get(), clock->WallNow());
}

bool QuicCryptoServerConfig::SetConfigs(
    const vector<QuicServerConfigProtobuf*>& protobufs,
    const QuicWallTime now) {
  vector<scoped_refptr<Config> > new_configs;
  bool ok = true;

  for (vector<QuicServerConfigProtobuf*>::const_iterator i = protobufs.begin();
       i != protobufs.end(); ++i) {
    scoped_refptr<Config> config(ParseConfigProtobuf(*i));
    if (!config.get()) {
      ok = false;
      break;
    }
    new_configs.push_back(config);
  }

  if (!ok) {
    LOG(WARNING) << "Rejecting QUIC configs because of above errors";
  } else {
    base::AutoLock locked(configs_lock_);
    typedef ConfigMap::iterator ConfigMapIterator;
    vector<ConfigMapIterator> to_delete;

    DCHECK_EQ(protobufs.size(), new_configs.size());

    // First, look for any configs that have been removed.
    for (ConfigMapIterator i = configs_.begin();
         i != configs_.end(); ++i) {
      const scoped_refptr<Config> old_config = i->second;
      bool found = false;

      for (vector<scoped_refptr<Config> >::const_iterator j =
               new_configs.begin();
           j != new_configs.end(); ++j) {
        if ((*j)->id == old_config->id) {
          found = true;
          break;
        }
      }

      if (!found) {
        // We cannot remove the primary config. This has probably happened
        // because our source of config information failed for a time and we're
        // suddenly seeing a jump in time. No matter - we'll configure a new
        // primary config and then we'll be able to delete it next time.
        if (!old_config->is_primary) {
          to_delete.push_back(i);
        }
      }
    }

    for (vector<ConfigMapIterator>::const_iterator i = to_delete.begin();
         i != to_delete.end(); ++i) {
      configs_.erase(*i);
    }

    // Find any configs that need to be added.
    for (vector<scoped_refptr<Config> >::const_iterator i = new_configs.begin();
         i != new_configs.end(); ++i) {
      const scoped_refptr<Config> new_config = *i;
      if (configs_.find(new_config->id) != configs_.end()) {
        continue;
      }

      configs_[new_config->id] = new_config;
    }

    SelectNewPrimaryConfig(now);
  }

  return ok;
}

void QuicCryptoServerConfig::ValidateClientHello(
    const CryptoHandshakeMessage& client_hello,
    IPEndPoint client_ip,
    const QuicClock* clock,
    ValidateClientHelloResultCallback* done_cb) const {
  const QuicWallTime now(clock->WallNow());
  ValidateClientHelloResultCallback::Result* result =
      new ValidateClientHelloResultCallback::Result(
          client_hello, client_ip, now);

  uint8 primary_orbit[kOrbitSize];
  {
    base::AutoLock locked(configs_lock_);

    if (!primary_config_) {
      result->error_code = QUIC_CRYPTO_INTERNAL_ERROR;
      result->error_details = "No configurations loaded";
    } else {
      if (!next_config_promotion_time_.IsZero() &&
          next_config_promotion_time_.IsAfter(now)) {
        SelectNewPrimaryConfig(now);
      }

      memcpy(primary_orbit, primary_config_->orbit, sizeof(primary_orbit));
    }
  }

  if (result->error_code == QUIC_NO_ERROR) {
    EvaluateClientHello(primary_orbit, result, done_cb);
  } else {
    done_cb->Run(result);
  }
}

QuicErrorCode QuicCryptoServerConfig::ProcessClientHello(
    const ValidateClientHelloResultCallback::Result& validate_chlo_result,
    QuicGuid guid,
    IPEndPoint client_ip,
    QuicVersion version,
    const QuicVersionVector& supported_versions,
    const QuicClock* clock,
    QuicRandom* rand,
    QuicCryptoNegotiatedParameters *params,
    CryptoHandshakeMessage* out,
    string* error_details) const {
  DCHECK(error_details);

  const CryptoHandshakeMessage& client_hello =
      validate_chlo_result.client_hello;
  const ClientHelloInfo& info = validate_chlo_result.info;

  // If the client's preferred version is not the version we are currently
  // speaking, then the client went through a version negotiation.  In this
  // case, we need to make sure that we actually do not support this version
  // and that it wasn't a downgrade attack.
  QuicTag client_version_tag;
  // TODO(rch): Make this check mandatory when we remove QUIC_VERSION_12.
  if (client_hello.GetUint32(kVER, &client_version_tag) == QUIC_NO_ERROR) {
    QuicVersion client_version = QuicTagToQuicVersion(client_version_tag);
    if (client_version != version) {
      // Just because client_version is a valid version enum doesn't mean that
      // this server actually supports that version, so we check to see if
      // it's actually in the supported versions list.
      for (size_t i = 0; i < supported_versions.size(); ++i) {
        if (client_version == supported_versions[i]) {
          *error_details = "Downgrade attack detected";
          return QUIC_VERSION_NEGOTIATION_MISMATCH;
        }
      }
    }
  }

  StringPiece requested_scid;
  client_hello.GetStringPiece(kSCID, &requested_scid);
  const QuicWallTime now(clock->WallNow());

  scoped_refptr<Config> requested_config;
  scoped_refptr<Config> primary_config;
  {
    base::AutoLock locked(configs_lock_);

    if (!primary_config_.get()) {
      *error_details = "No configurations loaded";
      return QUIC_CRYPTO_INTERNAL_ERROR;
    }

    if (!next_config_promotion_time_.IsZero() &&
        next_config_promotion_time_.IsAfter(now)) {
      SelectNewPrimaryConfig(now);
    }

    primary_config = primary_config_;

    if (!requested_scid.empty()) {
      ConfigMap::const_iterator it = configs_.find(requested_scid.as_string());
      if (it != configs_.end()) {
        // We'll use the config that the client requested in order to do
        // key-agreement. Otherwise we'll give it a copy of |primary_config_|
        // to use.
        requested_config = it->second;
      }
    }
  }

  if (validate_chlo_result.error_code != QUIC_NO_ERROR) {
    *error_details = validate_chlo_result.error_details;
    return validate_chlo_result.error_code;
  }

  out->Clear();

  if (!info.valid_source_address_token ||
      !info.client_nonce_well_formed ||
      !info.unique ||
      !requested_config.get()) {
    BuildRejection(primary_config.get(), client_hello, info, rand, out);
    return QUIC_NO_ERROR;
  }

  const QuicTag* their_aeads;
  const QuicTag* their_key_exchanges;
  size_t num_their_aeads, num_their_key_exchanges;
  if (client_hello.GetTaglist(kAEAD, &their_aeads,
                              &num_their_aeads) != QUIC_NO_ERROR ||
      client_hello.GetTaglist(kKEXS, &their_key_exchanges,
                              &num_their_key_exchanges) != QUIC_NO_ERROR ||
      num_their_aeads != 1 ||
      num_their_key_exchanges != 1) {
    *error_details = "Missing or invalid AEAD or KEXS";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  size_t key_exchange_index;
  if (!QuicUtils::FindMutualTag(requested_config->aead, their_aeads,
                                num_their_aeads, QuicUtils::LOCAL_PRIORITY,
                                &params->aead, NULL) ||
      !QuicUtils::FindMutualTag(
          requested_config->kexs, their_key_exchanges, num_their_key_exchanges,
          QuicUtils::LOCAL_PRIORITY, &params->key_exchange,
          &key_exchange_index)) {
    *error_details = "Unsupported AEAD or KEXS";
    return QUIC_CRYPTO_NO_SUPPORT;
  }

  StringPiece public_value;
  if (!client_hello.GetStringPiece(kPUBS, &public_value)) {
    *error_details = "Missing public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  const KeyExchange* key_exchange =
      requested_config->key_exchanges[key_exchange_index];
  if (!key_exchange->CalculateSharedKey(public_value,
                                        &params->initial_premaster_secret)) {
    *error_details = "Invalid public value";
    return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (!info.sni.empty()) {
    scoped_ptr<char[]> sni_tmp(new char[info.sni.length() + 1]);
    memcpy(sni_tmp.get(), info.sni.data(), info.sni.length());
    sni_tmp[info.sni.length()] = 0;
    params->sni = CryptoUtils::NormalizeHostname(sni_tmp.get());
  }

  string hkdf_suffix;
  const QuicData& client_hello_serialized = client_hello.GetSerialized();
  hkdf_suffix.reserve(sizeof(guid) + client_hello_serialized.length() +
                      requested_config->serialized.size());
  hkdf_suffix.append(reinterpret_cast<char*>(&guid), sizeof(guid));
  hkdf_suffix.append(client_hello_serialized.data(),
                     client_hello_serialized.length());
  hkdf_suffix.append(requested_config->serialized);

  StringPiece cetv_ciphertext;
  if (requested_config->channel_id_enabled &&
      client_hello.GetStringPiece(kCETV, &cetv_ciphertext)) {
    CryptoHandshakeMessage client_hello_copy(client_hello);
    client_hello_copy.Erase(kCETV);
    client_hello_copy.Erase(kPAD);

    const QuicData& client_hello_serialized = client_hello_copy.GetSerialized();
    string hkdf_input;
    hkdf_input.append(QuicCryptoConfig::kCETVLabel,
                      strlen(QuicCryptoConfig::kCETVLabel) + 1);
    hkdf_input.append(reinterpret_cast<char*>(&guid), sizeof(guid));
    hkdf_input.append(client_hello_serialized.data(),
                      client_hello_serialized.length());
    hkdf_input.append(requested_config->serialized);

    CrypterPair crypters;
    if (!CryptoUtils::DeriveKeys(params->initial_premaster_secret, params->aead,
                                 info.client_nonce, info.server_nonce,
                                 hkdf_input, CryptoUtils::SERVER, &crypters)) {
      *error_details = "Symmetric key setup failed";
      return QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED;
    }

    scoped_ptr<QuicData> cetv_plaintext(crypters.decrypter->DecryptPacket(
        0 /* sequence number */, StringPiece() /* associated data */,
        cetv_ciphertext));
    if (!cetv_plaintext.get()) {
      *error_details = "CETV decryption failure";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    scoped_ptr<CryptoHandshakeMessage> cetv(CryptoFramer::ParseMessage(
        cetv_plaintext->AsStringPiece()));
    if (!cetv.get()) {
      *error_details = "CETV parse error";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    StringPiece key, signature;
    if (cetv->GetStringPiece(kCIDK, &key) &&
        cetv->GetStringPiece(kCIDS, &signature)) {
      if (!ChannelIDVerifier::Verify(key, hkdf_input, signature)) {
        *error_details = "ChannelID signature failure";
        return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
      }

      params->channel_id = key.as_string();
    }
  }

  string hkdf_input;
  size_t label_len = strlen(QuicCryptoConfig::kInitialLabel) + 1;
  hkdf_input.reserve(label_len + hkdf_suffix.size());
  hkdf_input.append(QuicCryptoConfig::kInitialLabel, label_len);
  hkdf_input.append(hkdf_suffix);

  if (!CryptoUtils::DeriveKeys(params->initial_premaster_secret, params->aead,
                               info.client_nonce, info.server_nonce, hkdf_input,
                               CryptoUtils::SERVER,
                               &params->initial_crypters)) {
    *error_details = "Symmetric key setup failed";
    return QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED;
  }

  string forward_secure_public_value;
  if (ephemeral_key_source_.get()) {
    params->forward_secure_premaster_secret =
        ephemeral_key_source_->CalculateForwardSecureKey(
            key_exchange, rand, clock->ApproximateNow(), public_value,
            &forward_secure_public_value);
  } else {
    scoped_ptr<KeyExchange> forward_secure_key_exchange(
        key_exchange->NewKeyPair(rand));
    forward_secure_public_value =
        forward_secure_key_exchange->public_value().as_string();
    if (!forward_secure_key_exchange->CalculateSharedKey(
            public_value, &params->forward_secure_premaster_secret)) {
      *error_details = "Invalid public value";
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }
  }

  string forward_secure_hkdf_input;
  label_len = strlen(QuicCryptoConfig::kForwardSecureLabel) + 1;
  forward_secure_hkdf_input.reserve(label_len + hkdf_suffix.size());
  forward_secure_hkdf_input.append(QuicCryptoConfig::kForwardSecureLabel,
                                   label_len);
  forward_secure_hkdf_input.append(hkdf_suffix);

  if (!CryptoUtils::DeriveKeys(
           params->forward_secure_premaster_secret, params->aead,
           info.client_nonce, info.server_nonce, forward_secure_hkdf_input,
           CryptoUtils::SERVER, &params->forward_secure_crypters)) {
    *error_details = "Symmetric key setup failed";
    return QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED;
  }

  out->set_tag(kSHLO);
  QuicTagVector supported_version_tags;
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    supported_version_tags.push_back
        (QuicVersionToQuicTag(supported_versions[i]));
  }
  out->SetVector(kVER, supported_version_tags);
  out->SetStringPiece(kSourceAddressTokenTag,
                      NewSourceAddressToken(client_ip, rand, info.now));
  out->SetStringPiece(kPUBS, forward_secure_public_value);
  return QUIC_NO_ERROR;
}

// ConfigPrimaryTimeLessThan is a comparator that implements "less than" for
// Config's based on their primary_time.
// static
bool QuicCryptoServerConfig::ConfigPrimaryTimeLessThan(
    const scoped_refptr<Config>& a,
    const scoped_refptr<Config>& b) {
  return a->primary_time.IsBefore(b->primary_time);
}

void QuicCryptoServerConfig::SelectNewPrimaryConfig(
    const QuicWallTime now) const {
  vector<scoped_refptr<Config> > configs;
  configs.reserve(configs_.size());
  scoped_refptr<Config> first_config = NULL;

  for (ConfigMap::const_iterator it = configs_.begin();
       it != configs_.end(); ++it) {
    const scoped_refptr<Config> config(it->second);
    if (!first_config.get()) {
      first_config = config;
    }
    if (config->primary_time.IsZero()) {
      continue;
    }
    configs.push_back(it->second);
  }

  if (configs.empty()) {
    // Tests don't set |primary_time_|. For that case we promote the first
    // Config and leave it as primary forever.
    if (!primary_config_.get() && first_config.get()) {
      primary_config_ = first_config;
      primary_config_->is_primary = true;
    }
    return;
  }

  std::sort(configs.begin(), configs.end(), ConfigPrimaryTimeLessThan);

  for (size_t i = 0; i < configs.size(); ++i) {
    const scoped_refptr<Config> config(configs[i]);

    if (!config->primary_time.IsAfter(now)) {
      continue;
    }

    // This is the first config with a primary_time in the future. Thus the
    // previous Config should be the primary and this one should determine the
    // next_config_promotion_time_.
    scoped_refptr<Config> new_primary;
    if (i == 0) {
      // There was no previous Config, so this will have to be primary.
      new_primary = config;

      // We need the primary_time of the next config.
      if (configs.size() > 1) {
        next_config_promotion_time_ = configs[1]->primary_time;
      } else {
        next_config_promotion_time_ = QuicWallTime::Zero();
      }
    } else {
      new_primary = configs[i - 1];
      next_config_promotion_time_ = config->primary_time;
    }

    if (primary_config_.get()) {
      primary_config_->is_primary = false;
    }
    primary_config_ = new_primary;
    new_primary->is_primary = true;

    return;
  }

  // All config's primary times are in the past. We should make the most recent
  // primary.
  scoped_refptr<Config> new_primary = configs[configs.size() - 1];
  if (primary_config_.get()) {
    primary_config_->is_primary = false;
  }
  primary_config_ = new_primary;
  new_primary->is_primary = true;
  next_config_promotion_time_ = QuicWallTime::Zero();
}

void QuicCryptoServerConfig::EvaluateClientHello(
    const uint8* primary_orbit,
    ValidateClientHelloResultCallback::Result* client_hello_state,
    ValidateClientHelloResultCallback* done_cb) const {
  ValidateClientHelloHelper helper(client_hello_state, done_cb);

  const CryptoHandshakeMessage& client_hello =
      client_hello_state->client_hello;
  ClientHelloInfo* info = &(client_hello_state->info);

  if (client_hello.size() < kClientHelloMinimumSizeOld) {
    helper.ValidationComplete(QUIC_CRYPTO_INVALID_VALUE_LENGTH,
                              "Client hello too small");
    return;
  }

  if (client_hello.GetStringPiece(kSNI, &info->sni) &&
      !CryptoUtils::IsValidSNI(info->sni)) {
    helper.ValidationComplete(QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER,
                              "Invalid SNI name");
    return;
  }

  StringPiece srct;
  if (client_hello.GetStringPiece(kSourceAddressTokenTag, &srct) &&
      ValidateSourceAddressToken(srct, info->client_ip, info->now)) {
    info->valid_source_address_token = true;
  } else {
    // No valid source address token.
    helper.ValidationComplete(QUIC_NO_ERROR, "");
    return;
  }

  if (client_hello.GetStringPiece(kNONC, &info->client_nonce) &&
      info->client_nonce.size() == kNonceSize) {
    info->client_nonce_well_formed = true;
  } else {
    // Invalid client nonce.
    DVLOG(1) << "Invalid client nonce.";
    helper.ValidationComplete(QUIC_NO_ERROR, "");
    return;
  }

  if (!replay_protection_) {
    info->unique = true;
    DVLOG(1) << "No replay protection.";
    helper.ValidationComplete(QUIC_NO_ERROR, "");
    return;
  }

  client_hello.GetStringPiece(kServerNonceTag, &info->server_nonce);
  if (!info->server_nonce.empty()) {
    // If the server nonce is present, use it to establish uniqueness.
    info->unique = ValidateServerNonce(info->server_nonce, info->now);
    DVLOG(1) << "Using server nonce, unique: " << info->unique;
    helper.ValidationComplete(QUIC_NO_ERROR, "");
    return;
  }

  // Use the client nonce to establish uniqueness.
  base::AutoLock locked(strike_register_client_lock_);

  if (strike_register_client_.get() == NULL) {
    strike_register_client_.reset(new LocalStrikeRegisterClient(
        strike_register_max_entries_,
        static_cast<uint32>(info->now.ToUNIXSeconds()),
        strike_register_window_secs_,
        primary_orbit,
        strike_register_no_startup_period_ ?
        StrikeRegister::NO_STARTUP_PERIOD_NEEDED :
        StrikeRegister::DENY_REQUESTS_AT_STARTUP));
  }

  strike_register_client_->VerifyNonceIsValidAndUnique(
      info->client_nonce,
      info->now,
      new VerifyNonceIsValidAndUniqueCallback(client_hello_state, done_cb));
  helper.StartedAsyncCallback();
}

void QuicCryptoServerConfig::BuildRejection(
    const scoped_refptr<Config>& config,
    const CryptoHandshakeMessage& client_hello,
    const ClientHelloInfo& info,
    QuicRandom* rand,
    CryptoHandshakeMessage* out) const {
  out->set_tag(kREJ);
  out->SetStringPiece(kSCFG, config->serialized);
  out->SetStringPiece(kSourceAddressTokenTag,
                      NewSourceAddressToken(info.client_ip, rand, info.now));
  if (replay_protection_) {
    out->SetStringPiece(kServerNonceTag, NewServerNonce(rand, info.now));
  }

  // The client may have requested a certificate chain.
  const QuicTag* their_proof_demands;
  size_t num_their_proof_demands;

  if (proof_source_.get() == NULL ||
      client_hello.GetTaglist(kPDMD, &their_proof_demands,
                              &num_their_proof_demands) !=
          QUIC_NO_ERROR) {
    return;
  }

  bool x509_supported = false, x509_ecdsa_supported = false;
  for (size_t i = 0; i < num_their_proof_demands; i++) {
    switch (their_proof_demands[i]) {
      case kX509:
        x509_supported = true;
        x509_ecdsa_supported = true;
        break;
      case kX59R:
        x509_supported = true;
        break;
    }
  }

  if (!x509_supported) {
    return;
  }

  const vector<string>* certs;
  string signature;
  if (!proof_source_->GetProof(info.sni.as_string(), config->serialized,
                               x509_ecdsa_supported, &certs, &signature)) {
    return;
  }

  StringPiece their_common_set_hashes;
  StringPiece their_cached_cert_hashes;
  client_hello.GetStringPiece(kCCS, &their_common_set_hashes);
  client_hello.GetStringPiece(kCCRT, &their_cached_cert_hashes);

  const string compressed = CertCompressor::CompressChain(
      *certs, their_common_set_hashes, their_cached_cert_hashes,
      config->common_cert_sets);

  // kREJOverheadBytes is a very rough estimate of how much of a REJ
  // message is taken up by things other than the certificates.
  // STK: 56 bytes
  // SNO: 56 bytes
  // SCFG
  //   SCID: 16 bytes
  //   PUBS: 38 bytes
  const size_t kREJOverheadBytes = 166;
  // kMultiplier is the multiple of the CHLO message size that a REJ message
  // must stay under when the client doesn't present a valid source-address
  // token.
  const size_t kMultiplier = 2;
  // max_unverified_size is the number of bytes that the certificate chain
  // and signature can consume before we will demand a valid source-address
  // token.
  const size_t max_unverified_size =
      client_hello.size() * kMultiplier - kREJOverheadBytes;
  COMPILE_ASSERT(kClientHelloMinimumSizeOld * kMultiplier >= kREJOverheadBytes,
                 overhead_calculation_may_underflow);
  if (info.valid_source_address_token ||
      signature.size() + compressed.size() < max_unverified_size) {
    out->SetStringPiece(kCertificateTag, compressed);
    out->SetStringPiece(kPROF, signature);
  }
}

scoped_refptr<QuicCryptoServerConfig::Config>
QuicCryptoServerConfig::ParseConfigProtobuf(
    QuicServerConfigProtobuf* protobuf) {
  scoped_ptr<CryptoHandshakeMessage> msg(
      CryptoFramer::ParseMessage(protobuf->config()));

  if (msg->tag() != kSCFG) {
    LOG(WARNING) << "Server config message has tag " << msg->tag()
                 << " expected " << kSCFG;
    return NULL;
  }

  scoped_refptr<Config> config(new Config);
  config->serialized = protobuf->config();

  if (protobuf->has_primary_time()) {
    config->primary_time =
        QuicWallTime::FromUNIXSeconds(protobuf->primary_time());
  }

  StringPiece scid;
  if (!msg->GetStringPiece(kSCID, &scid)) {
    LOG(WARNING) << "Server config message is missing SCID";
    return NULL;
  }
  config->id = scid.as_string();

  const QuicTag* aead_tags;
  size_t aead_len;
  if (msg->GetTaglist(kAEAD, &aead_tags, &aead_len) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing AEAD";
    return NULL;
  }
  config->aead = vector<QuicTag>(aead_tags, aead_tags + aead_len);

  const QuicTag* kexs_tags;
  size_t kexs_len;
  if (msg->GetTaglist(kKEXS, &kexs_tags, &kexs_len) != QUIC_NO_ERROR) {
    LOG(WARNING) << "Server config message is missing KEXS";
    return NULL;
  }

  StringPiece orbit;
  if (!msg->GetStringPiece(kORBT, &orbit)) {
    LOG(WARNING) << "Server config message is missing OBIT";
    return NULL;
  }

  if (orbit.size() != kOrbitSize) {
    LOG(WARNING) << "Orbit value in server config is the wrong length."
                    " Got " << orbit.size() << " want " << kOrbitSize;
    return NULL;
  }
  COMPILE_ASSERT(sizeof(config->orbit) == kOrbitSize, orbit_incorrect_size);
  memcpy(config->orbit, orbit.data(), sizeof(config->orbit));

  {
    base::AutoLock locked(strike_register_client_lock_);
    if (strike_register_client_.get()) {
      const string& orbit = strike_register_client_->orbit();
      if (0 != memcmp(orbit.data(), config->orbit, kOrbitSize)) {
        LOG(WARNING)
            << "Server config has different orbit than current config. "
               "Switching orbits at run-time is not supported.";
        return NULL;
      }
    }
  }

  if (kexs_len != protobuf->key_size()) {
    LOG(WARNING) << "Server config has " << kexs_len
                 << " key exchange methods configured, but "
                 << protobuf->key_size() << " private keys";
    return NULL;
  }

  const QuicTag* proof_demand_tags;
  size_t num_proof_demand_tags;
  if (msg->GetTaglist(kPDMD, &proof_demand_tags, &num_proof_demand_tags) ==
      QUIC_NO_ERROR) {
    for (size_t i = 0; i < num_proof_demand_tags; i++) {
      if (proof_demand_tags[i] == kCHID) {
        config->channel_id_enabled = true;
        break;
      }
    }
  }

  for (size_t i = 0; i < kexs_len; i++) {
    const QuicTag tag = kexs_tags[i];
    string private_key;

    config->kexs.push_back(tag);

    for (size_t j = 0; j < protobuf->key_size(); j++) {
      const QuicServerConfigProtobuf::PrivateKey& key = protobuf->key(i);
      if (key.tag() == tag) {
        private_key = key.private_key();
        break;
      }
    }

    if (private_key.empty()) {
      LOG(WARNING) << "Server config contains key exchange method without "
                      "corresponding private key: " << tag;
      return NULL;
    }

    scoped_ptr<KeyExchange> ka;
    switch (tag) {
      case kC255:
        ka.reset(Curve25519KeyExchange::New(private_key));
        if (!ka.get()) {
          LOG(WARNING) << "Server config contained an invalid curve25519"
                          " private key.";
          return NULL;
        }
        break;
      case kP256:
        ka.reset(P256KeyExchange::New(private_key));
        if (!ka.get()) {
          LOG(WARNING) << "Server config contained an invalid P-256"
                          " private key.";
          return NULL;
        }
        break;
      default:
        LOG(WARNING) << "Server config message contains unknown key exchange "
                        "method: " << tag;
        return NULL;
    }

    for (vector<KeyExchange*>::const_iterator i = config->key_exchanges.begin();
         i != config->key_exchanges.end(); ++i) {
      if ((*i)->tag() == tag) {
        LOG(WARNING) << "Duplicate key exchange in config: " << tag;
        return NULL;
      }
    }

    config->key_exchanges.push_back(ka.release());
  }

  return config;
}

void QuicCryptoServerConfig::SetProofSource(ProofSource* proof_source) {
  proof_source_.reset(proof_source);
}

void QuicCryptoServerConfig::SetEphemeralKeySource(
    EphemeralKeySource* ephemeral_key_source) {
  ephemeral_key_source_.reset(ephemeral_key_source);
}

void QuicCryptoServerConfig::SetStrikeRegisterClient(
    StrikeRegisterClient* strike_register_client) {
  base::AutoLock locker(strike_register_client_lock_);
  DCHECK(!strike_register_client_.get());
  strike_register_client_.reset(strike_register_client);
}

void QuicCryptoServerConfig::set_replay_protection(bool on) {
  replay_protection_ = on;
}

void QuicCryptoServerConfig::set_strike_register_no_startup_period() {
  base::AutoLock locker(strike_register_client_lock_);
  DCHECK(!strike_register_client_.get());
  strike_register_no_startup_period_ = true;
}

void QuicCryptoServerConfig::set_strike_register_max_entries(
    uint32 max_entries) {
  base::AutoLock locker(strike_register_client_lock_);
  DCHECK(!strike_register_client_.get());
  strike_register_max_entries_ = max_entries;
}

void QuicCryptoServerConfig::set_strike_register_window_secs(
    uint32 window_secs) {
  base::AutoLock locker(strike_register_client_lock_);
  DCHECK(!strike_register_client_.get());
  strike_register_window_secs_ = window_secs;
}

void QuicCryptoServerConfig::set_source_address_token_future_secs(
    uint32 future_secs) {
  source_address_token_future_secs_ = future_secs;
}

void QuicCryptoServerConfig::set_source_address_token_lifetime_secs(
    uint32 lifetime_secs) {
  source_address_token_lifetime_secs_ = lifetime_secs;
}

void QuicCryptoServerConfig::set_server_nonce_strike_register_max_entries(
    uint32 max_entries) {
  DCHECK(!server_nonce_strike_register_.get());
  server_nonce_strike_register_max_entries_ = max_entries;
}

void QuicCryptoServerConfig::set_server_nonce_strike_register_window_secs(
    uint32 window_secs) {
  DCHECK(!server_nonce_strike_register_.get());
  server_nonce_strike_register_window_secs_ = window_secs;
}

string QuicCryptoServerConfig::NewSourceAddressToken(
    const IPEndPoint& ip,
    QuicRandom* rand,
    QuicWallTime now) const {
  SourceAddressToken source_address_token;
  source_address_token.set_ip(IPAddressToPackedString(ip.address()));
  source_address_token.set_timestamp(now.ToUNIXSeconds());

  return source_address_token_boxer_.Box(
      rand, source_address_token.SerializeAsString());
}

bool QuicCryptoServerConfig::ValidateSourceAddressToken(
    StringPiece token,
    const IPEndPoint& ip,
    QuicWallTime now) const {
  string storage;
  StringPiece plaintext;
  if (!source_address_token_boxer_.Unbox(token, &storage, &plaintext)) {
    return false;
  }

  SourceAddressToken source_address_token;
  if (!source_address_token.ParseFromArray(plaintext.data(),
                                           plaintext.size())) {
    return false;
  }

  if (source_address_token.ip() != IPAddressToPackedString(ip.address())) {
    // It's for a different IP address.
    return false;
  }

  const QuicWallTime timestamp(
      QuicWallTime::FromUNIXSeconds(source_address_token.timestamp()));
  const QuicTime::Delta delta(now.AbsoluteDifference(timestamp));

  if (now.IsBefore(timestamp) &&
      delta.ToSeconds() > source_address_token_future_secs_) {
    return false;
  }

  if (now.IsAfter(timestamp) &&
      delta.ToSeconds() > source_address_token_lifetime_secs_) {
    return false;
  }

  return true;
}

// kServerNoncePlaintextSize is the number of bytes in an unencrypted server
// nonce.
static const size_t kServerNoncePlaintextSize =
    4 /* timestamp */ + 20 /* random bytes */;

string QuicCryptoServerConfig::NewServerNonce(QuicRandom* rand,
                                              QuicWallTime now) const {
  const uint32 timestamp = static_cast<uint32>(now.ToUNIXSeconds());

  uint8 server_nonce[kServerNoncePlaintextSize];
  COMPILE_ASSERT(sizeof(server_nonce) > sizeof(timestamp), nonce_too_small);
  server_nonce[0] = static_cast<uint8>(timestamp >> 24);
  server_nonce[1] = static_cast<uint8>(timestamp >> 16);
  server_nonce[2] = static_cast<uint8>(timestamp >> 8);
  server_nonce[3] = static_cast<uint8>(timestamp);
  rand->RandBytes(&server_nonce[sizeof(timestamp)],
                  sizeof(server_nonce) - sizeof(timestamp));

  return server_nonce_boxer_.Box(
      rand,
      StringPiece(reinterpret_cast<char*>(server_nonce), sizeof(server_nonce)));
}

bool QuicCryptoServerConfig::ValidateServerNonce(StringPiece token,
                                                 QuicWallTime now) const {
  string storage;
  StringPiece plaintext;
  if (!server_nonce_boxer_.Unbox(token, &storage, &plaintext)) {
    return false;
  }

  // plaintext contains:
  //   uint32 timestamp
  //   uint8[20] random bytes

  if (plaintext.size() != kServerNoncePlaintextSize) {
    // This should never happen because the value decrypted correctly.
    LOG(DFATAL) << "Seemingly valid server nonce had incorrect length.";
    return false;
  }

  uint8 server_nonce[32];
  memcpy(server_nonce, plaintext.data(), 4);
  memcpy(server_nonce + 4, server_nonce_orbit_, sizeof(server_nonce_orbit_));
  memcpy(server_nonce + 4 + sizeof(server_nonce_orbit_), plaintext.data() + 4,
         20);
  COMPILE_ASSERT(4 + sizeof(server_nonce_orbit_) + 20 == sizeof(server_nonce),
                 bad_nonce_buffer_length);

  bool is_unique;
  {
    base::AutoLock auto_lock(server_nonce_strike_register_lock_);
    if (server_nonce_strike_register_.get() == NULL) {
      server_nonce_strike_register_.reset(new StrikeRegister(
          server_nonce_strike_register_max_entries_,
          static_cast<uint32>(now.ToUNIXSeconds()),
          server_nonce_strike_register_window_secs_, server_nonce_orbit_,
          StrikeRegister::NO_STARTUP_PERIOD_NEEDED));
    }
    is_unique = server_nonce_strike_register_->Insert(
        server_nonce, static_cast<uint32>(now.ToUNIXSeconds()));
  }

  return is_unique;
}

QuicCryptoServerConfig::Config::Config()
    : channel_id_enabled(false),
      is_primary(false),
      primary_time(QuicWallTime::Zero()) {}

QuicCryptoServerConfig::Config::~Config() { STLDeleteElements(&key_exchanges); }

}  // namespace net
