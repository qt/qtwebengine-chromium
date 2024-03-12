// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/socket/connect_job.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

template <typename T>
std::unique_ptr<T> CreateFactoryIfNull(std::unique_ptr<T> in) {
  if (in)
    return in;
  return std::make_unique<T>();
}

bool UsingSsl(const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return GURL::SchemeIsCryptographic(
        base::ToLowerASCII(absl::get<url::SchemeHostPort>(endpoint).scheme()));
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint).using_ssl;
}

HostPortPair ToHostPortPair(const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return HostPortPair::FromSchemeHostPort(
        absl::get<url::SchemeHostPort>(endpoint));
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint)
      .host_port_pair;
}

TransportSocketParams::Endpoint ToTransportEndpoint(
    const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint))
    return absl::get<url::SchemeHostPort>(endpoint);

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint)
      .host_port_pair;
}

base::flat_set<std::string> SupportedProtocolsFromSSLConfig(
    const SSLConfig& config) {
  // We convert because `SSLConfig` uses `NextProto` for ALPN protocols while
  // `TransportConnectJob` and DNS logic needs `std::string`. See
  // https://crbug.com/1286835.
  return base::MakeFlatSet<std::string>(config.alpn_protos, /*comp=*/{},
                                        NextProtoToString);
}

// Populates `ssl_config's` ALPN-related fields. Namely, `alpn_protos`,
// `application_settings`, `renego_allowed_default`, and
// `renego_allowed_for_protos`.
//
// In the case of AlpnMode::kDisabled, clears all of the fields.
//
// In the case of AlpnMode::kHttp11Only sets `alpn_protos` to only allow
// HTTP/1.1 negotiation.
//
// In the case of AlpnMode::kHttpAll, copying `alpn_protos` from
// `common_connect_job_params`, and gives HttpServerProperties a chance to force
// use of HTTP/1.1 only.
//
// If `alpn_mode` is not AlpnMode::kDisabled, then `server` must be a
// SchemeHostPort, as it makes no sense to negotiate ALPN when the scheme isn't
// known.
void ConfigureAlpn(
    const ConnectJobFactory::Endpoint& endpoint,
    ConnectJobFactory::AlpnMode alpn_mode,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const CommonConnectJobParams& common_connect_job_params,
    SSLConfig& ssl_config,
    bool renego_allowed) {
  if (alpn_mode == ConnectJobFactory::AlpnMode::kDisabled) {
    ssl_config.alpn_protos = {};
    ssl_config.application_settings = {};
    ssl_config.renego_allowed_default = false;
    return;
  }

  DCHECK(absl::holds_alternative<url::SchemeHostPort>(endpoint));

  if (alpn_mode == ConnectJobFactory::AlpnMode::kHttp11Only) {
    ssl_config.alpn_protos = {kProtoHTTP11};
    ssl_config.application_settings =
        *common_connect_job_params.application_settings;
  } else {
    DCHECK_EQ(alpn_mode, ConnectJobFactory::AlpnMode::kHttpAll);
    DCHECK(absl::holds_alternative<url::SchemeHostPort>(endpoint));
    ssl_config.alpn_protos = *common_connect_job_params.alpn_protos;
    ssl_config.application_settings =
        *common_connect_job_params.application_settings;
    if (common_connect_job_params.http_server_properties) {
      common_connect_job_params.http_server_properties->MaybeForceHTTP11(
          absl::get<url::SchemeHostPort>(endpoint), network_anonymization_key,
          &ssl_config);
    }
  }

  // Prior to HTTP/2 and SPDY, some servers used TLS renegotiation to request
  // TLS client authentication after the HTTP request was sent. Allow
  // renegotiation for only those connections.
  //
  // Note that this does NOT implement the provision in
  // https://http2.github.io/http2-spec/#rfc.section.9.2.1 which allows the
  // server to request a renegotiation immediately before sending the
  // connection preface as waiting for the preface would cost the round trip
  // that False Start otherwise saves.
  ssl_config.renego_allowed_default = renego_allowed;
  if (renego_allowed) {
    ssl_config.renego_allowed_for_protos = {kProtoHTTP11};
  }
}

}  // namespace

ConnectJobFactory::ConnectJobFactory(
    std::unique_ptr<HttpProxyConnectJob::Factory>
        http_proxy_connect_job_factory,
    std::unique_ptr<SOCKSConnectJob::Factory> socks_connect_job_factory,
    std::unique_ptr<SSLConnectJob::Factory> ssl_connect_job_factory,
    std::unique_ptr<TransportConnectJob::Factory> transport_connect_job_factory)
    : http_proxy_connect_job_factory_(
          CreateFactoryIfNull(std::move(http_proxy_connect_job_factory))),
      socks_connect_job_factory_(
          CreateFactoryIfNull(std::move(socks_connect_job_factory))),
      ssl_connect_job_factory_(
          CreateFactoryIfNull(std::move(ssl_connect_job_factory))),
      transport_connect_job_factory_(
          CreateFactoryIfNull(std::move(transport_connect_job_factory))) {}

