// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_observer.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_view_host.h"

namespace content {

WebContentsObserver::WebContentsObserver(WebContents* web_contents)
    : web_contents_(NULL) {
  Observe(web_contents);
}

WebContentsObserver::WebContentsObserver()
    : web_contents_(NULL) {
}

WebContentsObserver::~WebContentsObserver() {
  if (web_contents_)
    web_contents_->RemoveObserver(this);
}

WebContents* WebContentsObserver::web_contents() const {
  return web_contents_;
}

void WebContentsObserver::Observe(WebContents* web_contents) {
  if (web_contents == web_contents_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (web_contents_)
    web_contents_->RemoveObserver(this);
  web_contents_ = static_cast<WebContentsImpl*>(web_contents);
  if (web_contents_) {
    web_contents_->AddObserver(this);
  }
}

bool WebContentsObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool WebContentsObserver::Send(IPC::Message* message) {
  if (!web_contents_) {
    delete message;
    return false;
  }

  return web_contents_->Send(message);
}

int WebContentsObserver::routing_id() const {
  if (!web_contents_)
    return MSG_ROUTING_NONE;

  return web_contents_->GetRoutingID();
}

void WebContentsObserver::WebContentsImplDestroyed() {
  // Do cleanup so that 'this' can safely be deleted from WebContentsDestroyed.
  web_contents_->RemoveObserver(this);
  WebContentsImpl* contents = web_contents_;
  web_contents_ = NULL;
  WebContentsDestroyed(contents);
}

}  // namespace content
