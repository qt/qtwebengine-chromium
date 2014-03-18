// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SPAWNED_TEST_SERVER_BASE_TEST_SERVER_H_
#define NET_TEST_SPAWNED_TEST_SERVER_BASE_TEST_SERVER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/host_port_pair.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace net {

class AddressList;
class ScopedPortException;

// The base class of Test server implementation.
class BaseTestServer {
 public:
  typedef std::pair<std::string, std::string> StringPair;

  // Following types represent protocol schemes. See also
  // http://www.iana.org/assignments/uri-schemes.html
  enum Type {
    TYPE_BASIC_AUTH_PROXY,
    TYPE_FTP,
    TYPE_HTTP,
    TYPE_HTTPS,
    TYPE_WS,
    TYPE_WSS,
    TYPE_TCP_ECHO,
    TYPE_UDP_ECHO,
  };

  // Container for various options to control how the HTTPS or WSS server is
  // initialized.
  struct SSLOptions {
    enum ServerCertificate {
      CERT_OK,

      // CERT_AUTO causes the testserver to generate a test certificate issued
      // by "Testing CA" (see net/data/ssl/certificates/ocsp-test-root.pem).
      CERT_AUTO,

      CERT_MISMATCHED_NAME,
      CERT_EXPIRED,
      // Cross-signed certificate to test PKIX path building. Contains an
      // intermediate cross-signed by an unknown root, while the client (via
      // TestRootStore) is expected to have a self-signed version of the
      // intermediate.
      CERT_CHAIN_WRONG_ROOT,
    };

    // OCSPStatus enumerates the types of OCSP response that the testserver
    // can produce.
    enum OCSPStatus {
      OCSP_OK,
      OCSP_REVOKED,
      OCSP_INVALID,
      OCSP_UNAUTHORIZED,
      OCSP_UNKNOWN,
    };

    // Bitmask of bulk encryption algorithms that the test server supports
    // and that can be selectively enabled or disabled.
    enum BulkCipher {
      // Special value used to indicate that any algorithm the server supports
      // is acceptable. Preferred over explicitly OR-ing all ciphers.
      BULK_CIPHER_ANY    = 0,

      BULK_CIPHER_RC4    = (1 << 0),
      BULK_CIPHER_AES128 = (1 << 1),
      BULK_CIPHER_AES256 = (1 << 2),

      // NOTE: 3DES support in the Python test server has external
      // dependencies and not be available on all machines. Clients may not
      // be able to connect if only 3DES is specified.
      BULK_CIPHER_3DES   = (1 << 3),
    };

    // NOTE: the values of these enumerators are passed to the the Python test
    // server. Do not change them.
    enum TLSIntolerantLevel {
      TLS_INTOLERANT_NONE = 0,
      TLS_INTOLERANT_ALL = 1,  // Intolerant of all TLS versions.
      TLS_INTOLERANT_TLS1_1 = 2,  // Intolerant of TLS 1.1 or higher.
      TLS_INTOLERANT_TLS1_2 = 3,  // Intolerant of TLS 1.2 or higher.
    };

    // Initialize a new SSLOptions using CERT_OK as the certificate.
    SSLOptions();

    // Initialize a new SSLOptions that will use the specified certificate.
    explicit SSLOptions(ServerCertificate cert);
    ~SSLOptions();

    // Returns the relative filename of the file that contains the
    // |server_certificate|.
    base::FilePath GetCertificateFile() const;

    // GetOCSPArgument returns the value of any OCSP argument to testserver or
    // the empty string if there is none.
    std::string GetOCSPArgument() const;

    // The certificate to use when serving requests.
    ServerCertificate server_certificate;

    // If |server_certificate==CERT_AUTO| then this determines the type of OCSP
    // response returned.
    OCSPStatus ocsp_status;

    // If not zero, |cert_serial| will be the serial number of the
    // auto-generated leaf certificate when |server_certificate==CERT_AUTO|.
    uint64 cert_serial;

    // True if a CertificateRequest should be sent to the client during
    // handshaking.
    bool request_client_certificate;

    // If |request_client_certificate| is true, an optional list of files,
    // each containing a single, PEM-encoded X.509 certificates. The subject
    // from each certificate will be added to the certificate_authorities
    // field of the CertificateRequest.
    std::vector<base::FilePath> client_authorities;

    // A bitwise-OR of BulkCipher that should be used by the
    // HTTPS server, or BULK_CIPHER_ANY to indicate that all implemented
    // ciphers are acceptable.
    int bulk_ciphers;

    // If true, pass the --https-record-resume argument to testserver.py which
    // causes it to log session cache actions and echo the log on
    // /ssl-session-cache.
    bool record_resume;

