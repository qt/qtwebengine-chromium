// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_url_fetcher.h"

#include "content/child/child_thread.h"
#include "content/child/npapi/webplugin.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_stream_url.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/child/plugin_messages.h"
#include "content/child/resource_dispatcher.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "webkit/child/multipart_response_delegate.h"
#include "webkit/child/weburlloader_impl.h"
#include "webkit/common/resource_request_body.h"

namespace content {
namespace {

// This class handles individual multipart responses. It is instantiated when
// we receive HTTP status code 206 in the HTTP response. This indicates
// that the response could have multiple parts each separated by a boundary
// specified in the response header.
// TODO(jam): this is similar to MultiPartResponseClient in webplugin_impl.cc,
// we should remove that other class once we switch to loading from the plugin
// process by default.
class MultiPartResponseClient : public WebKit::WebURLLoaderClient {
 public:
  explicit MultiPartResponseClient(PluginStreamUrl* plugin_stream)
      : byte_range_lower_bound_(0), plugin_stream_(plugin_stream) {}

  // WebKit::WebURLLoaderClient implementation:
  virtual void didReceiveResponse(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLResponse& response) OVERRIDE {
    int64 byte_range_upper_bound, instance_size;
    if (!webkit_glue::MultipartResponseDelegate::ReadContentRanges(
            response, &byte_range_lower_bound_, &byte_range_upper_bound,
            &instance_size)) {
      NOTREACHED();
    }
  }
  virtual void didReceiveData(WebKit::WebURLLoader* loader,
                              const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE {
    // TODO(ananta)
    // We should defer further loads on multipart resources on the same lines
    // as regular resources requested by plugins to prevent reentrancy.
    plugin_stream_->DidReceiveData(data, data_length, byte_range_lower_bound_);
    byte_range_lower_bound_ += data_length;
  }

 private:
  // The lower bound of the byte range.
  int64 byte_range_lower_bound_;
  // The handler for the data.
  PluginStreamUrl* plugin_stream_;
};

}  // namespace

PluginURLFetcher::PluginURLFetcher(PluginStreamUrl* plugin_stream,
                                   const GURL& url,
                                   const GURL& first_party_for_cookies,
                                   const std::string& method,
                                   const char* buf,
                                   unsigned int len,
                                   const GURL& referrer,
                                   bool notify_redirects,
                                   bool is_plugin_src_load,
                                   int origin_pid,
                                   int render_view_id,
                                   unsigned long resource_id)
    : plugin_stream_(plugin_stream),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies),
      method_(method),
      referrer_(referrer),
      notify_redirects_(notify_redirects),
      is_plugin_src_load_(is_plugin_src_load),
      resource_id_(resource_id),
      data_offset_(0) {
  webkit_glue::ResourceLoaderBridge::RequestInfo request_info;
  request_info.method = method;
  request_info.url = url;
  request_info.first_party_for_cookies = first_party_for_cookies;
  request_info.referrer = referrer;
  request_info.load_flags = net::LOAD_NORMAL;
  request_info.requestor_pid = origin_pid;
  request_info.request_type = ResourceType::OBJECT;
  request_info.routing_id = render_view_id;

  std::vector<char> body;
  if (method == "POST") {
    bool content_type_found = false;
    std::vector<std::string> names;
    std::vector<std::string> values;
    PluginHost::SetPostData(buf, len, &names, &values, &body);
    for (size_t i = 0; i < names.size(); ++i) {
      if (!request_info.headers.empty())
        request_info.headers += "\r\n";
      request_info.headers += names[i] + ": " + values[i];
      if (LowerCaseEqualsASCII(names[i], "content-type"))
        content_type_found = true;
    }

    if (!content_type_found) {
      if (!request_info.headers.empty())
        request_info.headers += "\r\n";
      request_info.headers += "Content-Type: application/x-www-form-urlencoded";
    }
  }

  bridge_.reset(ChildThread::current()->resource_dispatcher()->CreateBridge(
      request_info));
  if (!body.empty()) {
    scoped_refptr<webkit_glue::ResourceRequestBody> request_body =
        new webkit_glue::ResourceRequestBody;
    request_body->AppendBytes(&body[0], body.size());
    bridge_->SetRequestBody(request_body.get());
  }

  bridge_->Start(this);

  // TODO(jam): range requests
}

PluginURLFetcher::~PluginURLFetcher() {
}

void PluginURLFetcher::Cancel() {
  bridge_->Cancel();
}

void PluginURLFetcher::URLRedirectResponse(bool allow) {
  if (allow) {
    bridge_->SetDefersLoading(true);
  } else {
    bridge_->Cancel();
    plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
  }
}

void PluginURLFetcher::OnUploadProgress(uint64 position, uint64 size) {
}

bool PluginURLFetcher::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceResponseInfo& info,
    bool* has_new_first_party_for_cookies,
    GURL* new_first_party_for_cookies) {
  // TODO(jam): THIS LOGIC IS COPIED FROM WebPluginImpl::willSendRequest until
  // kDirectNPAPIRequests is the default and we can remove the old path there.

  // Currently this check is just to catch an https -> http redirect when
  // loading the main plugin src URL. Longer term, we could investigate
  // firing mixed diplay or scripting issues for subresource loads
  // initiated by plug-ins.
  if (is_plugin_src_load_ &&
      !plugin_stream_->instance()->webplugin()->CheckIfRunInsecureContent(
          new_url)) {
    plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
    return false;
  }

  // It's unfortunate that this logic of when a redirect's method changes is
  // in url_request.cc, but weburlloader_impl.cc and this file have to duplicate
  // it instead of passing that information.
  int response_code = info.headers->response_code();
  if (response_code != 307)
    method_ = "GET";
  GURL old_url = url_;
  url_ = new_url;
  *has_new_first_party_for_cookies = true;
  *new_first_party_for_cookies = first_party_for_cookies_;

  // If the plugin does not participate in url redirect notifications then just
  // block cross origin 307 POST redirects.
  if (!notify_redirects_) {
    if (response_code == 307 && method_ == "POST" &&
        old_url.GetOrigin() != new_url.GetOrigin()) {
      plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
      return false;
    }
  } else {
    // Pause the request while we ask the plugin what to do about the redirect.
    bridge_->SetDefersLoading(true);
    plugin_stream_->WillSendRequest(url_, response_code);
  }

  return true;
}

