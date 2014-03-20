// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A standalone tool for testing MCS connections and the MCS client on their
// own.

#include <cstddef>
#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/net_log_logger.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/default_server_bound_cert_store.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/url_request/url_request_test_util.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

// This is a simple utility that initializes an mcs client and
// prints out any events.
namespace gcm {
namespace {

// The default server to communicate with.
const char kMCSServerHost[] = "mtalk.google.com";
const uint16 kMCSServerPort = 5228;

// Command line switches.
const char kRMQFileName[] = "rmq_file";
const char kAndroidIdSwitch[] = "android_id";
const char kSecretSwitch[] = "secret";
const char kLogFileSwitch[] = "log-file";
const char kIgnoreCertSwitch[] = "ignore-certs";
const char kServerHostSwitch[] = "host";
const char kServerPortSwitch[] = "port";

void MessageReceivedCallback(const MCSMessage& message) {
  LOG(INFO) << "Received message with id "
            << GetPersistentId(message.GetProtobuf()) << " and tag "
            << static_cast<int>(message.tag());

  if (message.tag() == kDataMessageStanzaTag) {
    const mcs_proto::DataMessageStanza& data_message =
        reinterpret_cast<const mcs_proto::DataMessageStanza&>(
            message.GetProtobuf());
    DVLOG(1) << "  to: " << data_message.to();
    DVLOG(1) << "  from: " << data_message.from();
    DVLOG(1) << "  category: " << data_message.category();
    DVLOG(1) << "  sent: " << data_message.sent();
    for (int i = 0; i < data_message.app_data_size(); ++i) {
      DVLOG(1) << "  App data " << i << " "
               << data_message.app_data(i).key() << " : "
               << data_message.app_data(i).value();
    }
  }
}

void MessageSentCallback(const std::string& local_id) {
  LOG(INFO) << "Message sent. Status: " << local_id;
}

// Needed to use a real host resolver.
class MyTestURLRequestContext : public net::TestURLRequestContext {
 public:
  MyTestURLRequestContext() : TestURLRequestContext(true) {
    context_storage_.set_host_resolver(
        net::HostResolver::CreateDefaultResolver(NULL));
    context_storage_.set_transport_security_state(
        new net::TransportSecurityState());
    Init();
  }

  virtual ~MyTestURLRequestContext() {}
};

class MyTestURLRequestContextGetter : public net::TestURLRequestContextGetter {
 public:
  explicit MyTestURLRequestContextGetter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy)
      : TestURLRequestContextGetter(io_message_loop_proxy) {}

  virtual net::TestURLRequestContext* GetURLRequestContext() OVERRIDE {
    // Construct |context_| lazily so it gets constructed on the right
    // thread (the IO thread).
    if (!context_)
      context_.reset(new MyTestURLRequestContext());
    return context_.get();
  }

 private:
  virtual ~MyTestURLRequestContextGetter() {}

  scoped_ptr<MyTestURLRequestContext> context_;
};

// A net log that logs all events by default.
class MyTestNetLog : public net::NetLog {
 public:
  MyTestNetLog() {
    SetBaseLogLevel(LOG_ALL);
  }
  virtual ~MyTestNetLog() {}
};

// A cert verifier that access all certificates.
class MyTestCertVerifier : public net::CertVerifier {
 public:
  MyTestCertVerifier() {}
  virtual ~MyTestCertVerifier() {}

  virtual int Verify(net::X509Certificate* cert,
                     const std::string& hostname,
                     int flags,
                     net::CRLSet* crl_set,
                     net::CertVerifyResult* verify_result,
                     const net::CompletionCallback& callback,
                     RequestHandle* out_req,
                     const net::BoundNetLog& net_log) OVERRIDE {
    return net::OK;
  }

