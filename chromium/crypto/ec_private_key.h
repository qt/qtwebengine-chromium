// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_EC_PRIVATE_KEY_H_
#define CRYPTO_EC_PRIVATE_KEY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

#if defined(USE_OPENSSL)
// Forward declaration for openssl/*.h
typedef struct evp_pkey_st EVP_PKEY;
#else
// Forward declaration.
typedef struct CERTSubjectPublicKeyInfoStr CERTSubjectPublicKeyInfo;
typedef struct PK11SlotInfoStr PK11SlotInfo;
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;
#endif

namespace crypto {

// Encapsulates an elliptic curve (EC) private key. Can be used to generate new
// keys, export keys to other formats, or to extract a public key.
// TODO(mattm): make this and RSAPrivateKey implement some PrivateKey interface.
// (The difference in types of key() and public_key() make this a little
// tricky.)
class CRYPTO_EXPORT ECPrivateKey {
 public:
  ~ECPrivateKey();

  // Returns whether the system supports elliptic curve cryptography.
  static bool IsSupported();

  // Creates a new random instance. Can return NULL if initialization fails.
  // The created key will use the NIST P-256 curve.
  // TODO(mattm): Add a curve parameter.
  static ECPrivateKey* Create();

#if defined(USE_NSS)
  // Creates a new random instance in |slot|. Can return NULL if initialization
  // fails.  The created key is permanent and is not exportable in plaintext
  // form.
  static ECPrivateKey* CreateSensitive(PK11SlotInfo* slot);
#endif

  // Creates a new instance by importing an existing key pair.
  // The key pair is given as an ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo
  // block and an X.509 SubjectPublicKeyInfo block.
  // Returns NULL if initialization fails.
  static ECPrivateKey* CreateFromEncryptedPrivateKeyInfo(
      const std::string& password,
      const std::vector<uint8>& encrypted_private_key_info,
      const std::vector<uint8>& subject_public_key_info);

#if defined(USE_NSS)
  // Creates a new instance in |slot| by importing an existing key pair.
  // The key pair is given as an ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo
  // block and an X.509 SubjectPublicKeyInfo block.
  // This can return NULL if initialization fails.  The created key is permanent
  // and is not exportable in plaintext form.
  static ECPrivateKey* CreateSensitiveFromEncryptedPrivateKeyInfo(
      PK11SlotInfo* slot,
      const std::string& password,
      const std::vector<uint8>& encrypted_private_key_info,
      const std::vector<uint8>& subject_public_key_info);
#endif

#if !defined(USE_OPENSSL)
  // Imports the key pair into |slot| and returns in |public_key| and |key|.
  // Shortcut for code that needs to keep a reference directly to NSS types
  // without having to create a ECPrivateKey object and make a copy of them.
  // TODO(mattm): move this function to some NSS util file.
  static bool ImportFromEncryptedPrivateKeyInfo(
      PK11SlotInfo* slot,
      const std::string& password,
      const uint8* encrypted_private_key_info,
      size_t encrypted_private_key_info_len,
      CERTSubjectPublicKeyInfo* decoded_spki,
      bool permanent,
      bool sensitive,
      SECKEYPrivateKey** key,
      SECKEYPublicKey** public_key);
#endif

#if defined(USE_OPENSSL)
  EVP_PKEY* key() { return key_; }
#else
  SECKEYPrivateKey* key() { return key_; }
  SECKEYPublicKey* public_key() { return public_key_; }
#endif

  // Exports the private key as an ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo
  // block and the public key as an X.509 SubjectPublicKeyInfo block.
  // The |password| and |iterations| are used as inputs to the key derivation
  // function for generating the encryption key.  PKCS #5 recommends a minimum
  // of 1000 iterations, on modern systems a larger value may be preferrable.
  bool ExportEncryptedPrivateKey(const std::string& password,
                                 int iterations,
                                 std::vector<uint8>* output);

  // Exports the public key to an X.509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8>* output);

  // Exports private key data for testing. The format of data stored into output
  // doesn't matter other than that it is consistent for the same key.
  bool ExportValue(std::vector<uint8>* output);
  bool ExportECParams(std::vector<uint8>* output);

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  ECPrivateKey();

#if !defined(USE_OPENSSL)
  // Shared helper for Create() and CreateSensitive().
  // TODO(cmasone): consider replacing |permanent| and |sensitive| with a
  //                flags arg created by ORing together some enumerated values.
  static ECPrivateKey* CreateWithParams(PK11SlotInfo* slot,
                                        bool permanent,
                                        bool sensitive);

  // Shared helper for CreateFromEncryptedPrivateKeyInfo() and
  // CreateSensitiveFromEncryptedPrivateKeyInfo().
  static ECPrivateKey* CreateFromEncryptedPrivateKeyInfoWithParams(
      PK11SlotInfo* slot,
      const std::string& password,
      const std::vector<uint8>& encrypted_private_key_info,
      const std::vector<uint8>& subject_public_key_info,
      bool permanent,
      bool sensitive);
#endif

#if defined(USE_OPENSSL)
  EVP_PKEY* key_;
#else
  SECKEYPrivateKey* key_;
  SECKEYPublicKey* public_key_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ECPrivateKey);
};


}  // namespace crypto

#endif  // CRYPTO_EC_PRIVATE_KEY_H_
