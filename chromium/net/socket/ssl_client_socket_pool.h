// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/privacy_mode.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class CertVerifier;
class ClientSocketFactory;
class ConnectJobFactory;
class HostPortPair;
class HttpProxyClientSocketPool;
class HttpProxySocketParams;
class SOCKSClientSocketPool;
class SOCKSSocketParams;
class SSLClientSocket;
class TransportClientSocketPool;
class TransportSecurityState;
class TransportSocketParams;

// SSLSocketParams only needs the socket params for the transport socket
// that will be used (denoted by |proxy|).
class NET_EXPORT_PRIVATE SSLSocketParams
    : public base::RefCounted<SSLSocketParams> {
 public:
  SSLSocketParams(const scoped_refptr<TransportSocketParams>& transport_params,
                  const scoped_refptr<SOCKSSocketParams>& socks_params,
                  const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
                  ProxyServer::Scheme proxy,
                  const HostPortPair& host_and_port,
                  const SSLConfig& ssl_config,
                  PrivacyMode privacy_mode,
                  int load_flags,
                  bool force_spdy_over_ssl,
                  bool want_spdy_over_npn);

  const scoped_refptr<TransportSocketParams>& transport_params() {
      return transport_params_;
  }
  const scoped_refptr<HttpProxySocketParams>& http_proxy_params() {
    return http_proxy_params_;
  }
  const scoped_refptr<SOCKSSocketParams>& socks_params() {
    return socks_params_;
  }
  ProxyServer::Scheme proxy() const { return proxy_; }
  const HostPortPair& host_and_port() const { return host_and_port_; }
  const SSLConfig& ssl_config() const { return ssl_config_; }
  PrivacyMode privacy_mode() const { return privacy_mode_; }
  int load_flags() const { return load_flags_; }
  bool force_spdy_over_ssl() const { return force_spdy_over_ssl_; }
  bool want_spdy_over_npn() const { return want_spdy_over_npn_; }
  bool ignore_limits() const { return ignore_limits_; }

 private:
  friend class base::RefCounted<SSLSocketParams>;
  ~SSLSocketParams();

  const scoped_refptr<TransportSocketParams> transport_params_;
  const scoped_refptr<HttpProxySocketParams> http_proxy_params_;
  const scoped_refptr<SOCKSSocketParams> socks_params_;
  const ProxyServer::Scheme proxy_;
  const HostPortPair host_and_port_;
  const SSLConfig ssl_config_;
  const PrivacyMode privacy_mode_;
  const int load_flags_;
  const bool force_spdy_over_ssl_;
  const bool want_spdy_over_npn_;
  bool ignore_limits_;

  DISALLOW_COPY_AND_ASSIGN(SSLSocketParams);
};

// SSLConnectJob handles the SSL handshake after setting up the underlying
// connection as specified in the params.
class SSLConnectJob : public ConnectJob {
 public:
  SSLConnectJob(
      const std::string& group_name,
      const scoped_refptr<SSLSocketParams>& params,
      const base::TimeDelta& timeout_duration,
      TransportClientSocketPool* transport_pool,
      SOCKSClientSocketPool* socks_pool,
      HttpProxyClientSocketPool* http_proxy_pool,
      ClientSocketFactory* client_socket_factory,
      HostResolver* host_resolver,
      const SSLClientSocketContext& context,
      Delegate* delegate,
      NetLog* net_log);
  virtual ~SSLConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const OVERRIDE;

  virtual void GetAdditionalErrorState(ClientSocketHandle * handle) OVERRIDE;

 private:
  enum State {
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_SOCKS_CONNECT,
    STATE_SOCKS_CONNECT_COMPLETE,
    STATE_TUNNEL_CONNECT,
    STATE_TUNNEL_CONNECT_COMPLETE,
    STATE_SSL_CONNECT,
    STATE_SSL_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoTransportConnect();
  int DoTransportConnectComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);
  int DoTunnelConnect();
  int DoTunnelConnectComplete(int result);
  int DoSSLConnect();
  int DoSSLConnectComplete(int result);

  // Starts the SSL connection process.  Returns OK on success and
  // ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int ConnectInternal() OVERRIDE;

  scoped_refptr<SSLSocketParams> params_;
  TransportClientSocketPool* const transport_pool_;
  SOCKSClientSocketPool* const socks_pool_;
  HttpProxyClientSocketPool* const http_proxy_pool_;
  ClientSocketFactory* const client_socket_factory_;
  HostResolver* const host_resolver_;

