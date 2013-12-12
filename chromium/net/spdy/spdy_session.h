// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_H_
#define NET_SPDY_SPDY_SESSION_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_buffer.h"
#include "net/spdy/spdy_credential_state.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_write_queue.h"
#include "net/ssl/ssl_config_service.h"
#include "url/gurl.h"

namespace net {

// This is somewhat arbitrary and not really fixed, but it will always work
// reasonably with ethernet. Chop the world into 2-packet chunks.  This is
// somewhat arbitrary, but is reasonably small and ensures that we elicit
// ACKs quickly from TCP (because TCP tries to only ACK every other packet).
const int kMss = 1430;
// The 8 is the size of the SPDY frame header.
const int kMaxSpdyFrameChunkSize = (2 * kMss) - 8;

// Maximum number of concurrent streams we will create, unless the server
// sends a SETTINGS frame with a different value.
const size_t kInitialMaxConcurrentStreams = 100;

// Specifies the maxiumum concurrent streams server could send (via push).
const int kMaxConcurrentPushedStreams = 1000;

// Specifies the maximum number of bytes to read synchronously before
// yielding.
const int kMaxReadBytesWithoutYielding = 32 * 1024;

// The initial receive window size for both streams and sessions.
const int32 kDefaultInitialRecvWindowSize = 10 * 1024 * 1024;  // 10MB

class BoundNetLog;
struct LoadTimingInfo;
class SpdyStream;
class SSLInfo;

// NOTE: There's an enum of the same name (also with numeric suffixes)
// in histograms.xml.
//
// WARNING: DO NOT INSERT ENUMS INTO THIS LIST! Add only to the end.
enum SpdyProtocolErrorDetails {
  // SpdyFramer::SpdyErrors
  SPDY_ERROR_NO_ERROR,
  SPDY_ERROR_INVALID_CONTROL_FRAME,
  SPDY_ERROR_CONTROL_PAYLOAD_TOO_LARGE,
  SPDY_ERROR_ZLIB_INIT_FAILURE,
  SPDY_ERROR_UNSUPPORTED_VERSION,
  SPDY_ERROR_DECOMPRESS_FAILURE,
  SPDY_ERROR_COMPRESS_FAILURE,
  SPDY_ERROR_CREDENTIAL_FRAME_CORRUPT,
  SPDY_ERROR_INVALID_DATA_FRAME_FLAGS,
  SPDY_ERROR_INVALID_CONTROL_FRAME_FLAGS,
  // SpdyRstStreamStatus
  STATUS_CODE_INVALID,
  STATUS_CODE_PROTOCOL_ERROR,
  STATUS_CODE_INVALID_STREAM,
  STATUS_CODE_REFUSED_STREAM,
  STATUS_CODE_UNSUPPORTED_VERSION,
  STATUS_CODE_CANCEL,
  STATUS_CODE_INTERNAL_ERROR,
  STATUS_CODE_FLOW_CONTROL_ERROR,
  STATUS_CODE_STREAM_IN_USE,
  STATUS_CODE_STREAM_ALREADY_CLOSED,
  STATUS_CODE_INVALID_CREDENTIALS,
  STATUS_CODE_FRAME_TOO_LARGE,
  // SpdySession errors
  PROTOCOL_ERROR_UNEXPECTED_PING,
  PROTOCOL_ERROR_RST_STREAM_FOR_NON_ACTIVE_STREAM,
  PROTOCOL_ERROR_SPDY_COMPRESSION_FAILURE,
  PROTOCOL_ERROR_REQUEST_FOR_SECURE_CONTENT_OVER_INSECURE_SESSION,
  PROTOCOL_ERROR_SYN_REPLY_NOT_RECEIVED,
  PROTOCOL_ERROR_INVALID_WINDOW_UPDATE_SIZE,
  PROTOCOL_ERROR_RECEIVE_WINDOW_VIOLATION,
  NUM_SPDY_PROTOCOL_ERROR_DETAILS
};

COMPILE_ASSERT(STATUS_CODE_INVALID ==
               static_cast<SpdyProtocolErrorDetails>(SpdyFramer::LAST_ERROR),
               SpdyProtocolErrorDetails_SpdyErrors_mismatch);

COMPILE_ASSERT(PROTOCOL_ERROR_UNEXPECTED_PING ==
               static_cast<SpdyProtocolErrorDetails>(
                   RST_STREAM_NUM_STATUS_CODES + STATUS_CODE_INVALID),
               SpdyProtocolErrorDetails_SpdyErrors_mismatch);

// A helper class used to manage a request to create a stream.
class NET_EXPORT_PRIVATE SpdyStreamRequest {
 public:
  SpdyStreamRequest();
  // Calls CancelRequest().
  ~SpdyStreamRequest();

  // Starts the request to create a stream. If OK is returned, then
  // ReleaseStream() may be called. If ERR_IO_PENDING is returned,
  // then when the stream is created, |callback| will be called, at
  // which point ReleaseStream() may be called. Otherwise, the stream
  // is not created, an error is returned, and ReleaseStream() may not
  // be called.
  //
  // If OK is returned, must not be called again without
  // ReleaseStream() being called first. If ERR_IO_PENDING is
  // returned, must not be called again without CancelRequest() or
  // ReleaseStream() being called first. Otherwise, in case of an
  // immediate error, this may be called again.
  int StartRequest(SpdyStreamType type,
                   const base::WeakPtr<SpdySession>& session,
                   const GURL& url,
                   RequestPriority priority,
                   const BoundNetLog& net_log,
                   const CompletionCallback& callback);

