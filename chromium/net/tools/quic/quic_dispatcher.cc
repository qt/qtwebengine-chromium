// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_dispatcher.h"

#include <errno.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_utils.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_socket_utils.h"

namespace net {
namespace tools {

using std::make_pair;

class DeleteSessionsAlarm : public EpollAlarm {
 public:
  explicit DeleteSessionsAlarm(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {
  }

  virtual int64 OnAlarm() OVERRIDE {
    EpollAlarm::OnAlarm();
    dispatcher_->DeleteSessions();
    return 0;
  }

 private:
  QuicDispatcher* dispatcher_;
};

QuicDispatcher::QuicDispatcher(const QuicConfig& config,
                               const QuicCryptoServerConfig& crypto_config,
                               int fd,
                               EpollServer* epoll_server)
    : config_(config),
      crypto_config_(crypto_config),
      time_wait_list_manager_(
          new QuicTimeWaitListManager(this, epoll_server)),
      delete_sessions_alarm_(new DeleteSessionsAlarm(this)),
      epoll_server_(epoll_server),
      fd_(fd),
      write_blocked_(false) {
}

QuicDispatcher::~QuicDispatcher() {
  STLDeleteValues(&session_map_);
  STLDeleteElements(&closed_session_list_);
}

int QuicDispatcher::WritePacket(const char* buffer, size_t buf_len,
                                const IPAddressNumber& self_address,
                                const IPEndPoint& peer_address,
                                QuicBlockedWriterInterface* writer,
                                int* error) {
  if (write_blocked_) {
    write_blocked_list_.insert(make_pair(writer, true));
    *error = EAGAIN;
    return -1;
  }

  int rc = QuicSocketUtils::WritePacket(fd_, buffer, buf_len,
                                        self_address, peer_address,
                                        error);
  if (rc == -1 && (*error == EWOULDBLOCK || *error == EAGAIN)) {
    write_blocked_list_.insert(make_pair(writer, true));
    write_blocked_ = true;
  }
  return rc;
}

void QuicDispatcher::ProcessPacket(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   QuicGuid guid,
                                   const QuicEncryptedPacket& packet) {
  QuicSession* session;

  SessionMap::iterator it = session_map_.find(guid);
  if (it == session_map_.end()) {
    if (time_wait_list_manager_->IsGuidInTimeWait(guid)) {
      time_wait_list_manager_->ProcessPacket(server_address,
                                             client_address,
                                             guid,
                                             packet);
      return;
    }
    session = CreateQuicSession(guid, client_address, fd_, epoll_server_);

    if (session == NULL) {
      DLOG(INFO) << "Failed to create session for " << guid;
      // Add this guid fo the time-wait state, to safely nack future packets.
      // We don't know the version here, so assume latest.
      time_wait_list_manager_->AddGuidToTimeWait(guid, QuicVersionMax());
      time_wait_list_manager_->ProcessPacket(server_address,
                                             client_address,
                                             guid,
                                             packet);
      return;
    }
    DLOG(INFO) << "Created new session for " << guid;
    session_map_.insert(make_pair(guid, session));
  } else {
    session = it->second;
  }

  session->connection()->ProcessUdpPacket(
      server_address, client_address, packet);
}

void QuicDispatcher::CleanUpSession(SessionMap::iterator it) {
  QuicSession* session = it->second;
  write_blocked_list_.erase(session->connection());
  time_wait_list_manager_->AddGuidToTimeWait(it->first,
                                             session->connection()->version());
  session_map_.erase(it);
}

void QuicDispatcher::DeleteSessions() {
  STLDeleteElements(&closed_session_list_);
}

bool QuicDispatcher::OnCanWrite() {
  // We got an EPOLLOUT: the socket should not be blocked.
  write_blocked_ = false;

  // Give each writer one attempt to write.
  int num_writers = write_blocked_list_.size();
  for (int i = 0; i < num_writers; ++i) {
    if (write_blocked_list_.empty()) {
      break;
    }
    QuicBlockedWriterInterface* writer = write_blocked_list_.begin()->first;
    write_blocked_list_.erase(write_blocked_list_.begin());
    bool can_write_more = writer->OnCanWrite();
    if (write_blocked_) {
      // We were unable to write.  Wait for the next EPOLLOUT.
      // In this case, the session would have been added to the blocked list
      // up in WritePacket.
      return false;
    }
    // The socket is not blocked but the writer has ceded work.  Add it to the
    // end of the list.
    if (can_write_more) {
      write_blocked_list_.insert(make_pair(writer, true));
    }
  }

  // We're not write blocked.  Return true if there's more work to do.
  return !write_blocked_list_.empty();
}

void QuicDispatcher::Shutdown() {
  while (!session_map_.empty()) {
    QuicSession* session = session_map_.begin()->second;
    session->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
    // Validate that the session removes itself from the session map on close.
    DCHECK(session_map_.empty() || session_map_.begin()->second != session);
  }
  DeleteSessions();
}

void QuicDispatcher::OnConnectionClose(QuicGuid guid, QuicErrorCode error) {
  SessionMap::iterator it = session_map_.find(guid);
  if (it == session_map_.end()) {
    LOG(DFATAL) << "GUID " << guid << " does not exist in the session map.  "
                << "Error: " << QuicUtils::ErrorToString(error);
    return;
  }

  DLOG_IF(INFO, error != QUIC_NO_ERROR) << "Closing connection due to error: "
                                        << QuicUtils::ErrorToString(error);

  if (closed_session_list_.empty()) {
    epoll_server_->RegisterAlarmApproximateDelta(
        0, delete_sessions_alarm_.get());
  }
  closed_session_list_.push_back(it->second);
  CleanUpSession(it);
}

QuicSession* QuicDispatcher::CreateQuicSession(
    QuicGuid guid,
    const IPEndPoint& client_address,
    int fd,
    EpollServer* epoll_server) {
  QuicConnectionHelperInterface* helper =
      new QuicEpollConnectionHelper(this, epoll_server);
  QuicServerSession* session = new QuicServerSession(
       config_, new QuicConnection(guid, client_address, helper, true,
                                   QuicVersionMax()), this);
  session->InitializeSession(crypto_config_);
  return session;
}

}  // namespace tools
}  // namespace net