  const SSLClientSocketContext context_;

  State next_state_;
  CompletionCallback callback_;
  scoped_ptr<ClientSocketHandle> transport_socket_handle_;
  scoped_ptr<SSLClientSocket> ssl_socket_;

  HttpResponseInfo error_response_info_;

  DISALLOW_COPY_AND_ASSIGN(SSLConnectJob);
};

class NET_EXPORT_PRIVATE SSLClientSocketPool
    : public ClientSocketPool,
      public LayeredPool,
      public SSLConfigService::Observer {
 public:
  // Only the pools that will be used are required. i.e. if you never
  // try to create an SSL over SOCKS socket, |socks_pool| may be NULL.
  SSLClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      HostResolver* host_resolver,
      CertVerifier* cert_verifier,
      ServerBoundCertService* server_bound_cert_service,
      TransportSecurityState* transport_security_state,
      const std::string& ssl_session_cache_shard,
      ClientSocketFactory* client_socket_factory,
      TransportClientSocketPool* transport_pool,
      SOCKSClientSocketPool* socks_pool,
      HttpProxyClientSocketPool* http_proxy_pool,
      SSLConfigService* ssl_config_service,
      NetLog* net_log);

  virtual ~SSLClientSocketPool();

  // ClientSocketPool implementation.
  virtual int RequestSocket(const std::string& group_name,
                            const void* connect_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            const CompletionCallback& callback,
                            const BoundNetLog& net_log) OVERRIDE;

  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) OVERRIDE;

  virtual void ReleaseSocket(const std::string& group_name,
                             StreamSocket* socket,
                             int id) OVERRIDE;

  virtual void FlushWithError(int error) OVERRIDE;

  virtual bool IsStalled() const OVERRIDE;

  virtual void CloseIdleSockets() OVERRIDE;

  virtual int IdleSocketCount() const OVERRIDE;

  virtual int IdleSocketCountInGroup(
      const std::string& group_name) const OVERRIDE;

  virtual LoadState GetLoadState(
      const std::string& group_name,
      const ClientSocketHandle* handle) const OVERRIDE;

  virtual void AddLayeredPool(LayeredPool* layered_pool) OVERRIDE;

  virtual void RemoveLayeredPool(LayeredPool* layered_pool) OVERRIDE;

  virtual base::DictionaryValue* GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const OVERRIDE;

  virtual base::TimeDelta ConnectionTimeout() const OVERRIDE;

  virtual ClientSocketPoolHistograms* histograms() const OVERRIDE;

  // LayeredPool implementation.
  virtual bool CloseOneIdleConnection() OVERRIDE;

 private:
  typedef ClientSocketPoolBase<SSLSocketParams> PoolBase;

  // SSLConfigService::Observer implementation.

  // When the user changes the SSL config, we flush all idle sockets so they
  // won't get re-used.
  virtual void OnSSLConfigChanged() OVERRIDE;

  class SSLConnectJobFactory : public PoolBase::ConnectJobFactory {
   public:
    SSLConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        SOCKSClientSocketPool* socks_pool,
        HttpProxyClientSocketPool* http_proxy_pool,
        ClientSocketFactory* client_socket_factory,
        HostResolver* host_resolver,
        const SSLClientSocketContext& context,
        NetLog* net_log);

    virtual ~SSLConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.
    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const OVERRIDE;

    virtual base::TimeDelta ConnectionTimeout() const OVERRIDE;

   private:
    TransportClientSocketPool* const transport_pool_;
    SOCKSClientSocketPool* const socks_pool_;
    HttpProxyClientSocketPool* const http_proxy_pool_;
    ClientSocketFactory* const client_socket_factory_;
    HostResolver* const host_resolver_;
    const SSLClientSocketContext context_;
    base::TimeDelta timeout_;
    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(SSLConnectJobFactory);
  };

  TransportClientSocketPool* const transport_pool_;
  SOCKSClientSocketPool* const socks_pool_;
  HttpProxyClientSocketPool* const http_proxy_pool_;
  PoolBase base_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientSocketPool);
};

REGISTER_SOCKET_PARAMS_FOR_POOL(SSLClientSocketPool, SSLSocketParams);

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_
