// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_win.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/capi_util.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_known_roots_win.h"

#pragma comment(lib, "crypt32.lib")

#if !defined(CERT_TRUST_HAS_WEAK_SIGNATURE)
// This was introduced in Windows 8 / Windows Server 2012, but retroactively
// ported as far back as Windows XP via system update.
#define CERT_TRUST_HAS_WEAK_SIGNATURE 0x00100000
#endif

namespace net {

namespace {

struct FreeChainEngineFunctor {
  void operator()(HCERTCHAINENGINE engine) const {
    if (engine)
      CertFreeCertificateChainEngine(engine);
  }
};

struct FreeCertChainContextFunctor {
  void operator()(PCCERT_CHAIN_CONTEXT chain_context) const {
    if (chain_context)
      CertFreeCertificateChain(chain_context);
  }
};

struct FreeCertContextFunctor {
  void operator()(PCCERT_CONTEXT context) const {
    if (context)
      CertFreeCertificateContext(context);
  }
};

typedef crypto::ScopedCAPIHandle<HCERTCHAINENGINE, FreeChainEngineFunctor>
    ScopedHCERTCHAINENGINE;

typedef scoped_ptr_malloc<const CERT_CHAIN_CONTEXT,
                          FreeCertChainContextFunctor>
    ScopedPCCERT_CHAIN_CONTEXT;

typedef scoped_ptr_malloc<const CERT_CONTEXT,
                          FreeCertContextFunctor> ScopedPCCERT_CONTEXT;

//-----------------------------------------------------------------------------

int MapSecurityError(SECURITY_STATUS err) {
  // There are numerous security error codes, but these are the ones we thus
  // far find interesting.
  switch (err) {
    case SEC_E_WRONG_PRINCIPAL:  // Schannel
    case CERT_E_CN_NO_MATCH:  // CryptoAPI
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_E_UNTRUSTED_ROOT:  // Schannel
    case CERT_E_UNTRUSTEDROOT:  // CryptoAPI
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_E_CERT_EXPIRED:  // Schannel
    case CERT_E_EXPIRED:  // CryptoAPI
      return ERR_CERT_DATE_INVALID;
    case CRYPT_E_NO_REVOCATION_CHECK:
      return ERR_CERT_NO_REVOCATION_MECHANISM;
    case CRYPT_E_REVOCATION_OFFLINE:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case CRYPT_E_REVOKED:  // Schannel and CryptoAPI
      return ERR_CERT_REVOKED;
    case SEC_E_CERT_UNKNOWN:
    case CERT_E_ROLE:
      return ERR_CERT_INVALID;
    case CERT_E_WRONG_USAGE:
      // TODO(wtc): Should we add ERR_CERT_WRONG_USAGE?
      return ERR_CERT_INVALID;
    // We received an unexpected_message or illegal_parameter alert message
    // from the server.
    case SEC_E_ILLEGAL_MESSAGE:
      return ERR_SSL_PROTOCOL_ERROR;
    case SEC_E_ALGORITHM_MISMATCH:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SEC_E_INVALID_HANDLE:
      return ERR_UNEXPECTED;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map the errors in the chain_context->TrustStatus.dwErrorStatus returned by
// CertGetCertificateChain to our certificate status flags.
int MapCertChainErrorStatusToCertStatus(DWORD error_status) {
  CertStatus cert_status = 0;

  // We don't include CERT_TRUST_IS_NOT_TIME_NESTED because it's obsolete and
  // we wouldn't consider it an error anyway
  const DWORD kDateInvalidErrors = CERT_TRUST_IS_NOT_TIME_VALID |
                                   CERT_TRUST_CTL_IS_NOT_TIME_VALID;
  if (error_status & kDateInvalidErrors)
    cert_status |= CERT_STATUS_DATE_INVALID;

  const DWORD kAuthorityInvalidErrors = CERT_TRUST_IS_UNTRUSTED_ROOT |
                                        CERT_TRUST_IS_EXPLICIT_DISTRUST |
                                        CERT_TRUST_IS_PARTIAL_CHAIN;
  if (error_status & kAuthorityInvalidErrors)
    cert_status |= CERT_STATUS_AUTHORITY_INVALID;

  if ((error_status & CERT_TRUST_REVOCATION_STATUS_UNKNOWN) &&
      !(error_status & CERT_TRUST_IS_OFFLINE_REVOCATION))
    cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (error_status & CERT_TRUST_IS_OFFLINE_REVOCATION)
    cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (error_status & CERT_TRUST_IS_REVOKED)
    cert_status |= CERT_STATUS_REVOKED;

  const DWORD kWrongUsageErrors = CERT_TRUST_IS_NOT_VALID_FOR_USAGE |
                                  CERT_TRUST_CTL_IS_NOT_VALID_FOR_USAGE;
  if (error_status & kWrongUsageErrors) {
    // TODO(wtc): Should we add CERT_STATUS_WRONG_USAGE?
    cert_status |= CERT_STATUS_INVALID;
  }

  if (error_status & CERT_TRUST_IS_NOT_SIGNATURE_VALID) {
    // Check for a signature that does not meet the OS criteria for strong
    // signatures.
    // Note: These checks may be more restrictive than the current weak key
    // criteria implemented within CertVerifier, such as excluding SHA-1 or
    // excluding RSA keys < 2048 bits. However, if the user has configured
    // these more stringent checks, respect that configuration and err on the
    // more restrictive criteria.
    if (error_status & CERT_TRUST_HAS_WEAK_SIGNATURE) {
      cert_status |= CERT_STATUS_WEAK_KEY;
    } else {
      cert_status |= CERT_STATUS_INVALID;
    }
  }

  // The rest of the errors.
  const DWORD kCertInvalidErrors =
      CERT_TRUST_IS_CYCLIC |
      CERT_TRUST_INVALID_EXTENSION |
      CERT_TRUST_INVALID_POLICY_CONSTRAINTS |
      CERT_TRUST_INVALID_BASIC_CONSTRAINTS |
      CERT_TRUST_INVALID_NAME_CONSTRAINTS |
      CERT_TRUST_CTL_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_HAS_NOT_SUPPORTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_DEFINED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_PERMITTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_EXCLUDED_NAME_CONSTRAINT |
      CERT_TRUST_NO_ISSUANCE_CHAIN_POLICY |
      CERT_TRUST_HAS_NOT_SUPPORTED_CRITICAL_EXT;
  if (error_status & kCertInvalidErrors)
    cert_status |= CERT_STATUS_INVALID;

  return cert_status;
}

// Returns true if any common name in the certificate's Subject field contains
// a NULL character.
bool CertSubjectCommonNameHasNull(PCCERT_CONTEXT cert) {
  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = crypto::CryptAlloc;
  decode_para.pfnFree = crypto::CryptFree;
  CERT_NAME_INFO* name_info = NULL;
  DWORD name_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           X509_NAME,
                           cert->pCertInfo->Subject.pbData,
                           cert->pCertInfo->Subject.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &name_info,
                           &name_info_size);
  if (rv) {
    scoped_ptr_malloc<CERT_NAME_INFO> scoped_name_info(name_info);

    // The Subject field may have multiple common names.  According to the
    // "PKI Layer Cake" paper, CryptoAPI uses every common name in the
    // Subject field, so we inspect every common name.
    //
    // From RFC 5280:
    // X520CommonName ::= CHOICE {
    //       teletexString     TeletexString   (SIZE (1..ub-common-name)),
    //       printableString   PrintableString (SIZE (1..ub-common-name)),
    //       universalString   UniversalString (SIZE (1..ub-common-name)),
    //       utf8String        UTF8String      (SIZE (1..ub-common-name)),
    //       bmpString         BMPString       (SIZE (1..ub-common-name)) }
    //
    // We also check IA5String and VisibleString.
    for (DWORD i = 0; i < name_info->cRDN; ++i) {
      PCERT_RDN rdn = &name_info->rgRDN[i];
      for (DWORD j = 0; j < rdn->cRDNAttr; ++j) {
        PCERT_RDN_ATTR rdn_attr = &rdn->rgRDNAttr[j];
        if (strcmp(rdn_attr->pszObjId, szOID_COMMON_NAME) == 0) {
          switch (rdn_attr->dwValueType) {
            // After the CryptoAPI ASN.1 security vulnerabilities described in
            // http://www.microsoft.com/technet/security/Bulletin/MS09-056.mspx
            // were patched, we get CERT_RDN_ENCODED_BLOB for a common name
            // that contains a NULL character.
            case CERT_RDN_ENCODED_BLOB:
              break;
            // Array of 8-bit characters.
            case CERT_RDN_PRINTABLE_STRING:
            case CERT_RDN_TELETEX_STRING:
            case CERT_RDN_IA5_STRING:
            case CERT_RDN_VISIBLE_STRING:
              for (DWORD k = 0; k < rdn_attr->Value.cbData; ++k) {
                if (rdn_attr->Value.pbData[k] == '\0')
                  return true;
              }
              break;
            // Array of 16-bit characters.
            case CERT_RDN_BMP_STRING:
            case CERT_RDN_UTF8_STRING: {
              DWORD num_wchars = rdn_attr->Value.cbData / 2;
              wchar_t* common_name =
                  reinterpret_cast<wchar_t*>(rdn_attr->Value.pbData);
              for (DWORD k = 0; k < num_wchars; ++k) {
                if (common_name[k] == L'\0')
                  return true;
              }
              break;
            }
            // Array of ints (32-bit).
            case CERT_RDN_UNIVERSAL_STRING: {
              DWORD num_ints = rdn_attr->Value.cbData / 4;
              int* common_name =
                  reinterpret_cast<int*>(rdn_attr->Value.pbData);
              for (DWORD k = 0; k < num_ints; ++k) {
                if (common_name[k] == 0)
                  return true;
              }
              break;
            }
            default:
              NOTREACHED();
              break;
          }
        }
      }
    }
  }
  return false;
}

// IsIssuedByKnownRoot returns true if the given chain is rooted at a root CA
// which we recognise as a standard root.
// static
bool IsIssuedByKnownRoot(PCCERT_CHAIN_CONTEXT chain_context) {
  PCERT_SIMPLE_CHAIN first_chain = chain_context->rgpChain[0];
  int num_elements = first_chain->cElement;
  if (num_elements < 1)
    return false;
  PCERT_CHAIN_ELEMENT* element = first_chain->rgpElement;
  PCCERT_CONTEXT cert = element[num_elements - 1]->pCertContext;

  SHA1HashValue hash = X509Certificate::CalculateFingerprint(cert);
  return IsSHA1HashInSortedArray(
      hash, &kKnownRootCertSHA1Hashes[0][0], sizeof(kKnownRootCertSHA1Hashes));
}

// Saves some information about the certificate chain |chain_context| in
// |*verify_result|. The caller MUST initialize |*verify_result| before
// calling this function.
void GetCertChainInfo(PCCERT_CHAIN_CONTEXT chain_context,
                      CertVerifyResult* verify_result) {
  if (chain_context->cChain == 0)
    return;

  PCERT_SIMPLE_CHAIN first_chain = chain_context->rgpChain[0];
  int num_elements = first_chain->cElement;
  PCERT_CHAIN_ELEMENT* element = first_chain->rgpElement;

  PCCERT_CONTEXT verified_cert = NULL;
  std::vector<PCCERT_CONTEXT> verified_chain;

  bool has_root_ca = num_elements > 1 &&
      !(chain_context->TrustStatus.dwErrorStatus &
          CERT_TRUST_IS_PARTIAL_CHAIN);

  // Each chain starts with the end entity certificate (i = 0) and ends with
  // either the root CA certificate or the last available intermediate. If a
  // root CA certificate is present, do not inspect the signature algorithm of
  // the root CA certificate because the signature on the trust anchor is not
  // important.
  if (has_root_ca) {
    // If a full chain was constructed, regardless of whether it was trusted,
    // don't inspect the root's signature algorithm.
    num_elements -= 1;
  }

  for (int i = 0; i < num_elements; ++i) {
    PCCERT_CONTEXT cert = element[i]->pCertContext;
    if (i == 0) {
      verified_cert = cert;
    } else {
      verified_chain.push_back(cert);
    }

    const char* algorithm = cert->pCertInfo->SignatureAlgorithm.pszObjId;
    if (strcmp(algorithm, szOID_RSA_MD5RSA) == 0) {
      // md5WithRSAEncryption: 1.2.840.113549.1.1.4
      verify_result->has_md5 = true;
    } else if (strcmp(algorithm, szOID_RSA_MD2RSA) == 0) {
      // md2WithRSAEncryption: 1.2.840.113549.1.1.2
      verify_result->has_md2 = true;
    } else if (strcmp(algorithm, szOID_RSA_MD4RSA) == 0) {
      // md4WithRSAEncryption: 1.2.840.113549.1.1.3
      verify_result->has_md4 = true;
    }
  }

  if (verified_cert) {
    // Add the root certificate, if present, as it was not added above.
    if (has_root_ca)
      verified_chain.push_back(element[num_elements]->pCertContext);
    verify_result->verified_cert =
          X509Certificate::CreateFromHandle(verified_cert, verified_chain);
  }
}

// Decodes the cert's certificatePolicies extension into a CERT_POLICIES_INFO
// structure and stores it in *output.
void GetCertPoliciesInfo(PCCERT_CONTEXT cert,
                         scoped_ptr_malloc<CERT_POLICIES_INFO>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_CERT_POLICIES,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = crypto::CryptAlloc;
  decode_para.pfnFree = crypto::CryptFree;
  CERT_POLICIES_INFO* policies_info = NULL;
  DWORD policies_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_CERT_POLICIES,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &policies_info,
                           &policies_info_size);
  if (rv)
    output->reset(policies_info);
}

enum CRLSetResult {
  kCRLSetOk,
  kCRLSetUnknown,
  kCRLSetRevoked,
};

// CheckRevocationWithCRLSet attempts to check each element of |chain|
// against |crl_set|. It returns:
//   kCRLSetRevoked: if any element of the chain is known to have been revoked.
//   kCRLSetUnknown: if there is no fresh information about some element in
//       the chain.
//   kCRLSetOk: if every element in the chain is covered by a fresh CRLSet and
//       is unrevoked.
CRLSetResult CheckRevocationWithCRLSet(PCCERT_CHAIN_CONTEXT chain,
                                       CRLSet* crl_set) {
  if (chain->cChain == 0)
    return kCRLSetOk;

  const PCERT_SIMPLE_CHAIN first_chain = chain->rgpChain[0];
  const PCERT_CHAIN_ELEMENT* element = first_chain->rgpElement;

  const int num_elements = first_chain->cElement;
  if (num_elements == 0)
    return kCRLSetOk;

  bool covered = true;

  // We iterate from the root certificate down to the leaf, keeping track of
  // the issuer's SPKI at each step.
  std::string issuer_spki_hash;
  for (int i = num_elements - 1; i >= 0; i--) {
    PCCERT_CONTEXT cert = element[i]->pCertContext;

    base::StringPiece der_bytes(
        reinterpret_cast<const char*>(cert->pbCertEncoded),
        cert->cbCertEncoded);

    base::StringPiece spki;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki)) {
      NOTREACHED();
      covered = false;
      continue;
    }