  virtual void CancelRequest(RequestHandle req) OVERRIDE {
    // Do nothing.
  }
};

class MCSProbe {
 public:
  MCSProbe(
      const CommandLine& command_line,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  ~MCSProbe();

  void Start();

  uint64 android_id() const { return android_id_; }
  uint64 secret() const { return secret_; }

 private:
  void InitializeNetworkState();
  void BuildNetworkSession();

  void InitializationCallback(bool success,
                              uint64 restored_android_id,
                              uint64 restored_security_token);

  CommandLine command_line_;

  base::FilePath rmq_path_;
  uint64 android_id_;
  uint64 secret_;
  std::string server_host_;
  int server_port_;

  // Network state.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  MyTestNetLog net_log_;
  scoped_ptr<net::NetLogLogger> logger_;
  scoped_ptr<base::Value> net_constants_;
  scoped_ptr<net::HostResolver> host_resolver_;
  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_ptr<net::ServerBoundCertService> system_server_bound_cert_service_;
  scoped_ptr<net::TransportSecurityState> transport_security_state_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
  scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory_;
  scoped_ptr<net::HttpServerPropertiesImpl> http_server_properties_;
  scoped_ptr<net::HostMappingRules> host_mapping_rules_;
  scoped_refptr<net::HttpNetworkSession> network_session_;
  scoped_ptr<net::ProxyService> proxy_service_;

  scoped_ptr<MCSClient> mcs_client_;

  scoped_ptr<ConnectionFactoryImpl> connection_factory_;

  base::Thread file_thread_;

  scoped_ptr<base::RunLoop> run_loop_;
};

MCSProbe::MCSProbe(
    const CommandLine& command_line,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : command_line_(command_line),
      rmq_path_(base::FilePath(FILE_PATH_LITERAL("gcm_rmq_store"))),
      android_id_(0),
      secret_(0),
      server_port_(0),
      url_request_context_getter_(url_request_context_getter),
      file_thread_("FileThread") {
  if (command_line.HasSwitch(kRMQFileName)) {
    rmq_path_ = command_line.GetSwitchValuePath(kRMQFileName);
  }
  if (command_line.HasSwitch(kAndroidIdSwitch)) {
    base::StringToUint64(command_line.GetSwitchValueASCII(kAndroidIdSwitch),
                         &android_id_);
  }
  if (command_line.HasSwitch(kSecretSwitch)) {
    base::StringToUint64(command_line.GetSwitchValueASCII(kSecretSwitch),
                         &secret_);
  }
  server_host_ = kMCSServerHost;
  if (command_line.HasSwitch(kServerHostSwitch)) {
    server_host_ = command_line.GetSwitchValueASCII(kServerHostSwitch);
  }
  server_port_ = kMCSServerPort;
  if (command_line.HasSwitch(kServerPortSwitch)) {
    base::StringToInt(command_line.GetSwitchValueASCII(kServerPortSwitch),
                      &server_port_);
  }
}

MCSProbe::~MCSProbe() {
  file_thread_.Stop();
}

void MCSProbe::Start() {
  file_thread_.Start();
  InitializeNetworkState();
  BuildNetworkSession();
  connection_factory_.reset(
      new ConnectionFactoryImpl(GURL("https://" + net::HostPortPair(
                                    server_host_, server_port_).ToString()),
                                network_session_,
                                &net_log_));
  mcs_client_.reset(new MCSClient(rmq_path_,
                                  connection_factory_.get(),
                                  file_thread_.message_loop_proxy()));
  run_loop_.reset(new base::RunLoop());
  mcs_client_->Initialize(base::Bind(&MCSProbe::InitializationCallback,
                                     base::Unretained(this)),
                          base::Bind(&MessageReceivedCallback),
                          base::Bind(&MessageSentCallback));
  run_loop_->Run();
}

void MCSProbe::InitializeNetworkState() {
  FILE* log_file = NULL;
  if (command_line_.HasSwitch(kLogFileSwitch)) {
    base::FilePath log_path = command_line_.GetSwitchValuePath(kLogFileSwitch);
#if defined(OS_WIN)
    log_file = _wfopen(log_path.value().c_str(), L"w");
#elif defined(OS_POSIX)
    log_file = fopen(log_path.value().c_str(), "w");
#endif
  }
  net_constants_.reset(net::NetLogLogger::GetConstants());
  if (log_file != NULL) {
    logger_.reset(new net::NetLogLogger(log_file, *net_constants_));
    logger_->StartObserving(&net_log_);
  }

  host_resolver_ = net::HostResolver::CreateDefaultResolver(&net_log_);

  if (command_line_.HasSwitch(kIgnoreCertSwitch)) {
    cert_verifier_.reset(new MyTestCertVerifier());
  } else {
    cert_verifier_.reset(net::CertVerifier::CreateDefault());
  }
  system_server_bound_cert_service_.reset(
      new net::ServerBoundCertService(
          new net::DefaultServerBoundCertStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));

  transport_security_state_.reset(new net::TransportSecurityState());
  url_security_manager_.reset(net::URLSecurityManager::Create(NULL, NULL));
  http_auth_handler_factory_.reset(
      net::HttpAuthHandlerRegistryFactory::Create(
          std::vector<std::string>(1, "basic"),
          url_security_manager_.get(),
          host_resolver_.get(),
          std::string(),
          false,
          false));
  http_server_properties_.reset(new net::HttpServerPropertiesImpl());
  host_mapping_rules_.reset(new net::HostMappingRules());
  proxy_service_.reset(net::ProxyService::CreateDirectWithNetLog(&net_log_));
}

void MCSProbe::BuildNetworkSession() {
  net::HttpNetworkSession::Params session_params;
  session_params.host_resolver = host_resolver_.get();
  session_params.cert_verifier = cert_verifier_.get();
  session_params.server_bound_cert_service =
      system_server_bound_cert_service_.get();
  session_params.transport_security_state = transport_security_state_.get();
  session_params.ssl_config_service = new net::SSLConfigServiceDefaults();
  session_params.http_auth_handler_factory = http_auth_handler_factory_.get();
  session_params.http_server_properties =
      http_server_properties_->GetWeakPtr();
  session_params.network_delegate = NULL;  // TODO(zea): implement?
  session_params.host_mapping_rules = host_mapping_rules_.get();
  session_params.ignore_certificate_errors = true;
  session_params.http_pipelining_enabled = false;
  session_params.testing_fixed_http_port = 0;
  session_params.testing_fixed_https_port = 0;
  session_params.net_log = &net_log_;
  session_params.proxy_service = proxy_service_.get();

  network_session_ = new net::HttpNetworkSession(session_params);
}

void MCSProbe::InitializationCallback(bool success,
                                      uint64 restored_android_id,
                                      uint64 restored_security_token) {
  LOG(INFO) << "Initialization " << (success ? "success!" : "failure!");
  if (restored_android_id && restored_security_token) {
    android_id_ = restored_android_id;
    secret_ = restored_security_token;
  }
  if (success)
    mcs_client_->Login(android_id_, secret_);
}

int MCSProbeMain(int argc, char* argv[]) {
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  base::MessageLoopForIO message_loop;

  // For check-in and creating registration ids.
  const scoped_refptr<MyTestURLRequestContextGetter> context_getter =
      new MyTestURLRequestContextGetter(
          base::MessageLoop::current()->message_loop_proxy());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  MCSProbe mcs_probe(command_line, context_getter);
  mcs_probe.Start();

  base::RunLoop run_loop;
  run_loop.Run();

  return 0;
}

}  // namespace
}  // namespace gcm

int main(int argc, char* argv[]) {
  return gcm::MCSProbeMain(argc, argv);
}
