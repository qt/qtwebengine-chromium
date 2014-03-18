// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager_impl.h"

#include "base/logging.h"
#include "base/values.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

namespace {

// Appends information about all |socket_pools| to the end of |list|.
template <class MapType>
void AddSocketPoolsToList(base::ListValue* list,
                          const MapType& socket_pools,
                          const std::string& type,
                          bool include_nested_pools) {
  for (typename MapType::const_iterator it = socket_pools.begin();
       it != socket_pools.end(); it++) {
    list->Append(it->second->GetInfoAsValue(it->first.ToString(),
                                            type,
                                            include_nested_pools));
  }
}

}  // namespace

ClientSocketPoolManagerImpl::ClientSocketPoolManagerImpl(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    HostResolver* host_resolver,
    CertVerifier* cert_verifier,
    ServerBoundCertService* server_bound_cert_service,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    const std::string& ssl_session_cache_shard,
    ProxyService* proxy_service,
    SSLConfigService* ssl_config_service,
    HttpNetworkSession::SocketPoolType pool_type)
    : net_log_(net_log),
      socket_factory_(socket_factory),
      host_resolver_(host_resolver),
      cert_verifier_(cert_verifier),
      server_bound_cert_service_(server_bound_cert_service),
      transport_security_state_(transport_security_state),
      cert_transparency_verifier_(cert_transparency_verifier),
      ssl_session_cache_shard_(ssl_session_cache_shard),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      pool_type_(pool_type),
      transport_pool_histograms_("TCP"),
      transport_socket_pool_(new TransportClientSocketPool(
          max_sockets_per_pool(pool_type), max_sockets_per_group(pool_type),
          &transport_pool_histograms_,
          host_resolver,
          socket_factory_,
          net_log)),
      ssl_pool_histograms_("SSL2"),
      ssl_socket_pool_(new SSLClientSocketPool(
          max_sockets_per_pool(pool_type), max_sockets_per_group(pool_type),
          &ssl_pool_histograms_,
          host_resolver,
          cert_verifier,
          server_bound_cert_service,
          transport_security_state,
          cert_transparency_verifier,
          ssl_session_cache_shard,
          socket_factory,
          transport_socket_pool_.get(),
          NULL /* no socks proxy */,
          NULL /* no http proxy */,
          ssl_config_service,
          net_log)),
      transport_for_socks_pool_histograms_("TCPforSOCKS"),
      socks_pool_histograms_("SOCK"),
      transport_for_http_proxy_pool_histograms_("TCPforHTTPProxy"),
      transport_for_https_proxy_pool_histograms_("TCPforHTTPSProxy"),
      ssl_for_https_proxy_pool_histograms_("SSLforHTTPSProxy"),
      http_proxy_pool_histograms_("HTTPProxy"),
      ssl_socket_pool_for_proxies_histograms_("SSLForProxies") {
  CertDatabase::GetInstance()->AddObserver(this);
}

ClientSocketPoolManagerImpl::~ClientSocketPoolManagerImpl() {
  CertDatabase::GetInstance()->RemoveObserver(this);
}

void ClientSocketPoolManagerImpl::FlushSocketPoolsWithError(int error) {
  // Flush the highest level pools first, since higher level pools may release
  // stuff to the lower level pools.

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->FlushWithError(error);

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_https_proxies_.begin();
       it != transport_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_http_proxies_.begin();
       it != transport_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->FlushWithError(error);

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_socks_proxies_.begin();
       it != transport_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  ssl_socket_pool_->FlushWithError(error);
  transport_socket_pool_->FlushWithError(error);
}

void ClientSocketPoolManagerImpl::CloseIdleSockets() {
  // Close sockets in the highest level pools first, since higher level pools'
  // sockets may release stuff to the lower level pools.
  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_https_proxies_.begin();
       it != transport_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_http_proxies_.begin();
       it != transport_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_socks_proxies_.begin();
       it != transport_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  ssl_socket_pool_->CloseIdleSockets();
  transport_socket_pool_->CloseIdleSockets();
}

TransportClientSocketPool*
ClientSocketPoolManagerImpl::GetTransportSocketPool() {
  return transport_socket_pool_.get();
}

SSLClientSocketPool* ClientSocketPoolManagerImpl::GetSSLSocketPool() {
  return ssl_socket_pool_.get();
}