  // Cancels any pending stream creation request. May be called
  // repeatedly.
  void CancelRequest();

  // Transfers the created stream (guaranteed to not be NULL) to the
  // caller. Must be called at most once after StartRequest() returns
  // OK or |callback| is called with OK. The caller must immediately
  // set a delegate for the returned stream (except for test code).
  base::WeakPtr<SpdyStream> ReleaseStream();

 private:
  friend class SpdySession;

  // Called by |session_| when the stream attempt has finished
  // successfully.
  void OnRequestCompleteSuccess(const base::WeakPtr<SpdyStream>& stream);

  // Called by |session_| when the stream attempt has finished with an
  // error. Also called with ERR_ABORTED if |session_| is destroyed
  // while the stream attempt is still pending.
  void OnRequestCompleteFailure(int rv);

  // Accessors called by |session_|.
  SpdyStreamType type() const { return type_; }
  const GURL& url() const { return url_; }
  RequestPriority priority() const { return priority_; }
  const BoundNetLog& net_log() const { return net_log_; }

  void Reset();

  base::WeakPtrFactory<SpdyStreamRequest> weak_ptr_factory_;
  SpdyStreamType type_;
  base::WeakPtr<SpdySession> session_;
  base::WeakPtr<SpdyStream> stream_;
  GURL url_;
  RequestPriority priority_;
  BoundNetLog net_log_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SpdyStreamRequest);
};

