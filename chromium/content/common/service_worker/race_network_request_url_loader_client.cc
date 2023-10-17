// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {
namespace {
MojoResult CreateDataPipe(mojo::ScopedDataPipeProducerHandle& producer_handle,
                          mojo::ScopedDataPipeConsumerHandle& consumer_handle,
                          uint32_t capacity_num_bytes) {
  MojoCreateDataPipeOptions options;

  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = capacity_num_bytes;

  return mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
}
}  // namespace

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ServiceWorkerRaceNetworkRequestURLLoaderClient(
        const network::ResourceRequest& request,
        base::WeakPtr<ServiceWorkerResourceLoader> owner,
        absl::optional<mojo::PendingRemote<network::mojom::URLLoaderClient>>
            forwarding_client,
        uint32_t data_pipe_capacity_num_bytes)
    : request_(request),
      owner_(std::move(owner)),
      forwarding_client_(std::move(forwarding_client)),
      body_consumer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             base::SequencedTaskRunner::GetCurrentDefault()) {
  // Create two data pipes. One is for RaceNetworkRequest. The other is for the
  // corresponding request in the fetch handler.
  if (CreateDataPipe(data_pipe_for_race_network_request_.producer,
                     data_pipe_for_race_network_request_.consumer,
                     data_pipe_capacity_num_bytes) != MOJO_RESULT_OK) {
    TransitionState(State::kAborted);
    return;
  }
  if (forwarding_client_) {
    if (CreateDataPipe(data_pipe_for_fetch_handler_.producer,
                       data_pipe_for_fetch_handler_.consumer,
                       data_pipe_capacity_num_bytes) != MOJO_RESULT_OK) {
      TransitionState(State::kAborted);
      return;
    }
  }
}

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ~ServiceWorkerRaceNetworkRequestURLLoaderClient() = default;

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kServiceWorkerRaceNetworkRequest);
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // Do nothing. Early Hints response will be handled by owner's
  // |url_loader_client_|.
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT0(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveResponse");
  if (!owner_) {
    return;
  }

  head_ = std::move(head);
  cached_metadata_ = std::move(cached_metadata);
  body_ = std::move(body);
  body_consumer_watcher_.Watch(
      body_.get(), MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite,
          base::Unretained(this)));
  body_consumer_watcher_.ArmOrNotify();
  data_pipe_for_race_network_request_.watcher.Watch(
      data_pipe_for_race_network_request_.producer.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite,
          base::Unretained(this)));
  if (forwarding_client_) {
    data_pipe_for_fetch_handler_.watcher.Watch(
        data_pipe_for_fetch_handler_.producer.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(
            &ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite,
            base::Unretained(this)));
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (!owner_) {
    return;
  }
  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
      owner_->SetCommitResponsibility(FetchResponseFrom::kWithoutServiceWorker);
      owner_->HandleRedirect(redirect_info, head);
      break;
    case FetchResponseFrom::kServiceWorker:
      // If commit_responsibility() is FetchResponseFrom::kServiceWorker, that
      // means the response was already received from the fetch handler. The
      // response from RaceNetworkRequest is simply discarded in that case.
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      owner_->HandleRedirect(redirect_info, head);
      break;
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (!owner_) {
    return;
  }
  completion_status_ = status;
  MaybeCompleteResponse();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Bind(
    mojo::PendingRemote<network::mojom::URLLoaderClient>* remote) {
  receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::CommitResponse() {
  owner_->CommitResponseHeaders(head_);
  owner_->CommitResponseBody(
      head_, std::move(data_pipe_for_race_network_request_.consumer),
      std::move(cached_metadata_));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::MaybeCommitResponse() {
  if (state_ != State::kWaitForBody) {
    return;
  }
  TransitionState(State::kResponseCommitted);
  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
      // If the fetch handler result is a fallback, commit the
      // RaceNetworkRequest response. If the result is not a fallback and the
      // response is not 200, use the other response from the fetch handler
      // instead because it may have a response from the cache.
      // TODO(crbug.com/1420517): More comprehensive error handling may be
      // needed, especially the case when HTTP cache hit or redirect happened.
      if (head_->headers->response_code() != net::HttpStatusCode::HTTP_OK) {
        owner_->SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
      } else {
        owner_->SetCommitResponsibility(
            FetchResponseFrom::kWithoutServiceWorker);
        CommitResponse();
      }
      break;
    case FetchResponseFrom::kServiceWorker:
      // If commit responsibility is FetchResponseFrom::kServiceWorker, that
      // means the response was already received from the fetch handler. The
      // response from RaceNetworkRequest is simply discarded in that case.
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      // kWithoutServiceWorker is set When the fetch handler response comes
      // first and the result is a fallback. Commit the RaceNetworkRequest
      // response.
      CommitResponse();
      break;
  }

  if (forwarding_client_) {
    forwarding_client_.value()->OnReceiveResponse(
        head_->Clone(), std::move(data_pipe_for_fetch_handler_.consumer),
        absl::nullopt);
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::MaybeCompleteResponse() {
  if (!completion_status_) {
    return;
  }

  // If the data transfer finished, or a network error happened, complete the
  // commit.
  if (state_ == State::kDataTransferFinished ||
      completion_status_->error_code != net::OK) {
    CompleteResponse();
    return;
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::CompleteResponse() {
  TransitionState(State::kCompleted);
  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
      // If a network error happens, there is a case that OnComplete can be
      // directly called, in that case |owner_->commit_responsibility()| is not
      // set yet. Ask the fetch handler side to handle response.
      owner_->SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
      break;
    case FetchResponseFrom::kServiceWorker:
      // If the fetch handler wins or there is a network error in
      // RaceNetworkRequest, do nothing. Defer the handling to the owner.
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      owner_->CommitCompleted(completion_status_->error_code,
                              "RaceNetworkRequest has completed.");
      break;
  }
  data_pipe_for_race_network_request_.producer.reset();
  if (forwarding_client_) {
    forwarding_client_.value()->OnComplete(completion_status_.value());
    data_pipe_for_fetch_handler_.producer.reset();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnDataTransferComplete() {
  MaybeCommitResponse();
  TRACE_EVENT0(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnDataTransferComplete");
  TransitionState(State::kDataTransferFinished);
  MaybeCompleteResponse();
  body_consumer_watcher_.Cancel();
  data_pipe_for_race_network_request_.watcher.Cancel();
  if (forwarding_client_) {
    data_pipe_for_fetch_handler_.watcher.Cancel();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite(
    MojoResult aresult) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite");
  // Read data from |body_| data pipe.
  const void* buffer;
  // Contains the actual byte size for read/write data. The smallest number from
  // 1) read size, 2) write size for the RaceNetworkRequest, 3) write size for
  // the fetch handler, will be used.
  uint32_t num_bytes_to_consume = 0;

  MojoResult result = body_->BeginReadData(&buffer, &num_bytes_to_consume,
                                           MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Successfully read the whole data.
      OnDataTransferComplete();
      return;
    case MOJO_RESULT_BUSY:
      return;
    default:
      NOTREACHED();
      return;
  }

  void* write_buffer = nullptr;
  void* write_buffer_for_fetch_handler = nullptr;

  // Begin the write process for the response of the race network request.
  result = data_pipe_for_race_network_request_.producer->BeginWriteData(
      &write_buffer, &data_pipe_for_race_network_request_.num_write_bytes,
      MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      // Perhaps writable size may be smaller than the readable size. Choose the
      // most smallest size.
      num_bytes_to_consume =
          std::min(num_bytes_to_consume,
                   data_pipe_for_race_network_request_.num_write_bytes);
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data pipe consumer is aborted.
      TransitionState(State::kAborted);
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // The data pipe is not writable yet. We don't consume data from |body_|
      // and write any data in this case. And retry it later.
      body_->EndReadData(0);
      data_pipe_for_race_network_request_.producer->EndWriteData(0);
      data_pipe_for_race_network_request_.watcher.ArmOrNotify();
      return;
  }

  if (forwarding_client_) {
    // Begin the write process for the response of the fetch handler.
    result = data_pipe_for_fetch_handler_.producer->BeginWriteData(
        &write_buffer_for_fetch_handler,
        &data_pipe_for_fetch_handler_.num_write_bytes,
        MOJO_WRITE_DATA_FLAG_NONE);
    switch (result) {
      case MOJO_RESULT_OK:
        // Perhaps writable size may be smaller than the readable size. Choose
        // the most smallest size.
        num_bytes_to_consume = std::min(
            num_bytes_to_consume, data_pipe_for_fetch_handler_.num_write_bytes);
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // The data pipe consumer is aborted.
        TransitionState(State::kAborted);
        Abort();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        // The data pipe is not writable yet. We don't consume data from |body_|
        // and write any data in this case. And retry it later.
        body_->EndReadData(0);
        data_pipe_for_race_network_request_.producer->EndWriteData(0);
        data_pipe_for_fetch_handler_.producer->EndWriteData(0);
        data_pipe_for_fetch_handler_.watcher.ArmOrNotify();
        return;
    }
  }

  // Copy data and complete read/write process.
  memcpy(write_buffer, buffer, num_bytes_to_consume);
  result = data_pipe_for_race_network_request_.producer->EndWriteData(
      num_bytes_to_consume);
  CHECK_EQ(result, MOJO_RESULT_OK);
  if (forwarding_client_) {
    memcpy(write_buffer_for_fetch_handler, buffer, num_bytes_to_consume);
    result = data_pipe_for_fetch_handler_.producer->EndWriteData(
        num_bytes_to_consume);
    CHECK_EQ(result, MOJO_RESULT_OK);
  }
  result = body_->EndReadData(num_bytes_to_consume);
  CHECK_EQ(result, MOJO_RESULT_OK);

  // Once data is written to the data pipe, start the commit process.
  MaybeCommitResponse();
  body_consumer_watcher_.ArmOrNotify();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Abort() {
  data_pipe_for_race_network_request_.producer.reset();
  data_pipe_for_race_network_request_.consumer.reset();
  data_pipe_for_race_network_request_.watcher.Cancel();
  if (forwarding_client_) {
    data_pipe_for_fetch_handler_.producer.reset();
    data_pipe_for_fetch_handler_.consumer.reset();
    data_pipe_for_fetch_handler_.watcher.Cancel();
  }
  body_consumer_watcher_.Cancel();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::TransitionState(
    State new_state) {
  switch (new_state) {
    case State::kWaitForBody:
      NOTREACHED_NORETURN();
    case State::kResponseCommitted:
      CHECK_EQ(state_, State::kWaitForBody);
      state_ = new_state;
      break;
    case State::kDataTransferFinished:
      CHECK(state_ == State::kResponseCommitted);
      state_ = new_state;
      break;
    case State::kCompleted:
      CHECK(state_ == State::kWaitForBody ||
            state_ == State::kResponseCommitted ||
            state_ == State::kDataTransferFinished);
      state_ = new_state;
      break;
    case State::kAborted:
      state_ = new_state;
      break;
  }
}

ServiceWorkerRaceNetworkRequestURLLoaderClient::DataPipeInfo::DataPipeInfo()
    : watcher(FROM_HERE,
              mojo::SimpleWatcher::ArmingPolicy::MANUAL,
              base::SequencedTaskRunner::GetCurrentDefault()),
      num_write_bytes(0) {}
ServiceWorkerRaceNetworkRequestURLLoaderClient::DataPipeInfo::~DataPipeInfo() =
    default;

net::NetworkTrafficAnnotationTag
ServiceWorkerRaceNetworkRequestURLLoaderClient::NetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation(
      "service_worker_race_network_request",
      R"(
    semantics {
      sender: "ServiceWorkerRaceNetworkRequest"
      description:
        "This request is issued by a navigation to fetch the content of the "
        "page that is being navigated to, or by a renderer to fetch "
        "subresources in the case where a service worker has been registered "
        "for the page and the ServiceWorkerBypassFetchHandler feature and the "
        "RaceNetworkRequest param are enabled."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc) and subsequent resource loading."
      data:
        "Arbitrary site-controlled data can be included in the URL, HTTP "
        "headers, and request body. Requests may include cookies and "
        "site-specific credentials."
      destination: WEBSITE
      internal {
        contacts {
          email: "chrome-worker@google.com"
        }
      }
      user_data {
        type: ARBITRARY_DATA
      }
      last_reviewed: "2023-03-22"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This request can be prevented by disabling service workers, which can "
        "be done by disabling cookie and site data under Settings, Content "
        "Settings, Cookies."
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "Chrome would be unable to use service workers if this feature were "
      "disabled, which could result in a degraded experience for websites that "
      "register a service worker. Using either URLBlocklist or URLAllowlist "
      "policies (or a combination of both) limits the scope of these requests."
)");
}
}  // namespace content
