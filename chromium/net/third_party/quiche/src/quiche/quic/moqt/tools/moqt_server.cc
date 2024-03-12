// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/tools/moqt_server.h"

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/proof_source.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_session.h"
#include "quiche/quic/tools/quic_server.h"
#include "quiche/quic/tools/web_transport_only_backend.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

namespace {
quic::WebTransportRequestCallback CreateWebTransportCallback(
    MoqtIncomingSessionCallback callback) {
  return [callback = std::move(callback)](absl::string_view path,
                                          webtransport::Session* session)
             -> absl::StatusOr<std::unique_ptr<webtransport::SessionVisitor>> {
    absl::StatusOr<MoqtSessionCallbacks> callbacks = callback(path);
    if (!callbacks.ok()) {
      return callbacks.status();
    }
    MoqtSessionParameters parameters;
    parameters.perspective = quic::Perspective::IS_SERVER;
    parameters.path = path;
    parameters.using_webtrans = true;
    parameters.version = MoqtVersion::kDraft01;
    parameters.deliver_partial_objects = false;
    return std::make_unique<MoqtSession>(session, parameters,
                                         *std::move(callbacks));
  };
}
}  // namespace

MoqtServer::MoqtServer(std::unique_ptr<quic::ProofSource> proof_source,
                       MoqtIncomingSessionCallback callback)
    : backend_(CreateWebTransportCallback(std::move(callback))),
      server_(std::move(proof_source), &backend_) {}

}  // namespace moqt