    const std::string spki_hash = crypto::SHA256HashString(spki);

    const CRYPT_INTEGER_BLOB* serial_blob = &cert->pCertInfo->SerialNumber;
    scoped_ptr<uint8[]> serial_bytes(new uint8[serial_blob->cbData]);
    // The bytes of the serial number are stored little-endian.
    for (unsigned j = 0; j < serial_blob->cbData; j++)
      serial_bytes[j] = serial_blob->pbData[serial_blob->cbData - j - 1];
    base::StringPiece serial(reinterpret_cast<const char*>(serial_bytes.get()),
                             serial_blob->cbData);

    CRLSet::Result result = crl_set->CheckSPKI(spki_hash);

    if (result != CRLSet::REVOKED && !issuer_spki_hash.empty())
      result = crl_set->CheckSerial(serial, issuer_spki_hash);

    issuer_spki_hash = spki_hash;

    switch (result) {
      case CRLSet::REVOKED:
        return kCRLSetRevoked;
      case CRLSet::UNKNOWN:
        covered = false;
        continue;
      case CRLSet::GOOD:
        continue;
      default:
        NOTREACHED();
        covered = false;
        continue;
    }
  }

  if (!covered || crl_set->IsExpired())
    return kCRLSetUnknown;
  return kCRLSetOk;
}