class NET_EXPORT SpdySession : public BufferedSpdyFramerVisitorInterface,
                               public SpdyFramerDebugVisitorInterface,
                               public HigherLayeredPool {
 public:
  // TODO(akalin): Use base::TickClock when it becomes available.
  typedef base::TimeTicks (*TimeFunc)(void);

  // How we handle flow control (version-dependent).
  enum FlowControlState {
    FLOW_CONTROL_NONE,
    FLOW_CONTROL_STREAM,
    FLOW_CONTROL_STREAM_AND_SESSION
  };

  // Create a new SpdySession.
  // |spdy_session_key| is the host/port that this session connects to, privacy
  // and proxy configuration settings that it's using.
  // |session| is the HttpNetworkSession.  |net_log| is the NetLog that we log
  // network events to.
  SpdySession(const SpdySessionKey& spdy_session_key,
              const base::WeakPtr<HttpServerProperties>& http_server_properties,
              bool verify_domain_authentication,
              bool enable_sending_initial_data,
              bool enable_credential_frames,
              bool enable_compression,
              bool enable_ping_based_connection_checking,
              NextProto default_protocol,
              size_t stream_initial_recv_window_size,
              size_t initial_max_concurrent_streams,
              size_t max_concurrent_streams_limit,
              TimeFunc time_func,
              const HostPortPair& trusted_spdy_proxy,
              NetLog* net_log);

  virtual ~SpdySession();

  const HostPortPair& host_port_pair() const {
    return spdy_session_key_.host_port_proxy_pair().first;
  }
  const HostPortProxyPair& host_port_proxy_pair() const {
    return spdy_session_key_.host_port_proxy_pair();
  }
  const SpdySessionKey& spdy_session_key() const {
    return spdy_session_key_;
  }
  // Get a pushed stream for a given |url|.  If the server initiates a
  // stream, it might already exist for a given path.  The server
  // might also not have initiated the stream yet, but indicated it
  // will via X-Associated-Content.  Returns OK if a stream was found
  // and put into |spdy_stream|, or if one was not found but it is
  // okay to create a new stream (in which case |spdy_stream| is
  // reset).  Returns an error (not ERR_IO_PENDING) otherwise, and
  // resets |spdy_stream|.
  int GetPushStream(
      const GURL& url,
      base::WeakPtr<SpdyStream>* spdy_stream,
      const BoundNetLog& stream_net_log);

  // Initialize the session with the given connection. |is_secure|
  // must indicate whether |connection| uses an SSL socket or not; it
  // is usually true, but it can be false for testing or when SPDY is
  // configured to work with non-secure sockets.
  //
  // |pool| is the SpdySessionPool that owns us.  Its lifetime must
  // strictly be greater than |this|.
  //
  // |certificate_error_code| must either be OK or less than
  // ERR_IO_PENDING.
  //
  // Returns OK on success, or an error on failure. Never returns
  // ERR_IO_PENDING. If an error is returned, the session must be
  // destroyed immediately.
  Error InitializeWithSocket(scoped_ptr<ClientSocketHandle> connection,
                             SpdySessionPool* pool,
                             bool is_secure,
                             int certificate_error_code);

  // Returns the protocol used by this session. Always between
  // kProtoSPDY2 and kProtoSPDYMaximumVersion.
  //
  // TODO(akalin): Change the lower bound to kProtoSPDYMinimumVersion
  // once we stop supporting SPDY/1.
  NextProto protocol() const { return protocol_; }

  // Check to see if this SPDY session can support an additional domain.
  // If the session is un-authenticated, then this call always returns true.
  // For SSL-based sessions, verifies that the server certificate in use by
  // this session provides authentication for the domain and no client
  // certificate or channel ID was sent to the original server during the SSL
  // handshake.  NOTE:  This function can have false negatives on some
  // platforms.
  // TODO(wtc): rename this function and the Net.SpdyIPPoolDomainMatch
  // histogram because this function does more than verifying domain
  // authentication now.
  bool VerifyDomainAuthentication(const std::string& domain);

  // Pushes the given producer into the write queue for
  // |stream|. |stream| is guaranteed to be activated before the
  // producer is used to produce its frame.
  void EnqueueStreamWrite(const base::WeakPtr<SpdyStream>& stream,
                          SpdyFrameType frame_type,
                          scoped_ptr<SpdyBufferProducer> producer);

  // Creates and returns a SYN frame for |stream_id|.
  scoped_ptr<SpdyFrame> CreateSynStream(
      SpdyStreamId stream_id,
      RequestPriority priority,
      uint8 credential_slot,
      SpdyControlFlags flags,
      const SpdyHeaderBlock& headers);

  // Tries to create a CREDENTIAL frame. If successful, fills in
  // |credential_frame| and returns OK. Returns the error (guaranteed
  // to not be ERR_IO_PENDING) otherwise.
  int CreateCredentialFrame(const std::string& origin,
                            const std::string& key,
                            const std::string& cert,
                            RequestPriority priority,
                            scoped_ptr<SpdyFrame>* credential_frame);

  // Creates and returns a SpdyBuffer holding a data frame with the
  // given data. May return NULL if stalled by flow control.
  scoped_ptr<SpdyBuffer> CreateDataBuffer(SpdyStreamId stream_id,
                                          IOBuffer* data,
                                          int len,
                                          SpdyDataFlags flags);

  // Close the stream with the given ID, which must exist and be
  // active. Note that that stream may hold the last reference to the
  // session.
  void CloseActiveStream(SpdyStreamId stream_id, int status);

  // Close the given created stream, which must exist but not yet be
  // active. Note that |stream| may hold the last reference to the
  // session.
  void CloseCreatedStream(const base::WeakPtr<SpdyStream>& stream, int status);

  // Send a RST_STREAM frame with the given status code and close the
  // stream with the given ID, which must exist and be active. Note
  // that that stream may hold the last reference to the session.
  void ResetStream(SpdyStreamId stream_id,
                   SpdyRstStreamStatus status,
                   const std::string& description);

  // Check if a stream is active.
  bool IsStreamActive(SpdyStreamId stream_id) const;

  // The LoadState is used for informing the user of the current network
  // status, such as "resolving host", "connecting", etc.
  LoadState GetLoadState() const;

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  bool GetSSLInfo(SSLInfo* ssl_info,
                  bool* was_npn_negotiated,
                  NextProto* protocol_negotiated);

  // Fills SSL Certificate Request info |cert_request_info| and returns
  // true when SSL is in use.
  bool GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);

  // Returns the ServerBoundCertService used by this Socket, or NULL
  // if server bound certs are not supported in this session.
  ServerBoundCertService* GetServerBoundCertService() const;

  // Send a WINDOW_UPDATE frame for a stream. Called by a stream
  // whenever receive window size is increased.
  void SendStreamWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size);

  // Whether the stream is closed, i.e. it has stopped processing data
  // and is about to be destroyed.
  //
  // TODO(akalin): This is only used in tests. Remove this function
  // and have tests test the WeakPtr instead.
  bool IsClosed() const { return availability_state_ == STATE_CLOSED; }

  // Closes this session. This will close all active streams and mark
  // the session as permanently closed. Callers must assume that the
  // session is destroyed after this is called. (However, it may not
  // be destroyed right away, e.g. when a SpdySession function is
  // present in the call stack.)
  //
  // |err| should be < ERR_IO_PENDING; this function is intended to be
  // called on error.
  // |description| indicates the reason for the error.
  void CloseSessionOnError(Error err, const std::string& description);

  // Retrieves information on the current state of the SPDY session as a
  // Value.  Caller takes possession of the returned value.
  base::Value* GetInfoAsValue() const;

  // Indicates whether the session is being reused after having successfully
  // used to send/receive data in the past.
  bool IsReused() const;

  // Returns true if the underlying transport socket ever had any reads or
  // writes.
  bool WasEverUsed() const {
    return connection_->socket()->WasEverUsed();
  }

  // Returns the load timing information from the perspective of the given
  // stream.  If it's not the first stream, the connection is considered reused
  // for that stream.
  //
  // This uses a different notion of reuse than IsReused().  This function
  // sets |socket_reused| to false only if |stream_id| is the ID of the first
  // stream using the session.  IsReused(), on the other hand, indicates if the
  // session has been used to send/receive data at all.
  bool GetLoadTimingInfo(SpdyStreamId stream_id,
                         LoadTimingInfo* load_timing_info) const;

  // Returns true if session is not currently active
  bool is_active() const {
    return !active_streams_.empty() || !created_streams_.empty();
  }

  // Access to the number of active and pending streams.  These are primarily
  // available for testing and diagnostics.
  size_t num_active_streams() const { return active_streams_.size(); }
  size_t num_unclaimed_pushed_streams() const {
      return unclaimed_pushed_streams_.size();
  }
  size_t num_created_streams() const { return created_streams_.size(); }

  size_t pending_create_stream_queue_size(RequestPriority priority) const {
    DCHECK_LT(priority, NUM_PRIORITIES);
    return pending_create_stream_queues_[priority].size();
  }

  // Returns the (version-dependent) flow control state.
  FlowControlState flow_control_state() const {
    return flow_control_state_;
  }

  // Returns the current |stream_initial_send_window_size_|.
  int32 stream_initial_send_window_size() const {
    return stream_initial_send_window_size_;
  }

  // Returns the current |stream_initial_recv_window_size_|.
  int32 stream_initial_recv_window_size() const {
    return stream_initial_recv_window_size_;
  }

  // Returns true if no stream in the session can send data due to
  // session flow control.
  bool IsSendStalled() const {
    return
        flow_control_state_ == FLOW_CONTROL_STREAM_AND_SESSION &&
        session_send_window_size_ == 0;
  }

  const BoundNetLog& net_log() const { return net_log_; }

  int GetPeerAddress(IPEndPoint* address) const;
  int GetLocalAddress(IPEndPoint* address) const;

  // Returns true if requests on this session require credentials.
  bool NeedsCredentials() const;

  SpdyCredentialState* credential_state() { return &credential_state_; }

  // Adds |alias| to set of aliases associated with this session.
  void AddPooledAlias(const SpdySessionKey& alias_key);

  // Returns the set of aliases associated with this session.
  const std::set<SpdySessionKey>& pooled_aliases() const {
    return pooled_aliases_;
  }

  int GetProtocolVersion() const;

  size_t GetDataFrameMinimumSize() const {
    return buffered_spdy_framer_->GetDataFrameMinimumSize();
  }

  size_t GetControlFrameHeaderSize() const {
    return buffered_spdy_framer_->GetControlFrameHeaderSize();
  }

  size_t GetFrameMinimumSize() const {
    return buffered_spdy_framer_->GetFrameMinimumSize();
  }

  size_t GetFrameMaximumSize() const {
    return buffered_spdy_framer_->GetFrameMaximumSize();
  }

  size_t GetDataFrameMaximumPayload() const {
    return buffered_spdy_framer_->GetDataFrameMaximumPayload();
  }

  // Must be used only by |pool_|.
  base::WeakPtr<SpdySession> GetWeakPtr();

  // HigherLayeredPool implementation:
  virtual bool CloseOneIdleConnection() OVERRIDE;

 private:
  friend class base::RefCounted<SpdySession>;
  friend class SpdyStreamRequest;
  friend class SpdySessionTest;

  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, ClientPing);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, FailedPing);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, GetActivePushStream);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, DeleteExpiredPushStreams);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, ProtocolNegotiation);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, ClearSettings);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, AdjustRecvWindowSize);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, AdjustSendWindowSize);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, SessionFlowControlInactiveStream);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, SessionFlowControlNoReceiveLeaks);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, SessionFlowControlNoSendLeaks);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, SessionFlowControlEndToEnd);

  typedef std::deque<base::WeakPtr<SpdyStreamRequest> >
      PendingStreamRequestQueue;

  struct ActiveStreamInfo {
    ActiveStreamInfo();
    explicit ActiveStreamInfo(SpdyStream* stream);
    ~ActiveStreamInfo();

    SpdyStream* stream;
    bool waiting_for_syn_reply;
  };
  typedef std::map<SpdyStreamId, ActiveStreamInfo> ActiveStreamMap;

  struct PushedStreamInfo {
    PushedStreamInfo();
    PushedStreamInfo(SpdyStreamId stream_id, base::TimeTicks creation_time);
    ~PushedStreamInfo();

    SpdyStreamId stream_id;
    base::TimeTicks creation_time;
  };
  typedef std::map<GURL, PushedStreamInfo> PushedStreamMap;

  typedef std::set<SpdyStream*> CreatedStreamSet;

  enum AvailabilityState {
    // The session is available in its socket pool and can be used
    // freely.
    STATE_AVAILABLE,
    // The session can process data on existing streams but will
    // refuse to create new ones.
    STATE_GOING_AWAY,
    // The session has been closed, is waiting to be deleted, and will
    // refuse to process any more data.
    STATE_CLOSED
  };

  enum ReadState {
    READ_STATE_DO_READ,
    READ_STATE_DO_READ_COMPLETE,
  };

  enum WriteState {
    // There is no in-flight write and the write queue is empty.
    WRITE_STATE_IDLE,
    WRITE_STATE_DO_WRITE,
    WRITE_STATE_DO_WRITE_COMPLETE,
  };

  // The return value of DoCloseSession() describing what was done.
  enum CloseSessionResult {
    // The session was already closed so nothing was done.
    SESSION_ALREADY_CLOSED,
    // The session was moved into the closed state but was not removed
    // from |pool_| (because we're in an IO loop).
    SESSION_CLOSED_BUT_NOT_REMOVED,
    // The session was moved into the closed state and removed from
    // |pool_|.
    SESSION_CLOSED_AND_REMOVED,
  };

  // Checks whether a stream for the given |url| can be created or
  // retrieved from the set of unclaimed push streams. Returns OK if
  // so. Otherwise, the session is closed and an error <
  // ERR_IO_PENDING is returned.
  Error TryAccessStream(const GURL& url);

  // Called by SpdyStreamRequest to start a request to create a
  // stream. If OK is returned, then |stream| will be filled in with a
  // valid stream. If ERR_IO_PENDING is returned, then
  // |request->OnRequestComplete{Success,Failure}()| will be called
  // when the stream is created (unless it is cancelled). Otherwise,
  // no stream is created and the error is returned.
  int TryCreateStream(const base::WeakPtr<SpdyStreamRequest>& request,
                      base::WeakPtr<SpdyStream>* stream);

  // Actually create a stream into |stream|. Returns OK if successful;
  // otherwise, returns an error and |stream| is not filled.
  int CreateStream(const SpdyStreamRequest& request,
                   base::WeakPtr<SpdyStream>* stream);

  // Called by SpdyStreamRequest to remove |request| from the stream
  // creation queue.
  void CancelStreamRequest(const base::WeakPtr<SpdyStreamRequest>& request);

  // Returns the next pending stream request to process, or NULL if
  // there is none.
  base::WeakPtr<SpdyStreamRequest> GetNextPendingStreamRequest();

  // Called when there is room to create more streams (e.g., a stream
  // was closed). Processes as many pending stream requests as
  // possible.
  void ProcessPendingStreamRequests();

  // Close the stream pointed to by the given iterator. Note that that
  // stream may hold the last reference to the session.
  void CloseActiveStreamIterator(ActiveStreamMap::iterator it, int status);

  // Close the stream pointed to by the given iterator. Note that that
  // stream may hold the last reference to the session.
  void CloseCreatedStreamIterator(CreatedStreamSet::iterator it, int status);

  // Calls EnqueueResetStreamFrame() and then
  // CloseActiveStreamIterator().
  void ResetStreamIterator(ActiveStreamMap::iterator it,
                           SpdyRstStreamStatus status,
                           const std::string& description);

  // Send a RST_STREAM frame with the given parameters. There should
  // either be no active stream with the given ID, or that active
  // stream should be closed shortly after this function is called.
  //
  // TODO(akalin): Rename this to EnqueueResetStreamFrame().
  void EnqueueResetStreamFrame(SpdyStreamId stream_id,
                               RequestPriority priority,
                               SpdyRstStreamStatus status,
                               const std::string& description);

  // Calls DoReadLoop and then if |availability_state_| is
  // STATE_CLOSED, calls RemoveFromPool().
  //
  // Use this function instead of DoReadLoop when posting a task to
  // pump the read loop.
  void PumpReadLoop(ReadState expected_read_state, int result);

  // Advance the ReadState state machine. |expected_read_state| is the
  // expected starting read state.
  //
  // This function must always be called via PumpReadLoop() except for
  // from InitializeWithSocket().
  int DoReadLoop(ReadState expected_read_state, int result);
  // The implementations of the states of the ReadState state machine.
  int DoRead();
  int DoReadComplete(int result);

  // Calls DoWriteLoop and then if |availability_state_| is
  // STATE_CLOSED, calls RemoveFromPool().
  //
  // Use this function instead of DoWriteLoop when posting a task to
  // pump the write loop.
  void PumpWriteLoop(WriteState expected_write_state, int result);

  // Advance the WriteState state machine. |expected_write_state| is
  // the expected starting write state.
  //
  // This function must always be called via PumpWriteLoop().
  int DoWriteLoop(WriteState expected_write_state, int result);
  // The implementations of the states of the WriteState state machine.
  int DoWrite();
  int DoWriteComplete(int result);

  // TODO(akalin): Rename the Send* and Write* functions below to
  // Enqueue*.

  // Send initial data. Called when a connection is successfully
  // established in InitializeWithSocket() and
  // |enable_sending_initial_data_| is true.
  void SendInitialData();

  // Helper method to send a SETTINGS frame.
  void SendSettings(const SettingsMap& settings);

  // Handle SETTING.  Either when we send settings, or when we receive a
  // SETTINGS control frame, update our SpdySession accordingly.
  void HandleSetting(uint32 id, uint32 value);

  // Adjust the send window size of all ActiveStreams and PendingStreamRequests.
  void UpdateStreamsSendWindowSize(int32 delta_window_size);

  // Send the PING (preface-PING) frame.
  void SendPrefacePingIfNoneInFlight();

  // Send PING if there are no PINGs in flight and we haven't heard from server.
  void SendPrefacePing();

  // Send a single WINDOW_UPDATE frame.
  void SendWindowUpdateFrame(SpdyStreamId stream_id, uint32 delta_window_size,
                             RequestPriority priority);

  // Send the PING frame.
  void WritePingFrame(uint32 unique_id);

  // Post a CheckPingStatus call after delay. Don't post if there is already
  // CheckPingStatus running.
  void PlanToCheckPingStatus();

  // Check the status of the connection. It calls |CloseSessionOnError| if we
  // haven't received any data in |kHungInterval| time period.
  void CheckPingStatus(base::TimeTicks last_check_time);

  // Get a new stream id.
  int GetNewStreamId();

  // Pushes the given frame with the given priority into the write
  // queue for the session.
  void EnqueueSessionWrite(RequestPriority priority,
                           SpdyFrameType frame_type,
                           scoped_ptr<SpdyFrame> frame);

  // Puts |producer| associated with |stream| onto the write queue
  // with the given priority.
  void EnqueueWrite(RequestPriority priority,
                    SpdyFrameType frame_type,
                    scoped_ptr<SpdyBufferProducer> producer,
                    const base::WeakPtr<SpdyStream>& stream);

  // Inserts a newly-created stream into |created_streams_|.
  void InsertCreatedStream(scoped_ptr<SpdyStream> stream);

  // Activates |stream| (which must be in |created_streams_|) by
  // assigning it an ID and returns it.
  scoped_ptr<SpdyStream> ActivateCreatedStream(SpdyStream* stream);

  // Inserts a newly-activated stream into |active_streams_|.
  void InsertActivatedStream(scoped_ptr<SpdyStream> stream);

  // Remove all internal references to |stream|, call OnClose() on it,
  // and process any pending stream requests before deleting it.  Note
  // that |stream| may hold the last reference to the session.
  void DeleteStream(scoped_ptr<SpdyStream> stream, int status);

  // Check if we have a pending pushed-stream for this url
  // Returns the stream if found (and returns it from the pending
  // list). Returns NULL otherwise.
  base::WeakPtr<SpdyStream> GetActivePushStream(const GURL& url);

  // Delegates to |stream->OnInitialResponseHeadersReceived()|. If an
  // error is returned, the last reference to |this| may have been
  // released.
  int OnInitialResponseHeadersReceived(const SpdyHeaderBlock& response_headers,
                                       base::Time response_time,
                                       base::TimeTicks recv_first_byte_time,
                                       SpdyStream* stream);

  void RecordPingRTTHistogram(base::TimeDelta duration);
  void RecordHistograms();
  void RecordProtocolErrorHistogram(SpdyProtocolErrorDetails details);

  // DCHECKs that |availability_state_| >= STATE_GOING_AWAY, that
  // there are no pending stream creation requests, and that there are
  // no created streams.
  void DcheckGoingAway() const;

  // Calls DcheckGoingAway(), then DCHECKs that |availability_state_|
  // == STATE_CLOSED, |error_on_close_| has a valid value, that there
  // are no active streams or unclaimed pushed streams, and that the
  // write queue is empty.
  void DcheckClosed() const;

  // Closes all active streams with stream id's greater than
  // |last_good_stream_id|, as well as any created or pending
  // streams. Must be called only when |availability_state_| >=
  // STATE_GOING_AWAY. After this function, DcheckGoingAway() will
  // pass. May be called multiple times.
  void StartGoingAway(SpdyStreamId last_good_stream_id, Error status);

  // Must be called only when going away (i.e., DcheckGoingAway()
  // passes). If there are no more active streams and the session
  // isn't closed yet, close it.
  void MaybeFinishGoingAway();

  // If the stream is already closed, does nothing. Otherwise, moves
  // the session to a closed state. Then, if we're in an IO loop,
  // returns (as the IO loop will do the pool removal itself when its
  // done). Otherwise, also removes |this| from |pool_|. The returned
  // result describes what was done.
  CloseSessionResult DoCloseSession(Error err, const std::string& description);

  // Remove this session from its pool, which must exist. Must be
  // called only when the session is closed.
  //
  // Must be called only via Pump{Read,Write}Loop() or
  // DoCloseSession().
  void RemoveFromPool();

  // Called right before closing a (possibly-inactive) stream for a
  // reason other than being requested to by the stream.
  void LogAbandonedStream(SpdyStream* stream, Error status);

  // Called right before closing an active stream for a reason other
  // than being requested to by the stream.
  void LogAbandonedActiveStream(ActiveStreamMap::const_iterator it,
                                Error status);

  // Invokes a user callback for stream creation.  We provide this method so it
  // can be deferred to the MessageLoop, so we avoid re-entrancy problems.
  void CompleteStreamRequest(
      const base::WeakPtr<SpdyStreamRequest>& pending_request);

  // Remove old unclaimed pushed streams.
  void DeleteExpiredPushedStreams();

  // BufferedSpdyFramerVisitorInterface:
  virtual void OnError(SpdyFramer::SpdyError error_code) OVERRIDE;
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) OVERRIDE;
  virtual void OnPing(uint32 unique_id) OVERRIDE;
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE;
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE;
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE;
  virtual void OnSettings(bool clear_persisted) OVERRIDE;
  virtual void OnSetting(
      SpdySettingsIds id, uint8 flags, uint32 value) OVERRIDE;
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE;
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id) OVERRIDE;
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) OVERRIDE;
  virtual void OnSynReply(
      SpdyStreamId stream_id,
      bool fin,
      const SpdyHeaderBlock& headers) OVERRIDE;
  virtual void OnHeaders(
      SpdyStreamId stream_id,
      bool fin,
      const SpdyHeaderBlock& headers) OVERRIDE;

  // SpdyFramerDebugVisitorInterface
  virtual void OnSendCompressedFrame(
      SpdyStreamId stream_id,
      SpdyFrameType type,
      size_t payload_len,
      size_t frame_len) OVERRIDE;
  virtual void OnReceiveCompressedFrame(
      SpdyStreamId stream_id,
      SpdyFrameType type,
      size_t frame_len) OVERRIDE {}

  // Called when bytes are consumed from a SpdyBuffer for a DATA frame
  // that is to be written or is being written. Increases the send
  // window size accordingly if some or all of the SpdyBuffer is being
  // discarded.
  //
  // If session flow control is turned off, this must not be called.
  void OnWriteBufferConsumed(size_t frame_payload_size,
                             size_t consume_size,
                             SpdyBuffer::ConsumeSource consume_source);

  // Called by OnWindowUpdate() (which is in turn called by the
  // framer) to increase this session's send window size by
  // |delta_window_size| from a WINDOW_UPDATE frome, which must be at
  // least 1. If |delta_window_size| would cause this session's send
  // window size to overflow, does nothing.
  //
  // If session flow control is turned off, this must not be called.
  void IncreaseSendWindowSize(int32 delta_window_size);

  // If session flow control is turned on, called by CreateDataFrame()
  // (which is in turn called by a stream) to decrease this session's
  // send window size by |delta_window_size|, which must be at least 1
  // and at most kMaxSpdyFrameChunkSize.  |delta_window_size| must not
  // cause this session's send window size to go negative.
  //
  // If session flow control is turned off, this must not be called.
  void DecreaseSendWindowSize(int32 delta_window_size);

  // Called when bytes are consumed by the delegate from a SpdyBuffer
  // containing received data. Increases the receive window size
  // accordingly.
  //
  // If session flow control is turned off, this must not be called.
  void OnReadBufferConsumed(size_t consume_size,
                            SpdyBuffer::ConsumeSource consume_source);

  // Called by OnReadBufferConsume to increase this session's receive
  // window size by |delta_window_size|, which must be at least 1 and
  // must not cause this session's receive window size to overflow,
  // possibly also sending a WINDOW_UPDATE frame. Also called during
  // initialization to set the initial receive window size.
  //
  // If session flow control is turned off, this must not be called.
  void IncreaseRecvWindowSize(int32 delta_window_size);

  // Called by OnStreamFrameData (which is in turn called by the
  // framer) to decrease this session's receive window size by
  // |delta_window_size|, which must be at least 1 and must not cause
  // this session's receive window size to go negative.
  //
  // If session flow control is turned off, this must not be called.
  void DecreaseRecvWindowSize(int32 delta_window_size);

  // Queue a send-stalled stream for possibly resuming once we're not
  // send-stalled anymore.
  void QueueSendStalledStream(const SpdyStream& stream);

  // Go through the queue of send-stalled streams and try to resume as
  // many as possible.
  void ResumeSendStalledStreams();

  // Returns the next stream to possibly resume, or 0 if the queue is
  // empty.
  SpdyStreamId PopStreamToPossiblyResume();

  // --------------------------
  // Helper methods for testing
  // --------------------------

  void set_connection_at_risk_of_loss_time(base::TimeDelta duration) {
    connection_at_risk_of_loss_time_ = duration;
  }

  void set_hung_interval(base::TimeDelta duration) {
    hung_interval_ = duration;
  }

  int64 pings_in_flight() const { return pings_in_flight_; }

  uint32 next_ping_id() const { return next_ping_id_; }

  base::TimeTicks last_activity_time() const { return last_activity_time_; }

  bool check_ping_status_pending() const { return check_ping_status_pending_; }

  size_t max_concurrent_streams() const { return max_concurrent_streams_; }

  // Returns the SSLClientSocket that this SPDY session sits on top of,
  // or NULL, if the transport is not SSL.
  SSLClientSocket* GetSSLClientSocket() const;

  // Used for posting asynchronous IO tasks.  We use this even though
  // SpdySession is refcounted because we don't need to keep the SpdySession
  // alive if the last reference is within a RunnableMethod.  Just revoke the
  // method.
  base::WeakPtrFactory<SpdySession> weak_factory_;

  // Whether Do{Read,Write}Loop() is in the call stack. Useful for
  // making sure we don't destroy ourselves prematurely in that case.
  bool in_io_loop_;

  // The key used to identify this session.
  const SpdySessionKey spdy_session_key_;

  // Set set of SpdySessionKeys for which this session has serviced
  // requests.
  std::set<SpdySessionKey> pooled_aliases_;

  // |pool_| owns us, therefore its lifetime must exceed ours.  We set
  // this to NULL after we are removed from the pool.
  SpdySessionPool* pool_;
  const base::WeakPtr<HttpServerProperties> http_server_properties_;

  // The socket handle for this session.
  scoped_ptr<ClientSocketHandle> connection_;

  // The read buffer used to read data from the socket.
  scoped_refptr<IOBuffer> read_buffer_;

  int stream_hi_water_mark_;  // The next stream id to use.

  // Queue, for each priority, of pending stream requests that have
  // not yet been satisfied.
  PendingStreamRequestQueue pending_create_stream_queues_[NUM_PRIORITIES];

  // Map from stream id to all active streams.  Streams are active in the sense
  // that they have a consumer (typically SpdyNetworkTransaction and regardless
  // of whether or not there is currently any ongoing IO [might be waiting for
  // the server to start pushing the stream]) or there are still network events
  // incoming even though the consumer has already gone away (cancellation).
  //
  // |active_streams_| owns all its SpdyStream objects.
  //
  // TODO(willchan): Perhaps we should separate out cancelled streams and move
  // them into a separate ActiveStreamMap, and not deliver network events to
  // them?
  ActiveStreamMap active_streams_;

  // (Bijective) map from the URL to the ID of the streams that have
  // already started to be pushed by the server, but do not have
  // consumers yet. Contains a subset of |active_streams_|.
  PushedStreamMap unclaimed_pushed_streams_;

  // Set of all created streams but that have not yet sent any frames.
  //
  // |created_streams_| owns all its SpdyStream objects.
  CreatedStreamSet created_streams_;

  // The write queue.
  SpdyWriteQueue write_queue_;

  // Data for the frame we are currently sending.

  // The buffer we're currently writing.
  scoped_ptr<SpdyBuffer> in_flight_write_;
  // The type of the frame in |in_flight_write_|.
  SpdyFrameType in_flight_write_frame_type_;
  // The size of the frame in |in_flight_write_|.
  size_t in_flight_write_frame_size_;
  // The stream to notify when |in_flight_write_| has been written to
  // the socket completely.
  base::WeakPtr<SpdyStream> in_flight_write_stream_;

  // Flag if we're using an SSL connection for this SpdySession.
  bool is_secure_;

  // Certificate error code when using a secure connection.
  int certificate_error_code_;

  // Spdy Frame state.
  scoped_ptr<BufferedSpdyFramer> buffered_spdy_framer_;

  // The state variables.
  AvailabilityState availability_state_;
  ReadState read_state_;
  WriteState write_state_;

  // If the session was closed (i.e., |availability_state_| is
  // STATE_CLOSED), then |error_on_close_| holds the error with which
  // it was closed, which is < ERR_IO_PENDING. Otherwise, it is set to
  // OK.
  Error error_on_close_;

  // Limits
  size_t max_concurrent_streams_;  // 0 if no limit
  size_t max_concurrent_streams_limit_;

  // Some statistics counters for the session.
  int streams_initiated_count_;
  int streams_pushed_count_;
  int streams_pushed_and_claimed_count_;
  int streams_abandoned_count_;

  // |total_bytes_received_| keeps track of all the bytes read by the
  // SpdySession. It is used by the |Net.SpdySettingsCwnd...| histograms.
  int total_bytes_received_;

  bool sent_settings_;      // Did this session send settings when it started.
  bool received_settings_;  // Did this session receive at least one settings
                            // frame.
  int stalled_streams_;     // Count of streams that were ever stalled.

  // Count of all pings on the wire, for which we have not gotten a response.
  int64 pings_in_flight_;

  // This is the next ping_id (unique_id) to be sent in PING frame.
  uint32 next_ping_id_;

  // This is the last time we have sent a PING.
  base::TimeTicks last_ping_sent_time_;

  // This is the last time we had activity in the session.
  base::TimeTicks last_activity_time_;

  // This is the next time that unclaimed push streams should be checked for
  // expirations.
  base::TimeTicks next_unclaimed_push_stream_sweep_time_;

  // Indicate if we have already scheduled a delayed task to check the ping
  // status.
  bool check_ping_status_pending_;

  // Whether to send the (HTTP/2) connection header prefix.
  bool send_connection_header_prefix_;

  // The (version-dependent) flow control state.
  FlowControlState flow_control_state_;

  // Initial send window size for this session's streams. Can be
  // changed by an arriving SETTINGS frame. Newly created streams use
  // this value for the initial send window size.
  int32 stream_initial_send_window_size_;

  // Initial receive window size for this session's streams. There are
  // plans to add a command line switch that would cause a SETTINGS
  // frame with window size announcement to be sent on startup. Newly
  // created streams will use this value for the initial receive
  // window size.
  int32 stream_initial_recv_window_size_;

  // Session flow control variables. All zero unless session flow
  // control is turned on.
  int32 session_send_window_size_;
  int32 session_recv_window_size_;
  int32 session_unacked_recv_window_bytes_;

  // A queue of stream IDs that have been send-stalled at some point
  // in the past.
  std::deque<SpdyStreamId> stream_send_unstall_queue_[NUM_PRIORITIES];

  BoundNetLog net_log_;

  // Outside of tests, these should always be true.
  bool verify_domain_authentication_;
  bool enable_sending_initial_data_;
  bool enable_credential_frames_;
  bool enable_compression_;
  bool enable_ping_based_connection_checking_;

  // The SPDY protocol used. Always between kProtoSPDY2 and
  // kProtoSPDYMaximumVersion.
  //
  // TODO(akalin): Change the lower bound to kProtoSPDYMinimumVersion
  // once we stop supporting SPDY/1.
  NextProto protocol_;

  SpdyCredentialState credential_state_;

  // |connection_at_risk_of_loss_time_| is an optimization to avoid sending
  // wasteful preface pings (when we just got some data).
  //
  // If it is zero (the most conservative figure), then we always send the
  // preface ping (when none are in flight).
  //
  // It is common for TCP/IP sessions to time out in about 3-5 minutes.
  // Certainly if it has been more than 3 minutes, we do want to send a preface
  // ping.
  //
  // We don't think any connection will time out in under about 10 seconds. So
  // this might as well be set to something conservative like 10 seconds. Later,
  // we could adjust it to send fewer pings perhaps.
  base::TimeDelta connection_at_risk_of_loss_time_;

  // The amount of time that we are willing to tolerate with no activity (of any
  // form), while there is a ping in flight, before we declare the connection to
  // be hung. TODO(rtenneti): When hung, instead of resetting connection, race
  // to build a new connection, and see if that completes before we (finally)
  // get a PING response (http://crbug.com/127812).
  base::TimeDelta hung_interval_;

  // This SPDY proxy is allowed to push resources from origins that are
  // different from those of their associated streams.
  HostPortPair trusted_spdy_proxy_;

  TimeFunc time_func_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_H_