SOCKSClientSocketPool* ClientSocketPoolManagerImpl::GetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy) {
  SOCKSSocketPoolMap::const_iterator it = socks_socket_pools_.find(socks_proxy);
  if (it != socks_socket_pools_.end()) {
    DCHECK(ContainsKey(transport_socket_pools_for_socks_proxies_, socks_proxy));
    return it->second;
  }

  DCHECK(!ContainsKey(transport_socket_pools_for_socks_proxies_, socks_proxy));

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_ret =
      transport_socket_pools_for_socks_proxies_.insert(
          std::make_pair(
              socks_proxy,
              new TransportClientSocketPool(
                  max_sockets_per_proxy_server(pool_type_),
                  max_sockets_per_group(pool_type_),
                  &transport_for_socks_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_ret.second);

  std::pair<SOCKSSocketPoolMap::iterator, bool> ret =
      socks_socket_pools_.insert(
          std::make_pair(socks_proxy, new SOCKSClientSocketPool(
              max_sockets_per_proxy_server(pool_type_),
              max_sockets_per_group(pool_type_),
              &socks_pool_histograms_,
              host_resolver_,
              tcp_ret.first->second,
              net_log_)));

  return ret.first->second;
}

HttpProxyClientSocketPool*
ClientSocketPoolManagerImpl::GetSocketPoolForHTTPProxy(
    const HostPortPair& http_proxy) {
  HTTPProxySocketPoolMap::const_iterator it =
      http_proxy_socket_pools_.find(http_proxy);
  if (it != http_proxy_socket_pools_.end()) {
    DCHECK(ContainsKey(transport_socket_pools_for_http_proxies_, http_proxy));
    DCHECK(ContainsKey(transport_socket_pools_for_https_proxies_, http_proxy));
    DCHECK(ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));
    return it->second;
  }

  DCHECK(!ContainsKey(transport_socket_pools_for_http_proxies_, http_proxy));
  DCHECK(!ContainsKey(transport_socket_pools_for_https_proxies_, http_proxy));
  DCHECK(!ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_http_ret =
      transport_socket_pools_for_http_proxies_.insert(
          std::make_pair(
              http_proxy,
              new TransportClientSocketPool(
                  max_sockets_per_proxy_server(pool_type_),
                  max_sockets_per_group(pool_type_),
                  &transport_for_http_proxy_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_http_ret.second);

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_https_ret =
      transport_socket_pools_for_https_proxies_.insert(
          std::make_pair(
              http_proxy,
              new TransportClientSocketPool(
                  max_sockets_per_proxy_server(pool_type_),
                  max_sockets_per_group(pool_type_),
                  &transport_for_https_proxy_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<SSLSocketPoolMap::iterator, bool> ssl_https_ret =
      ssl_socket_pools_for_https_proxies_.insert(std::make_pair(
          http_proxy,
          new SSLClientSocketPool(max_sockets_per_proxy_server(pool_type_),
                                  max_sockets_per_group(pool_type_),
                                  &ssl_for_https_proxy_pool_histograms_,
                                  host_resolver_,
                                  cert_verifier_,
                                  server_bound_cert_service_,
                                  transport_security_state_,
                                  cert_transparency_verifier_,
                                  ssl_session_cache_shard_,
                                  socket_factory_,
                                  tcp_https_ret.first->second /* https proxy */,
                                  NULL /* no socks proxy */,
                                  NULL /* no http proxy */,
                                  ssl_config_service_.get(),
                                  net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<HTTPProxySocketPoolMap::iterator, bool> ret =
      http_proxy_socket_pools_.insert(
          std::make_pair(
              http_proxy,
              new HttpProxyClientSocketPool(
                  max_sockets_per_proxy_server(pool_type_),
                  max_sockets_per_group(pool_type_),
                  &http_proxy_pool_histograms_,
                  host_resolver_,
                  tcp_http_ret.first->second,
                  ssl_https_ret.first->second,
                  net_log_)));

  return ret.first->second;
}

SSLClientSocketPool* ClientSocketPoolManagerImpl::GetSocketPoolForSSLWithProxy(
    const HostPortPair& proxy_server) {
  SSLSocketPoolMap::const_iterator it =
      ssl_socket_pools_for_proxies_.find(proxy_server);
  if (it != ssl_socket_pools_for_proxies_.end())
    return it->second;

  SSLClientSocketPool* new_pool = new SSLClientSocketPool(
      max_sockets_per_proxy_server(pool_type_),
      max_sockets_per_group(pool_type_),
      &ssl_pool_histograms_,
      host_resolver_,
      cert_verifier_,
      server_bound_cert_service_,
      transport_security_state_,
      cert_transparency_verifier_,
      ssl_session_cache_shard_,
      socket_factory_,
      NULL, /* no tcp pool, we always go through a proxy */
      GetSocketPoolForSOCKSProxy(proxy_server),
      GetSocketPoolForHTTPProxy(proxy_server),
      ssl_config_service_.get(),
      net_log_);

  std::pair<SSLSocketPoolMap::iterator, bool> ret =
      ssl_socket_pools_for_proxies_.insert(std::make_pair(proxy_server,
                                                          new_pool));

  return ret.first->second;
}

base::Value* ClientSocketPoolManagerImpl::SocketPoolInfoToValue() const {
  base::ListValue* list = new base::ListValue();
  list->Append(transport_socket_pool_->GetInfoAsValue("transport_socket_pool",
                                                "transport_socket_pool",
                                                false));
  // Third parameter is false because |ssl_socket_pool_| uses
  // |transport_socket_pool_| internally, and do not want to add it a second
  // time.
  list->Append(ssl_socket_pool_->GetInfoAsValue("ssl_socket_pool",
                                                "ssl_socket_pool",
                                                false));
  AddSocketPoolsToList(list,
                       http_proxy_socket_pools_,
                       "http_proxy_socket_pool",
                       true);
  AddSocketPoolsToList(list,
                       socks_socket_pools_,
                       "socks_socket_pool",
                       true);

  // Third parameter is false because |ssl_socket_pools_for_proxies_| use
  // socket pools in |http_proxy_socket_pools_| and |socks_socket_pools_|.
  AddSocketPoolsToList(list,
                       ssl_socket_pools_for_proxies_,
                       "ssl_socket_pool_for_proxies",
                       false);
  return list;
}

void ClientSocketPoolManagerImpl::OnCertAdded(const X509Certificate* cert) {
  FlushSocketPoolsWithError(ERR_NETWORK_CHANGED);
}

void ClientSocketPoolManagerImpl::OnCACertChanged(
    const X509Certificate* cert) {
  // We should flush the socket pools if we removed trust from a
  // cert, because a previously trusted server may have become
  // untrusted.
  //
  // We should not flush the socket pools if we added trust to a
  // cert.
  //
  // Since the OnCACertChanged method doesn't tell us what
  // kind of change it is, we have to flush the socket
  // pools to be safe.
  FlushSocketPoolsWithError(ERR_NETWORK_CHANGED);
}

}  // namespace net