void AppendPublicKeyHashes(PCCERT_CHAIN_CONTEXT chain,
                           HashValueVector* hashes) {
  if (chain->cChain == 0)
    return;

  PCERT_SIMPLE_CHAIN first_chain = chain->rgpChain[0];
  PCERT_CHAIN_ELEMENT* const element = first_chain->rgpElement;

  const DWORD num_elements = first_chain->cElement;
  for (DWORD i = 0; i < num_elements; i++) {
    PCCERT_CONTEXT cert = element[i]->pCertContext;

    base::StringPiece der_bytes(
        reinterpret_cast<const char*>(cert->pbCertEncoded),
        cert->cbCertEncoded);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha1(HASH_VALUE_SHA1);
    base::SHA1HashBytes(reinterpret_cast<const uint8*>(spki_bytes.data()),
                        spki_bytes.size(), sha1.data());
    hashes->push_back(sha1);

    HashValue sha256(HASH_VALUE_SHA256);
    crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
    hashes->push_back(sha256);
  }
}

// Returns true if the certificate is an extended-validation certificate.
//
// This function checks the certificatePolicies extensions of the
// certificates in the certificate chain according to Section 7 (pp. 11-12)
// of the EV Certificate Guidelines Version 1.0 at
// http://cabforum.org/EV_Certificate_Guidelines.pdf.
bool CheckEV(PCCERT_CHAIN_CONTEXT chain_context,
             bool rev_checking_enabled,
             const char* policy_oid) {
  DCHECK_NE(static_cast<DWORD>(0), chain_context->cChain);
  // If the cert doesn't match any of the policies, the
  // CERT_TRUST_IS_NOT_VALID_FOR_USAGE bit (0x10) in
  // chain_context->TrustStatus.dwErrorStatus is set.
  DWORD error_status = chain_context->TrustStatus.dwErrorStatus;

  if (!rev_checking_enabled) {
    // If online revocation checking is disabled then we will have still
    // requested that the revocation cache be checked. However, that will often
    // cause the following two error bits to be set. These error bits mean that
    // the local OCSP/CRL is stale or missing entries for these certificates.
    // Since they are expected, we mask them away.
    error_status &= ~(CERT_TRUST_IS_OFFLINE_REVOCATION |
                      CERT_TRUST_REVOCATION_STATUS_UNKNOWN);
  }
  if (!chain_context->cChain || error_status != CERT_TRUST_NO_ERROR)
    return false;

  // Check the end certificate simple chain (chain_context->rgpChain[0]).
  // If the end certificate's certificatePolicies extension contains the
  // EV policy OID of the root CA, return true.
  PCERT_CHAIN_ELEMENT* element = chain_context->rgpChain[0]->rgpElement;
  int num_elements = chain_context->rgpChain[0]->cElement;
  if (num_elements < 2)
    return false;

  // Look up the EV policy OID of the root CA.
  PCCERT_CONTEXT root_cert = element[num_elements - 1]->pCertContext;
  SHA1HashValue fingerprint =
      X509Certificate::CalculateFingerprint(root_cert);
  EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
  return metadata->HasEVPolicyOID(fingerprint, policy_oid);
}

}  // namespace

