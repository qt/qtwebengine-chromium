// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_private_key.h"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/openssl_util.h"

namespace crypto {

namespace {

// Function pointer definition, for injecting the required key export function
// into ExportKeyWithBio, below. |bio| is a temporary memory BIO object, and
// |key| is a handle to the input key object. Return 1 on success, 0 otherwise.
// NOTE: Used with OpenSSL functions, which do not comply with the Chromium
//       style guide, hence the unusual parameter placement / types.
typedef int (*ExportBioFunction)(BIO* bio, const void* key);

// Helper to export |key| into |output| via the specified ExportBioFunction.
bool ExportKeyWithBio(const void* key,
                      ExportBioFunction export_fn,
                      std::vector<uint8>* output) {
  if (!key)
    return false;

  ScopedOpenSSL<BIO, BIO_free_all> bio(BIO_new(BIO_s_mem()));
  if (!bio.get())
    return false;

  if (!export_fn(bio.get(), key))
    return false;

  char* data = NULL;
  long len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0)
    return false;

  output->assign(data, data + len);
  return true;
}

// Function pointer definition, for injecting the required key export function
// into ExportKey below. |key| is a pointer to the input key object,
// and |data| is either NULL, or the address of an 'unsigned char*' pointer
// that points to the start of the output buffer. The function must return
// the number of bytes required to export the data, or -1 in case of error.
typedef int (*ExportDataFunction)(const void* key, unsigned char** data);

// Helper to export |key| into |output| via the specified export function.
bool ExportKey(const void* key,
               ExportDataFunction export_fn,
               std::vector<uint8>* output) {
  if (!key)
    return false;

  int data_len = export_fn(key, NULL);
  if (data_len < 0)
    return false;

  output->resize(static_cast<size_t>(data_len));
  unsigned char* data = &(*output)[0];
  if (export_fn(key, &data) < 0)
    return false;

  return true;
}

}  // namespace

ECPrivateKey::~ECPrivateKey() {
  if (key_)
    EVP_PKEY_free(key_);
}

// static
bool ECPrivateKey::IsSupported() { return true; }

// static
ECPrivateKey* ECPrivateKey::Create() {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ScopedOpenSSL<EC_KEY, EC_KEY_free> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!ec_key.get() || !EC_KEY_generate_key(ec_key.get()))
    return NULL;

  scoped_ptr<ECPrivateKey> result(new ECPrivateKey());
  result->key_ = EVP_PKEY_new();
  if (!result->key_ || !EVP_PKEY_set1_EC_KEY(result->key_, ec_key.get()))
    return NULL;

  return result.release();
}

// static
ECPrivateKey* ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
    const std::string& password,
    const std::vector<uint8>& encrypted_private_key_info,
    const std::vector<uint8>& subject_public_key_info) {
  // NOTE: The |subject_public_key_info| can be ignored here, it is only
  // useful for the NSS implementation (which uses the public key's SHA1
  // as a lookup key when storing the private one in its store).
  if (encrypted_private_key_info.empty())
    return NULL;

  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  // Write the encrypted private key into a memory BIO.
  char* private_key_data = reinterpret_cast<char*>(
      const_cast<uint8*>(&encrypted_private_key_info[0]));
  int private_key_data_len =
      static_cast<int>(encrypted_private_key_info.size());
  ScopedOpenSSL<BIO, BIO_free_all> bio(
      BIO_new_mem_buf(private_key_data, private_key_data_len));
  if (!bio.get())
    return NULL;

  // Convert it, then decrypt it into a PKCS#8 object.
  ScopedOpenSSL<X509_SIG, X509_SIG_free> p8_encrypted(
      d2i_PKCS8_bio(bio.get(), NULL));
  if (!p8_encrypted.get())
    return NULL;

  ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free> p8_decrypted(
      PKCS8_decrypt(p8_encrypted.get(),
                    password.c_str(),
                    static_cast<int>(password.size())));
  if (!p8_decrypted.get())
    return NULL;

  // Create a new EVP_PKEY for it.
  scoped_ptr<ECPrivateKey> result(new ECPrivateKey);
  result->key_ = EVP_PKCS82PKEY(p8_decrypted.get());
  if (!result->key_)
    return NULL;

  return result.release();
}

bool ECPrivateKey::ExportEncryptedPrivateKey(
    const std::string& password,
    int iterations,
    std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  // Convert into a PKCS#8 object.
  ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free> pkcs8(
      EVP_PKEY2PKCS8(key_));
  if (!pkcs8.get())
    return false;

  // Encrypt the object.
  // NOTE: NSS uses SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC
  // so use NID_pbe_WithSHA1And3_Key_TripleDES_CBC which should be the OpenSSL
  // equivalent.
  ScopedOpenSSL<X509_SIG, X509_SIG_free> encrypted(
      PKCS8_encrypt(NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                    NULL,
                    password.c_str(),
                    static_cast<int>(password.size()),
                    NULL,
                    0,
                    iterations,
                    pkcs8.get()));
  if (!encrypted.get())
    return false;

  // Write it into |*output|
  return ExportKeyWithBio(encrypted.get(),
                          reinterpret_cast<ExportBioFunction>(i2d_PKCS8_bio),
                          output);
}

bool ECPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  return ExportKeyWithBio(
      key_, reinterpret_cast<ExportBioFunction>(i2d_PUBKEY_bio), output);
}

bool ECPrivateKey::ExportValue(std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  ScopedOpenSSL<EC_KEY, EC_KEY_free> ec_key(EVP_PKEY_get1_EC_KEY(key_));
  return ExportKey(ec_key.get(),
                   reinterpret_cast<ExportDataFunction>(i2d_ECPrivateKey),
                   output);
}

bool ECPrivateKey::ExportECParams(std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  ScopedOpenSSL<EC_KEY, EC_KEY_free> ec_key(EVP_PKEY_get1_EC_KEY(key_));
  return ExportKey(ec_key.get(),
                   reinterpret_cast<ExportDataFunction>(i2d_ECParameters),
                   output);
}

ECPrivateKey::ECPrivateKey() : key_(NULL) {}

}  // namespace crypto