void PluginURLFetcher::OnReceivedResponse(
    const webkit_glue::ResourceResponseInfo& info) {
  // TODO(jam): THIS LOGIC IS COPIED FROM WebPluginImpl::didReceiveResponse
  // GetAllHeaders, and GetResponseInfo until kDirectNPAPIRequests is the
  // default and we can remove the old path there.

  bool request_is_seekable = true;
  DCHECK(!multipart_delegate_.get());
  if (plugin_stream_->seekable()) {
    int response_code = info.headers->response_code();
    if (response_code == 206) {
      WebKit::WebURLResponse response;
      response.initialize();
      webkit_glue::WebURLLoaderImpl::PopulateURLResponse(url_, info, &response);

      std::string multipart_boundary;
      if (webkit_glue::MultipartResponseDelegate::ReadMultipartBoundary(
              response, &multipart_boundary)) {
        plugin_stream_->instance()->webplugin()->DidStartLoading();

        MultiPartResponseClient* multi_part_response_client =
            new MultiPartResponseClient(plugin_stream_);

        multipart_delegate_.reset(new webkit_glue::MultipartResponseDelegate(
            multi_part_response_client, NULL, response, multipart_boundary));

        // Multiple ranges requested, data will be delivered by
        // MultipartResponseDelegate.
        data_offset_ = 0;
        return;
      }

      int64 upper_bound = 0, instance_size = 0;
      // Single range requested - go through original processing for
      // non-multipart requests, but update data offset.
      webkit_glue::MultipartResponseDelegate::ReadContentRanges(
          response, &data_offset_, &upper_bound, &instance_size);
    } else if (response_code == 200) {
      // TODO: should we handle this case? We used to but it's not clear that we
      // still need to. This was bug 5403, fixed in r7139.
    }
  }

  // If the length comes in as -1, then it indicates that it was not
  // read off the HTTP headers. We replicate Safari webkit behavior here,
  // which is to set it to 0.
  int expected_length = std::max(static_cast<int>(info.content_length), 0);

  base::Time temp;
  uint32 last_modified = 0;
  std::string headers;
  if (info.headers) {  // NULL for data: urls.
    if (info.headers->GetLastModifiedValue(&temp))
      last_modified = static_cast<uint32>(temp.ToDoubleT());

     // TODO(darin): Shouldn't we also report HTTP version numbers?
    headers = base::StringPrintf("HTTP %d ", info.headers->response_code());
    headers += info.headers->GetStatusText();
    headers += "\n";

    void* iter = NULL;
    std::string name, value;
    while (info.headers->EnumerateHeaderLines(&iter, &name, &value)) {
      // TODO(darin): Should we really exclude headers with an empty value?
      if (!name.empty() && !value.empty())
        headers += name + ": " + value + "\n";
    }
  }

  plugin_stream_->DidReceiveResponse(info.mime_type,
                                     headers,
                                     expected_length,
                                     last_modified,
                                     request_is_seekable);
}

void PluginURLFetcher::OnDownloadedData(int len,
                                        int encoded_data_length) {
}

void PluginURLFetcher::OnReceivedData(const char* data,
                                      int data_length,
                                      int encoded_data_length) {
  if (multipart_delegate_) {
    multipart_delegate_->OnReceivedData(data, data_length, encoded_data_length);
  } else {
    plugin_stream_->DidReceiveData(data, data_length, data_offset_);
    data_offset_ += data_length;
  }
}

void PluginURLFetcher::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    const std::string& security_info,
    const base::TimeTicks& completion_time) {
  if (multipart_delegate_) {
    multipart_delegate_->OnCompletedRequest();
    multipart_delegate_.reset();
  }

  if (error_code == net::OK) {
    plugin_stream_->DidFinishLoading(resource_id_);
  } else {
    plugin_stream_->DidFail(resource_id_);
  }
}

}  // namespace content