ConnectJobFactory::~ConnectJobFactory() = default;

std::unique_ptr<ConnectJob> ConnectJobFactory::CreateConnectJob(
    url::SchemeHostPort endpoint,
    const ProxyChain& proxy_chain,
    const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    ConnectJobFactory::AlpnMode alpn_mode,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool disable_cert_network_fetches,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) const {
  return CreateConnectJob(
      Endpoint(std::move(endpoint)), proxy_chain, proxy_annotation_tag,
      ssl_config_for_origin, alpn_mode, force_tunnel, privacy_mode,
      resolution_callback, request_priority, socket_tag,
      network_anonymization_key, secure_dns_policy,
      disable_cert_network_fetches, common_connect_job_params, delegate);
}

std::unique_ptr<ConnectJob> ConnectJobFactory::CreateConnectJob(
    bool using_ssl,
    HostPortPair endpoint,
    const ProxyChain& proxy_chain,
    const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) const {
  SchemelessEndpoint schemeless_endpoint{using_ssl, std::move(endpoint)};
  return CreateConnectJob(
      std::move(schemeless_endpoint), proxy_chain, proxy_annotation_tag,
      ssl_config_for_origin, ConnectJobFactory::AlpnMode::kDisabled,
      force_tunnel, privacy_mode, resolution_callback, request_priority,
      socket_tag, network_anonymization_key, secure_dns_policy,
      /*disable_cert_network_fetches=*/false, common_connect_job_params,
      delegate);
}

