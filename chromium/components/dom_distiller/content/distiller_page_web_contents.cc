// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/distiller_page_web_contents.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace dom_distiller {

scoped_ptr<DistillerPage> DistillerPageWebContentsFactory::CreateDistillerPage(
    DistillerPage::Delegate* delegate) const {
  DCHECK(browser_context_);
  return scoped_ptr<DistillerPage>(
      new DistillerPageWebContents(delegate, browser_context_));
}

DistillerPageWebContents::DistillerPageWebContents(
    DistillerPage::Delegate* delegate,
    content::BrowserContext* browser_context)
  : DistillerPage(delegate),
    browser_context_(browser_context) {}

DistillerPageWebContents::~DistillerPageWebContents() {
}

void DistillerPageWebContents::InitImpl() {
  DCHECK(browser_context_);
  web_contents_.reset(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser_context_)));
}

void DistillerPageWebContents::LoadURLImpl(const GURL& gurl) {
  DCHECK(web_contents_.get());
  content::WebContentsObserver::Observe(web_contents_.get());
  content::NavigationController::LoadURLParams params(gurl);
  web_contents_->GetController().LoadURLWithParams(params);
}

void DistillerPageWebContents::ExecuteJavaScriptImpl(
    const std::string& script) {
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  DCHECK(host);
  host->ExecuteJavascriptInWebFrameCallbackResult(
      base::string16(),  // frame_xpath
      UTF8ToUTF16(script),
      base::Bind(&DistillerPage::OnExecuteJavaScriptDone,
                 base::Unretained(this)));
}

void DistillerPageWebContents::DidFinishLoad(int64 frame_id,
                                             const GURL& validated_url,
                                             bool is_main_frame,
                                             RenderViewHost* render_view_host) {
  content::WebContentsObserver::Observe(NULL);
  OnLoadURLDone();
}
void DistillerPageWebContents::DidFailLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    int error_code,
    const base::string16& error_description,
    RenderViewHost* render_view_host) {
  content::WebContentsObserver::Observe(NULL);
  OnLoadURLFailed();
}

}  // namespace dom_distiller