    // If not TLS_INTOLERANT_NONE, the server will abort any handshake that
    // negotiates an intolerant TLS version in order to test version fallback.
    TLSIntolerantLevel tls_intolerant;

    // fallback_scsv_enabled, if true, causes the server to process the
    // TLS_FALLBACK_SCSV cipher suite. This cipher suite is sent by Chrome
    // when performing TLS version fallback in response to an SSL handshake
    // failure. If this option is enabled then the server will reject fallback
    // connections.
    bool fallback_scsv_enabled;

    // Temporary glue for testing: validation of SCTs is application-controlled
    // and can be appropriately mocked out, so sending fake data here does not
    // affect handshaking behaviour.
    // TODO(ekasper): replace with valid SCT files for test certs.
    // (Fake) SignedCertificateTimestampList (as a raw binary string) to send in
    // a TLS extension.
    std::string signed_cert_timestamps_tls_ext;

    // Whether to staple the OCSP response.
    bool staple_ocsp_response;
  };

  // Pass as the 'host' parameter during construction to server on 127.0.0.1
  static const char kLocalhost[];

  // Initialize a TestServer listening on a specific host (IP or hostname).
  BaseTestServer(Type type,  const std::string& host);

  // Initialize a TestServer with a specific set of SSLOptions for HTTPS or WSS.
  BaseTestServer(Type type, const SSLOptions& ssl_options);

  // Returns the host port pair used by current Python based test server only
  // if the server is started.
  const HostPortPair& host_port_pair() const;

  const base::FilePath& document_root() const { return document_root_; }
  const base::DictionaryValue& server_data() const;
  std::string GetScheme() const;
  bool GetAddressList(AddressList* address_list) const WARN_UNUSED_RESULT;

  GURL GetURL(const std::string& path) const;

  GURL GetURLWithUser(const std::string& path,
                      const std::string& user) const;

  GURL GetURLWithUserAndPassword(const std::string& path,
                                 const std::string& user,
                                 const std::string& password) const;

  static bool GetFilePathWithReplacements(
      const std::string& original_path,
      const std::vector<StringPair>& text_to_replace,
      std::string* replacement_path);

  static bool UsingSSL(Type type) {
    return type == BaseTestServer::TYPE_HTTPS ||
           type == BaseTestServer::TYPE_WSS;
  }

 protected:
  virtual ~BaseTestServer();
  Type type() const { return type_; }

  // Gets port currently assigned to host_port_pair_ without checking
  // whether it's available (server started) or not.
  uint16 GetPort();

  // Sets |port| as the actual port used by Python based test server.
  void SetPort(uint16 port);

  // Set up internal status when the server is started.
  bool SetupWhenServerStarted() WARN_UNUSED_RESULT;

  // Clean up internal status when starting to stop server.
  void CleanUpWhenStoppingServer();

  // Set path of test resources.
  void SetResourcePath(const base::FilePath& document_root,
                       const base::FilePath& certificates_dir);

  // Parses the server data read from the test server.  Returns true
  // on success.
  bool ParseServerData(const std::string& server_data) WARN_UNUSED_RESULT;

  // Generates a DictionaryValue with the arguments for launching the external
  // Python test server.
  bool GenerateArguments(base::DictionaryValue* arguments) const
    WARN_UNUSED_RESULT;

  // Subclasses can override this to add arguments that are specific to their
  // own test servers.
  virtual bool GenerateAdditionalArguments(
      base::DictionaryValue* arguments) const WARN_UNUSED_RESULT;

 private:
  void Init(const std::string& host);

  // Marks the root certificate of an HTTPS test server as trusted for
  // the duration of tests.
  bool LoadTestRootCert() const WARN_UNUSED_RESULT;

  // Document root of the test server.
  base::FilePath document_root_;

  // Directory that contains the SSL certificates.
  base::FilePath certificates_dir_;

  // Address the test server listens on.
  HostPortPair host_port_pair_;

  // Holds the data sent from the server (e.g., port number).
  scoped_ptr<base::DictionaryValue> server_data_;

  // If |type_| is TYPE_HTTPS or TYPE_WSS, the TLS settings to use for the test
  // server.
  SSLOptions ssl_options_;

  Type type_;

  // Has the server been started?
  bool started_;

  // Enables logging of the server to the console.
  bool log_to_console_;

  scoped_ptr<ScopedPortException> allowed_port_;

  DISALLOW_COPY_AND_ASSIGN(BaseTestServer);
};

}  // namespace net

#endif  // NET_TEST_SPAWNED_TEST_SERVER_BASE_TEST_SERVER_H_