std::unique_ptr<ConnectJob> ConnectJobFactory::CreateConnectJob(
    Endpoint endpoint,
    const ProxyChain& proxy_chain,
    const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    ConnectJobFactory::AlpnMode alpn_mode,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool disable_cert_network_fetches,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) const {
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;
  base::flat_set<std::string> no_alpn_protocols;

  DCHECK(proxy_chain.IsValid());
  if (!proxy_chain.is_direct()) {
    // The first iteration of this loop is taken for all types of proxies and
    // creates a TransportSocketParams and other socket params based on the
    // proxy type. For nested proxies, we then create additional SSLSocketParam
    // and HttpProxySocketParam objects for the remaining hops. This is done by
    // working backwards through the proxy chain and creating socket params
    // such that connect jobs will be created recursively with dependencies in
    // the correct order (in other words, the inner-most connect job will
    // establish a connection to the first proxy, and then that connection
    // will get used to establish a connection to the second proxy).
    for (size_t proxy_index = 0; proxy_index < proxy_chain.length();
         ++proxy_index) {
      const ProxyServer& proxy_server = proxy_chain.GetProxyServer(proxy_index);

      SSLConfig proxy_server_ssl_config;
      if (proxy_server.is_secure_http_like()) {
        // Disable cert verification network fetches for secure proxies, since
        // those network requests are probably going to need to go through the
        // proxy chain too.
        //
        // Any proxy-specific SSL behavior here should also be configured for
        // QUIC proxies.
        //
        proxy_server_ssl_config.disable_cert_verification_network_fetches =
            true;
        ConfigureAlpn(url::SchemeHostPort(url::kHttpsScheme,
                                          proxy_server.host_port_pair().host(),
                                          proxy_server.host_port_pair().port()),
                      // Always enable ALPN for proxies.
                      ConnectJobFactory::AlpnMode::kHttpAll,
                      network_anonymization_key, *common_connect_job_params,
                      proxy_server_ssl_config, /*renego_allowed=*/false);
      }

      scoped_refptr<TransportSocketParams> proxy_tcp_params;
      if (proxy_index == 0) {
        // In the first iteration create the only TransportSocketParams object,
        // corresponding to the transport socket we want to create to the first
        // proxy.
        // TODO(crbug.com/1206799): For an http-like proxy, should this pass a
        // `SchemeHostPort`, so proxies can participate in ECH? Note doing so
        // with `SCHEME_HTTP` requires handling the HTTPS record upgrade.
        proxy_tcp_params = base::MakeRefCounted<TransportSocketParams>(
            proxy_server.host_port_pair(), proxy_dns_network_anonymization_key_,
            secure_dns_policy, resolution_callback,
            proxy_server.is_secure_http_like()
                ? SupportedProtocolsFromSSLConfig(proxy_server_ssl_config)
                : no_alpn_protocols);
      } else {
        // TODO(https://crbug.com/1491092): For now we will assume that proxy
        // chains with multiple proxies must all use HTTPS.
        CHECK(http_proxy_params);
        CHECK(http_proxy_params->ssl_params());
        CHECK(
            proxy_chain.GetProxyServer(proxy_index - 1).is_secure_http_like());
      }

      if (proxy_server.is_http_like()) {
        scoped_refptr<SSLSocketParams> ssl_params;
        if (proxy_server.is_secure_http_like()) {
          // Set `ssl_params`, and unset `proxy_tcp_params`.
          ssl_params = base::MakeRefCounted<SSLSocketParams>(
              std::move(proxy_tcp_params), /*socks_proxy_params=*/nullptr,
              std::move(http_proxy_params), proxy_server.host_port_pair(),
              proxy_server_ssl_config, PRIVACY_MODE_DISABLED,
              network_anonymization_key);
          proxy_tcp_params = nullptr;
        }

        // The endpoint parameter for this HttpProxySocketParams, which is what
        // we will CONNECT to, should correspond to either `endpoint` (for
        // one-hop proxies) or the proxy server at index 1 (for n-hop proxies).
        HostPortPair connect_host_port_pair;
        bool should_tunnel;
        if (proxy_index + 1 == proxy_chain.length()) {
          connect_host_port_pair = ToHostPortPair(endpoint);
          should_tunnel = force_tunnel || UsingSsl(endpoint) ||
                          !proxy_chain.is_get_to_proxy_allowed();
        } else {
          const auto& next_proxy_server =
              proxy_chain.GetProxyServer(proxy_index + 1);
          connect_host_port_pair = next_proxy_server.host_port_pair();
          // TODO(https://crbug.com/1491092): For now we will assume that proxy
          // chains with multiple proxies must all use HTTPS.
          CHECK(next_proxy_server.is_secure_http_like());
          should_tunnel = true;
        }

        // TODO(crbug.com/1206799): Pass `endpoint` directly (preserving
        // scheme when available)?
        http_proxy_params = base::MakeRefCounted<HttpProxySocketParams>(
            std::move(proxy_tcp_params), std::move(ssl_params),
            connect_host_port_pair, proxy_chain, proxy_index, should_tunnel,
            *proxy_annotation_tag, network_anonymization_key,
            secure_dns_policy);
      } else {
        DCHECK(proxy_server.is_socks());
        DCHECK_EQ(1u, proxy_chain.length());
        // TODO(crbug.com/1206799): Pass `endpoint` directly (preserving scheme
        // when available)?
        socks_params = base::MakeRefCounted<SOCKSSocketParams>(
            std::move(proxy_tcp_params),
            proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5,
            ToHostPortPair(endpoint), network_anonymization_key,
            *proxy_annotation_tag);
      }
    }
  }

  // Deal with SSL - which layers on top of any given proxy.
  if (UsingSsl(endpoint)) {
    DCHECK(ssl_config_for_origin);
    scoped_refptr<TransportSocketParams> ssl_tcp_params;

    SSLConfig ssl_config = *ssl_config_for_origin;

    ConfigureAlpn(endpoint, alpn_mode, network_anonymization_key,
                  *common_connect_job_params, ssl_config,
                  /*renego_allowed=*/true);

    ssl_config.disable_cert_verification_network_fetches =
        disable_cert_network_fetches;

    // TODO(https://crbug.com/964642): Also enable 0-RTT for TLS proxies.
    ssl_config.early_data_enabled =
        *common_connect_job_params->enable_early_data;

    if (proxy_chain.is_direct()) {
      ssl_tcp_params = base::MakeRefCounted<TransportSocketParams>(
          ToTransportEndpoint(endpoint), network_anonymization_key,
          secure_dns_policy, resolution_callback,
          SupportedProtocolsFromSSLConfig(ssl_config));
    }
    // TODO(crbug.com/1206799): Pass `endpoint` directly (preserving scheme
    // when available)?
    auto ssl_params = base::MakeRefCounted<SSLSocketParams>(
        std::move(ssl_tcp_params), std::move(socks_params),
        std::move(http_proxy_params), ToHostPortPair(endpoint), ssl_config,
        privacy_mode, network_anonymization_key);
    return ssl_connect_job_factory_->Create(
        request_priority, socket_tag, common_connect_job_params,
        std::move(ssl_params), delegate, /*net_log=*/nullptr);
  }

  // Only SSL/TLS-based endpoints have ALPN protocols.
  if (proxy_chain.is_direct()) {
    auto tcp_params = base::MakeRefCounted<TransportSocketParams>(
        ToTransportEndpoint(endpoint), network_anonymization_key,
        secure_dns_policy, resolution_callback, no_alpn_protocols);
    return transport_connect_job_factory_->Create(
        request_priority, socket_tag, common_connect_job_params, tcp_params,
        delegate, /*net_log=*/nullptr);
  }

  const ProxyServer& first_proxy_server =
      proxy_chain.GetProxyServer(/*chain_index=*/0);
  if (first_proxy_server.is_http_like()) {
    return http_proxy_connect_job_factory_->Create(
        request_priority, socket_tag, common_connect_job_params,
        std::move(http_proxy_params), delegate, /*net_log=*/nullptr);
  }

  DCHECK(first_proxy_server.is_socks());
  return socks_connect_job_factory_->Create(
      request_priority, socket_tag, common_connect_job_params,
      std::move(socks_params), delegate, /*net_log=*/nullptr);
}

}  // namespace net
