// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/crypto/proof_source.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using std::string;
using std::vector;

namespace net {
namespace test {

TEST(ProofTest, Verify) {
  // TODO(rtenneti): Enable testing of ProofVerifier.
#if 0
  scoped_ptr<ProofSource> source(CryptoTestUtils::ProofSourceForTesting());
  scoped_ptr<ProofVerifier> verifier(
      CryptoTestUtils::ProofVerifierForTesting());

  const string server_config = "server config bytes";
  const string hostname = "test.example.com";
  const vector<string>* certs;
  const vector<string>* first_certs;
  string error_details, signature, first_signature;
  CertVerifyResult cert_verify_result;

  ASSERT_TRUE(source->GetProof(hostname, server_config, false /* no ECDSA */,
                               &first_certs, &first_signature));
  ASSERT_TRUE(source->GetProof(hostname, server_config, false /* no ECDSA */,
                               &certs, &signature));

  // Check that the proof source is caching correctly:
  ASSERT_EQ(first_certs, certs);
  ASSERT_EQ(signature, first_signature);

  int rv;
  TestCompletionCallback callback;
  rv = verifier->VerifyProof(hostname, server_config, *certs, signature,
                             &error_details, &cert_verify_result,
                             callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);
  ASSERT_EQ("", error_details);
  ASSERT_FALSE(IsCertStatusError(cert_verify_result.cert_status));

  rv = verifier->VerifyProof("foo.com", server_config, *certs, signature,
                             &error_details, &cert_verify_result,
                             callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(ERR_FAILED, rv);
  ASSERT_NE("", error_details);

  rv = verifier->VerifyProof(hostname, server_config.substr(1, string::npos),
                             *certs, signature, &error_details,
                             &cert_verify_result, callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(ERR_FAILED, rv);
  ASSERT_NE("", error_details);

  const string corrupt_signature = "1" + signature;
  rv = verifier->VerifyProof(hostname, server_config, *certs,
                             corrupt_signature, &error_details,
                             &cert_verify_result, callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(ERR_FAILED, rv);
  ASSERT_NE("", error_details);

  vector<string> wrong_certs;
  for (size_t i = 1; i < certs->size(); i++) {
    wrong_certs.push_back((*certs)[i]);
  }
  rv = verifier->VerifyProof("foo.com", server_config, wrong_certs, signature,
                             &error_details, &cert_verify_result,
                             callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(ERR_FAILED, rv);
  ASSERT_NE("", error_details);
#endif  // 0
}

// TestProofVerifierCallback is a simple callback for a ProofVerifier that
// signals a TestCompletionCallback when called and stores the results from the
// ProofVerifier in pointers passed to the constructor.
class TestProofVerifierCallback : public ProofVerifierCallback {
 public:
  TestProofVerifierCallback(TestCompletionCallback* comp_callback,
                            bool* ok,
                            std::string* error_details)
      : comp_callback_(comp_callback),
        ok_(ok),
        error_details_(error_details) {}

  virtual void Run(bool ok,
                   const std::string& error_details,
                   scoped_ptr<ProofVerifyDetails>* details) OVERRIDE {
    *ok_ = ok;
    *error_details_ = error_details;

    comp_callback_->callback().Run(0);
  }

 private:
  TestCompletionCallback* const comp_callback_;
  bool* const ok_;
  std::string* const error_details_;
};

// RunVerification runs |verifier->VerifyProof| and asserts that the result
// matches |expected_ok|.
static void RunVerification(ProofVerifier* verifier,
                            const std::string& hostname,
                            const std::string& server_config,
                            const vector<std::string>& certs,
                            const std::string& proof,
                            bool expected_ok) {
  scoped_ptr<ProofVerifyDetails> details;
  TestCompletionCallback comp_callback;
  bool ok;
  std::string error_details;
  TestProofVerifierCallback* callback =
      new TestProofVerifierCallback(&comp_callback, &ok, &error_details);

  ProofVerifier::Status status = verifier->VerifyProof(
      hostname, server_config, certs, proof, &error_details, &details,
      callback);

  switch (status) {
    case ProofVerifier::FAILURE:
      ASSERT_FALSE(expected_ok);
      ASSERT_NE("", error_details);
      return;
    case ProofVerifier::SUCCESS:
      ASSERT_TRUE(expected_ok);
      ASSERT_EQ("", error_details);
      return;
    case ProofVerifier::PENDING:
      comp_callback.WaitForResult();
      ASSERT_EQ(expected_ok, ok);
      break;
  }
}

static string PEMCertFileToDER(const string& file_name) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, file_name);
  CHECK_NE(static_cast<X509Certificate*>(NULL), cert);

  string der_bytes;
  CHECK(X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_bytes));
  return der_bytes;
}

// A known answer test that allows us to test ProofVerifier without a working
// ProofSource.
TEST(ProofTest, VerifyRSAKnownAnswerTest) {
  // These sample signatures were generated by running the Proof.Verify test
  // and dumping the bytes of the |signature| output of ProofSource::GetProof().
  // sLen = special value -2 used by OpenSSL.
  static const unsigned char signature_data_0[] = {
    0x9e, 0xe6, 0x74, 0x3b, 0x8f, 0xb8, 0x66, 0x77, 0x57, 0x09,
    0x8a, 0x04, 0xe9, 0xf0, 0x7c, 0x91, 0xa9, 0x5c, 0xe9, 0xdf,
    0x12, 0x4d, 0x23, 0x82, 0x8c, 0x29, 0x72, 0x7f, 0xc2, 0x20,
    0xa7, 0xb3, 0xe5, 0xbc, 0xcf, 0x3c, 0x0d, 0x8f, 0xae, 0x46,
    0x6a, 0xb9, 0xee, 0x0c, 0xe1, 0x13, 0x21, 0xc0, 0x7e, 0x45,
    0x24, 0x24, 0x4b, 0x72, 0x43, 0x5e, 0xc4, 0x0d, 0xdf, 0x6c,
    0xd8, 0xaa, 0x35, 0x97, 0x05, 0x40, 0x76, 0xd3, 0x2c, 0xee,
    0x82, 0x16, 0x6a, 0x43, 0xf9, 0xa2, 0xd0, 0x41, 0x3c, 0xed,
    0x3f, 0x40, 0x10, 0x95, 0xc7, 0xa9, 0x1f, 0x04, 0xdb, 0xd5,
    0x98, 0x9f, 0xe2, 0xbf, 0x77, 0x3d, 0xc9, 0x9a, 0xaf, 0xf7,
    0xef, 0x63, 0x0b, 0x7d, 0xc8, 0x37, 0xda, 0x37, 0x23, 0x88,
    0x78, 0xc8, 0x8b, 0xf5, 0xb9, 0x36, 0x5d, 0x72, 0x1f, 0xfc,
    0x14, 0xff, 0xa7, 0x81, 0x27, 0x49, 0xae, 0xe1,
  };
  static const unsigned char signature_data_1[] = {
    0x5e, 0xc2, 0xab, 0x6b, 0x16, 0xe6, 0x55, 0xf3, 0x16, 0x46,
    0x35, 0xdc, 0xcc, 0xde, 0xd0, 0xbd, 0x6c, 0x66, 0xb2, 0x3d,
    0xd3, 0x14, 0x78, 0xed, 0x47, 0x55, 0xfb, 0xdb, 0xe1, 0x7d,
    0xbf, 0x31, 0xf6, 0xf4, 0x10, 0x4c, 0x8d, 0x22, 0x17, 0xaa,
    0xe1, 0x85, 0xc7, 0x96, 0x4c, 0x42, 0xfb, 0xf4, 0x63, 0x53,
    0x8a, 0x79, 0x01, 0x63, 0x48, 0xa8, 0x3a, 0xbc, 0xc9, 0xd2,
    0xf5, 0xec, 0xe9, 0x09, 0x71, 0xaf, 0xce, 0x34, 0x56, 0xe5,
    0x00, 0xbe, 0xee, 0x3c, 0x1c, 0xc4, 0xa0, 0x07, 0xd5, 0x77,
    0xb8, 0x83, 0x57, 0x7d, 0x1a, 0xc9, 0xd0, 0xc0, 0x59, 0x9a,
    0x88, 0x19, 0x3f, 0xb9, 0xf0, 0x45, 0x37, 0xc3, 0x00, 0x8b,
    0xb3, 0x89, 0xf4, 0x89, 0x07, 0xa9, 0xc3, 0x26, 0xbf, 0x81,
    0xaf, 0x6b, 0x47, 0xbc, 0x16, 0x55, 0x37, 0x0a, 0xbe, 0x0e,
    0xc5, 0x75, 0x3f, 0x3d, 0x8e, 0xe8, 0x44, 0xe3,
  };
  static const unsigned char signature_data_2[] = {
    0x8e, 0x5c, 0x78, 0x63, 0x74, 0x99, 0x2e, 0x96, 0xc0, 0x14,
    0x8d, 0xb5, 0x13, 0x74, 0xa3, 0xa4, 0xe0, 0x43, 0x3e, 0x85,
    0xba, 0x8f, 0x3c, 0x5e, 0x14, 0x64, 0x0e, 0x5e, 0xff, 0x89,
    0x88, 0x8a, 0x65, 0xe2, 0xa2, 0x79, 0xe4, 0xe9, 0x3a, 0x7f,
    0xf6, 0x9d, 0x3d, 0xe2, 0xb0, 0x8a, 0x35, 0x55, 0xed, 0x21,
    0xee, 0x20, 0xd8, 0x8a, 0x60, 0x47, 0xca, 0x52, 0x54, 0x91,
    0x99, 0x69, 0x8d, 0x16, 0x34, 0x69, 0xe1, 0x46, 0x56, 0x67,
    0x5f, 0x50, 0xf0, 0x94, 0xe7, 0x8b, 0xf2, 0x6a, 0x73, 0x0f,
    0x30, 0x30, 0xde, 0x59, 0xdc, 0xc7, 0xfe, 0xb6, 0x83, 0xe1,
    0x86, 0x1d, 0x88, 0xd3, 0x2f, 0x2f, 0x74, 0x68, 0xbd, 0x6c,
    0xd1, 0x46, 0x76, 0x06, 0xa9, 0xd4, 0x03, 0x3f, 0xda, 0x7d,
    0xa7, 0xff, 0x48, 0xe4, 0xb4, 0x42, 0x06, 0xac, 0x19, 0x12,
    0xe6, 0x05, 0xae, 0xbe, 0x29, 0x94, 0x8f, 0x99,
  };

  scoped_ptr<ProofVerifier> verifier(
      CryptoTestUtils::ProofVerifierForTesting());

  const string server_config = "server config bytes";
  const string hostname = "test.example.com";
  string error_details;
  CertVerifyResult cert_verify_result;

  vector<string> certs(2);
  certs[0] = PEMCertFileToDER("quic_test.example.com.crt");
  certs[1] = PEMCertFileToDER("quic_intermediate.crt");

  // Signatures are nondeterministic, so we test multiple signatures on the
  // same server_config.
  vector<string> signatures(3);
  signatures[0].assign(reinterpret_cast<const char*>(signature_data_0),
                       sizeof(signature_data_0));
  signatures[1].assign(reinterpret_cast<const char*>(signature_data_1),
                       sizeof(signature_data_1));
  signatures[2].assign(reinterpret_cast<const char*>(signature_data_2),
                       sizeof(signature_data_2));

  for (size_t i = 0; i < signatures.size(); i++) {
    const string& signature = signatures[i];

    RunVerification(
        verifier.get(), hostname, server_config, certs, signature, true);
    RunVerification(
        verifier.get(), "foo.com", server_config, certs, signature, false);
    RunVerification(
        verifier.get(), hostname, server_config.substr(1, string::npos),
        certs, signature, false);

    const string corrupt_signature = "1" + signature;
    RunVerification(
        verifier.get(), hostname, server_config, certs, corrupt_signature,
        false);

    vector<string> wrong_certs;
    for (size_t i = 1; i < certs.size(); i++) {
      wrong_certs.push_back(certs[i]);
    }
    RunVerification(verifier.get(), hostname, server_config, wrong_certs,
                    signature, false);
  }
}

// A known answer test that allows us to test ProofVerifier without a working
// ProofSource.
TEST(ProofTest, VerifyECDSAKnownAnswerTest) {
  // Disable this test on platforms that do not support ECDSA certificates.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  // These sample signatures were generated by running the Proof.Verify test
  // (modified to use ECDSA for signing proofs) and dumping the bytes of the
  // |signature| output of ProofSource::GetProof().
  static const unsigned char signature_data_0[] = {
    0x30, 0x45, 0x02, 0x20, 0x15, 0xb7, 0x9f, 0xe3, 0xd9, 0x7a,
    0x3c, 0x3b, 0x18, 0xb0, 0xdb, 0x60, 0x23, 0x56, 0xa0, 0x06,
    0x4e, 0x70, 0xa3, 0xf7, 0x4b, 0xe5, 0x0d, 0x69, 0xf0, 0x35,
    0x8c, 0xae, 0xb5, 0x54, 0x32, 0xe9, 0x02, 0x21, 0x00, 0xf7,
    0xe3, 0x06, 0x99, 0x16, 0x56, 0x7e, 0xab, 0x33, 0x53, 0x0d,
    0xde, 0xbe, 0xef, 0x6d, 0xb0, 0xc7, 0xa6, 0x63, 0xaf, 0x8d,
    0xab, 0x34, 0xa9, 0xc0, 0x63, 0x88, 0x47, 0x17, 0x4c, 0x4c,
    0x04,
  };
  static const unsigned char signature_data_1[] = {
    0x30, 0x44, 0x02, 0x20, 0x69, 0x60, 0x55, 0xbb, 0x11, 0x93,
    0x6a, 0xdc, 0x9b, 0x61, 0x2c, 0x60, 0x19, 0xbc, 0x15, 0x55,
    0xcf, 0xf2, 0x8e, 0x2e, 0x27, 0x0b, 0x69, 0xef, 0x33, 0x25,
    0x1e, 0x5d, 0x8c, 0x00, 0x11, 0xef, 0x02, 0x20, 0x0c, 0x26,
    0xfe, 0x0b, 0x06, 0x8f, 0xe8, 0xe2, 0x02, 0x63, 0xe5, 0x43,
    0x0d, 0xc9, 0x80, 0x4d, 0xe9, 0x6f, 0x6e, 0x18, 0xdb, 0xb0,
    0x04, 0x2a, 0x45, 0x37, 0x1a, 0x60, 0x0e, 0xc6, 0xc4, 0x8f,
  };
  static const unsigned char signature_data_2[] = {
    0x30, 0x45, 0x02, 0x21, 0x00, 0xd5, 0x43, 0x36, 0x60, 0x50,
    0xce, 0xe0, 0x00, 0x51, 0x02, 0x84, 0x95, 0x51, 0x47, 0xaf,
    0xe4, 0xf9, 0xe1, 0x23, 0xae, 0x21, 0xb4, 0x98, 0xd1, 0xa3,
    0x5f, 0x3b, 0xf3, 0x6a, 0x65, 0x44, 0x6b, 0x02, 0x20, 0x30,
    0x7e, 0xb4, 0xea, 0xf0, 0xda, 0xdb, 0xbd, 0x38, 0xb9, 0x7a,
    0x5d, 0x12, 0x04, 0x0e, 0xc2, 0xf0, 0xb1, 0x0e, 0x25, 0xf8,
    0x0a, 0x27, 0xa3, 0x16, 0x94, 0xac, 0x1e, 0xb8, 0x6e, 0x00,
    0x05,
  };

  scoped_ptr<ProofVerifier> verifier(
      CryptoTestUtils::ProofVerifierForTesting());

  const string server_config = "server config bytes";
  const string hostname = "test.example.com";
  string error_details;
  CertVerifyResult cert_verify_result;

  vector<string> certs(2);
  certs[0] = PEMCertFileToDER("quic_test_ecc.example.com.crt");
  certs[1] = PEMCertFileToDER("quic_intermediate.crt");

  // Signatures are nondeterministic, so we test multiple signatures on the
  // same server_config.
  vector<string> signatures(3);
  signatures[0].assign(reinterpret_cast<const char*>(signature_data_0),
                       sizeof(signature_data_0));
  signatures[1].assign(reinterpret_cast<const char*>(signature_data_1),
                       sizeof(signature_data_1));
  signatures[2].assign(reinterpret_cast<const char*>(signature_data_2),
                       sizeof(signature_data_2));

  for (size_t i = 0; i < signatures.size(); i++) {
    const string& signature = signatures[i];

    RunVerification(
        verifier.get(), hostname, server_config, certs, signature, true);
    RunVerification(
        verifier.get(), "foo.com", server_config, certs, signature, false);
    RunVerification(
        verifier.get(), hostname, server_config.substr(1, string::npos),
        certs, signature, false);

    // An ECDSA signature is DER-encoded. Corrupt the last byte so that the
    // signature can still be DER-decoded correctly.
    string corrupt_signature = signature;
    corrupt_signature[corrupt_signature.size() - 1] += 1;
    RunVerification(
        verifier.get(), hostname, server_config, certs, corrupt_signature,
        false);

    // Prepending a "1" makes the DER invalid.
    const string bad_der_signature1 = "1" + signature;
    RunVerification(
        verifier.get(), hostname, server_config, certs, bad_der_signature1,
        false);

    vector<string> wrong_certs;
    for (size_t i = 1; i < certs.size(); i++) {
      wrong_certs.push_back(certs[i]);
    }
    RunVerification(
        verifier.get(), hostname, server_config, wrong_certs, signature,
        false);
  }
}

}  // namespace test
}  // namespace net
