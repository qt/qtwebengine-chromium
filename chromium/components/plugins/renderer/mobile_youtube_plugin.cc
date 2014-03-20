// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/mobile_youtube_plugin.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "ui/base/webui/jstemplate_builder.h"

using blink::WebFrame;
using blink::WebPlugin;
using blink::WebURLRequest;

const char* const kSlashVSlash = "/v/";
const char* const kSlashESlash = "/e/";

namespace {

std::string GetYoutubeVideoId(const blink::WebPluginParams& params) {
  GURL url(params.url);
  std::string video_id = url.path().substr(strlen(kSlashVSlash));

  // Extract just the video id
  size_t video_id_end = video_id.find('&');
  if (video_id_end != std::string::npos)
    video_id = video_id.substr(0, video_id_end);
  return video_id;
}

std::string HtmlData(const blink::WebPluginParams& params,
                     base::StringPiece template_html) {
  base::DictionaryValue values;
  values.SetString("video_id", GetYoutubeVideoId(params));
  return webui::GetI18nTemplateHtml(template_html, &values);
}

bool IsValidYouTubeVideo(const std::string& path) {
  unsigned len = strlen(kSlashVSlash);

  // check for more than just /v/ or /e/.
  if (path.length() <= len)
    return false;

  std::string str = StringToLowerASCII(path);
  // Youtube flash url can start with /v/ or /e/.
  if (strncmp(str.data(), kSlashVSlash, len) != 0 &&
      strncmp(str.data(), kSlashESlash, len) != 0)
    return false;

  // Start after /v/
  for (unsigned i = len; i < path.length(); i++) {
    char c = str[i];
    if (isalpha(c) || isdigit(c) || c == '_' || c == '-')
      continue;
    // The url can have more parameters such as &hl=en after the video id.
    // Once we start seeing extra parameters we can return true.
    return c == '&' && i > len;
  }
  return true;
}

}  // namespace

namespace plugins {

MobileYouTubePlugin::MobileYouTubePlugin(content::RenderFrame* render_frame,
                                         blink::WebFrame* frame,
                                         const blink::WebPluginParams& params,
                                         base::StringPiece& template_html,
                                         GURL placeholderDataUrl)
    : PluginPlaceholder(render_frame,
                        frame,
                        params,
                        HtmlData(params, template_html),
                        placeholderDataUrl) {}

// static
bool MobileYouTubePlugin::IsYouTubeURL(const GURL& url,
                                       const std::string& mime_type) {
  std::string host = url.host();
  bool is_youtube = EndsWith(host, "youtube.com", true) ||
                    EndsWith(host, "youtube-nocookie.com", true);

  return is_youtube && IsValidYouTubeVideo(url.path()) &&
         LowerCaseEqualsASCII(mime_type, content::kFlashPluginSwfMimeType);
}

void MobileYouTubePlugin::OpenYoutubeUrlCallback(
    const webkit_glue::CppArgumentList& args,
    webkit_glue::CppVariant* result) {
  std::string youtube("vnd.youtube:");
  GURL url(youtube.append(GetYoutubeVideoId(GetPluginParams())));
  WebURLRequest request;
  request.initialize();
  request.setURL(url);
  render_frame()->LoadURLExternally(
      GetFrame(), request, blink::WebNavigationPolicyNewForegroundTab);
}
void MobileYouTubePlugin::BindWebFrame(WebFrame* frame) {
  PluginPlaceholder::BindWebFrame(frame);
  BindCallback("openYoutubeURL",
               base::Bind(&MobileYouTubePlugin::OpenYoutubeUrlCallback,
                          base::Unretained(this)));
}

}  // namespace plugins