CertVerifyProcWin::CertVerifyProcWin() {}

CertVerifyProcWin::~CertVerifyProcWin() {}

bool CertVerifyProcWin::SupportsAdditionalTrustAnchors() const {
  return false;
}

int CertVerifyProcWin::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
  PCCERT_CONTEXT cert_handle = cert->os_cert_handle();
  if (!cert_handle)
    return ERR_UNEXPECTED;

  // Build and validate certificate chain.
  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  // ExtendedKeyUsage.
  // We still need to request szOID_SERVER_GATED_CRYPTO and szOID_SGC_NETSCAPE
  // today because some certificate chains need them.  IE also requests these
  // two usages.
  static const LPSTR usage[] = {
    szOID_PKIX_KP_SERVER_AUTH,
    szOID_SERVER_GATED_CRYPTO,
    szOID_SGC_NETSCAPE
  };
  chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
  chain_para.RequestedUsage.Usage.cUsageIdentifier = arraysize(usage);
  chain_para.RequestedUsage.Usage.rgpszUsageIdentifier =
      const_cast<LPSTR*>(usage);

  // Get the certificatePolicies extension of the certificate.
  scoped_ptr_malloc<CERT_POLICIES_INFO> policies_info;
  LPSTR ev_policy_oid = NULL;
  if (flags & CertVerifier::VERIFY_EV_CERT) {
    GetCertPoliciesInfo(cert_handle, &policies_info);
    if (policies_info.get()) {
      EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
      for (DWORD i = 0; i < policies_info->cPolicyInfo; ++i) {
        LPSTR policy_oid = policies_info->rgPolicyInfo[i].pszPolicyIdentifier;
        if (metadata->IsEVPolicyOID(policy_oid)) {
          ev_policy_oid = policy_oid;
          chain_para.RequestedIssuancePolicy.dwType = USAGE_MATCH_TYPE_AND;
          chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = 1;
          chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier =
              &ev_policy_oid;
          break;
        }
      }
    }
  }

  // We can set CERT_CHAIN_RETURN_LOWER_QUALITY_CONTEXTS to get more chains.
  DWORD chain_flags = CERT_CHAIN_CACHE_END_CERT |
                      CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  bool rev_checking_enabled =
      (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED);

  if (rev_checking_enabled) {
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
  } else {
    chain_flags |= CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
  }

  // For non-test scenarios, use the default HCERTCHAINENGINE, NULL, which
  // corresponds to HCCE_CURRENT_USER and is is initialized as needed by
  // crypt32. However, when testing, it is necessary to create a new
  // HCERTCHAINENGINE and use that instead. This is because each
  // HCERTCHAINENGINE maintains a cache of information about certificates
  // encountered, and each test run may modify the trust status of a
  // certificate.
  ScopedHCERTCHAINENGINE chain_engine(NULL);
  if (TestRootCerts::HasInstance())
    chain_engine.reset(TestRootCerts::GetInstance()->GetChainEngine());

  ScopedPCCERT_CONTEXT cert_list(cert->CreateOSCertChainForCert());
  PCCERT_CHAIN_CONTEXT chain_context;
  // IE passes a non-NULL pTime argument that specifies the current system
  // time.  IE passes CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT as the
  // chain_flags argument.
  if (!CertGetCertificateChain(
           chain_engine,
           cert_list.get(),
           NULL,  // current system time
           cert_list->hCertStore,
           &chain_para,
           chain_flags,
           NULL,  // reserved
           &chain_context)) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return MapSecurityError(GetLastError());
  }

  CRLSetResult crl_set_result = kCRLSetUnknown;
  if (crl_set)
    crl_set_result = CheckRevocationWithCRLSet(chain_context, crl_set);

  if (crl_set_result == kCRLSetRevoked) {
    verify_result->cert_status |= CERT_STATUS_REVOKED;
  } else if (crl_set_result == kCRLSetUnknown &&
             (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED_EV_ONLY) &&
             !rev_checking_enabled &&
             ev_policy_oid != NULL) {
    // We don't have fresh information about this chain from the CRLSet and
    // it's probably an EV certificate. Retry with online revocation checking.
    rev_checking_enabled = true;
    chain_flags &= ~CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

    CertFreeCertificateChain(chain_context);
    if (!CertGetCertificateChain(
             chain_engine,
             cert_list.get(),
             NULL,  // current system time
             cert_list->hCertStore,
             &chain_para,
             chain_flags,
             NULL,  // reserved
             &chain_context)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return MapSecurityError(GetLastError());
    }
  }

  if (chain_context->TrustStatus.dwErrorStatus &
      CERT_TRUST_IS_NOT_VALID_FOR_USAGE) {
    ev_policy_oid = NULL;
    chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = 0;
    chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier = NULL;
    CertFreeCertificateChain(chain_context);
    if (!CertGetCertificateChain(
             chain_engine,
             cert_list.get(),
             NULL,  // current system time
             cert_list->hCertStore,
             &chain_para,
             chain_flags,
             NULL,  // reserved
             &chain_context)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return MapSecurityError(GetLastError());
    }
  }

  CertVerifyResult temp_verify_result = *verify_result;
  GetCertChainInfo(chain_context, verify_result);
  if (!verify_result->is_issued_by_known_root &&
      (flags & CertVerifier::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS)) {
    *verify_result = temp_verify_result;

    rev_checking_enabled = true;
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
    chain_flags &= ~CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;

    CertFreeCertificateChain(chain_context);
    if (!CertGetCertificateChain(
             chain_engine,
             cert_list.get(),
             NULL,  // current system time
             cert_list->hCertStore,
             &chain_para,
             chain_flags,
             NULL,  // reserved
             &chain_context)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return MapSecurityError(GetLastError());
    }
    GetCertChainInfo(chain_context, verify_result);

    if (chain_context->TrustStatus.dwErrorStatus &
        CERT_TRUST_IS_OFFLINE_REVOCATION) {
      verify_result->cert_status |= CERT_STATUS_REVOKED;
    }
  }

  ScopedPCCERT_CHAIN_CONTEXT scoped_chain_context(chain_context);

  verify_result->cert_status |= MapCertChainErrorStatusToCertStatus(
      chain_context->TrustStatus.dwErrorStatus);

  // Flag certificates that have a Subject common name with a NULL character.
  if (CertSubjectCommonNameHasNull(cert_handle))
    verify_result->cert_status |= CERT_STATUS_INVALID;

  std::wstring wstr_hostname = ASCIIToWide(hostname);

  SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra_policy_para;
  memset(&extra_policy_para, 0, sizeof(extra_policy_para));
  extra_policy_para.cbSize = sizeof(extra_policy_para);
  extra_policy_para.dwAuthType = AUTHTYPE_SERVER;
  // Certificate name validation happens separately, later, using an internal
  // routine that has better support for RFC 6125 name matching.
  extra_policy_para.fdwChecks =
      0x00001000;  // SECURITY_FLAG_IGNORE_CERT_CN_INVALID
  extra_policy_para.pwszServerName =
      const_cast<wchar_t*>(wstr_hostname.c_str());

  CERT_CHAIN_POLICY_PARA policy_para;
  memset(&policy_para, 0, sizeof(policy_para));
  policy_para.cbSize = sizeof(policy_para);
  policy_para.dwFlags = 0;
  policy_para.pvExtraPolicyPara = &extra_policy_para;

  CERT_CHAIN_POLICY_STATUS policy_status;
  memset(&policy_status, 0, sizeof(policy_status));
  policy_status.cbSize = sizeof(policy_status);

  if (!CertVerifyCertificateChainPolicy(
           CERT_CHAIN_POLICY_SSL,
           chain_context,
           &policy_para,
           &policy_status)) {
    return MapSecurityError(GetLastError());
  }

  if (policy_status.dwError) {
    verify_result->cert_status |= MapNetErrorToCertStatus(
        MapSecurityError(policy_status.dwError));
  }

  // TODO(wtc): Suppress CERT_STATUS_NO_REVOCATION_MECHANISM for now to be
  // compatible with WinHTTP, which doesn't report this error (bug 3004).
  verify_result->cert_status &= ~CERT_STATUS_NO_REVOCATION_MECHANISM;

  // Perform hostname verification independent of
  // CertVerifyCertificateChainPolicy.
  if (!cert->VerifyNameMatch(hostname,
                             &verify_result->common_name_fallback_used)) {
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;
  }

  if (!rev_checking_enabled) {
    // If we didn't do online revocation checking then Windows will report
    // CERT_UNABLE_TO_CHECK_REVOCATION unless it had cached OCSP or CRL
    // information for every certificate. We only want to put up revoked
    // statuses from the offline checks so we squash this error.
    verify_result->cert_status &= ~CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  }

  AppendPublicKeyHashes(chain_context, &verify_result->public_key_hashes);
  verify_result->is_issued_by_known_root = IsIssuedByKnownRoot(chain_context);

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (ev_policy_oid &&
      CheckEV(chain_context, rev_checking_enabled, ev_policy_oid)) {
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  }
  return OK;
}

}  // namespace net
