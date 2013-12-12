// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SITE_ISOLATION_POLICY_H_
#define CONTENT_CHILD_SITE_ISOLATION_POLICY_H_

#include <map>
#include <utility>

#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "webkit/common/resource_response_info.h"
#include "webkit/common/resource_type.h"

namespace content {

// SiteIsolationPolicy implements the cross-site document blocking policy (XSDP)
// for Site Isolation. XSDP will monitor network responses to a renderer and
// block illegal responses so that a compromised renderer cannot steal private
// information from other sites. For now SiteIsolationPolicy monitors responses
// to gather various UMA stats to see the compatibility impact of actual
// deployment of the policy. The UMA stat categories SiteIsolationPolicy gathers
// are as follows:
//
// SiteIsolation.AllResponses : # of all network responses.
// SiteIsolation.XSD.DataLength : the length of the first packet of a response.
// SiteIsolation.XSD.MimeType (enum):
//   # of responses from other sites, tagged with a document mime type.
//   0:HTML, 1:XML, 2:JSON, 3:Plain, 4:Others
// SiteIsolation.XSD.[%MIMETYPE].Blocked :
//   blocked # of cross-site document responses grouped by sniffed MIME type.
// SiteIsolation.XSD.[%MIMETYPE].Blocked.RenderableStatusCode :
//   # of responses with renderable status code,
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].Blocked.NonRenderableStatusCode :
//   # of responses with non-renderable status code,
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NoSniffBlocked.RenderableStatusCode :
//   # of responses failed to be sniffed for its MIME type, but blocked by
//   "X-Content-Type-Options: nosniff" header, and with renderable status code
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NoSniffBlocked.NonRenderableStatusCode :
//   # of responses failed to be sniffed for its MIME type, but blocked by
//   "X-Content-Type-Options: nosniff" header, and with non-renderable status
//   code out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NotBlocked :
//   # of responses, but not blocked due to failure of mime sniffing.
// SiteIsolation.XSD.[%MIMETYPE].NotBlocked.MaybeJS :
//   # of responses that are plausibly sniffed to be JavaScript.

class CONTENT_EXPORT SiteIsolationPolicy {
 public:
  // Set activation flag for the UMA data collection for this renderer process.
  static void SetPolicyEnabled(bool enabled);

  // Records the bookkeeping data about the HTTP header information for the
  // request identified by |request_id|. The bookkeeping data is used by
  // ShouldBlockResponse. We have to make sure to call OnRequestComplete to free
  // the bookkeeping data.
  static void OnReceivedResponse(int request_id,
                                 GURL& frame_origin,
                                 GURL& response_url,
                                 ResourceType::Type resource_type,
                                 int origin_pid,
                                 const webkit_glue::ResourceResponseInfo& info);

  // Examines the first network packet in case response_url is registered as a
  // cross-site document by DidReceiveResponse().  In case that this response is
  // blocked, it returns an alternative data to be sent to the renderer in
  // |alternative_data|. This records various kinds of UMA data stats. This
  // function is called only if the length of received data is non-zero.
  static bool ShouldBlockResponse(int request_id,
                                  const char* payload,
                                  int length,
                                  std::string* alternative_data);

  // Clean up booking data registered by OnReceiveResponse and OnReceivedData.
  static void OnRequestComplete(int request_id);

  struct ResponseMetaData {

    enum CanonicalMimeType {
      HTML = 0,
      XML = 1,
      JSON = 2,
      Plain = 3,
      Others = 4,
      MaxCanonicalMimeType,
    };

    ResponseMetaData();

    std::string frame_origin;
    GURL response_url;
    ResourceType::Type resource_type;
    CanonicalMimeType canonical_mime_type;
    int http_status_code;
    bool no_sniff;
  };

  typedef std::map<int, ResponseMetaData> RequestIdToMetaDataMap;
  typedef std::map<int, bool> RequestIdToResultMap;

private:
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, IsBlockableScheme);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, IsSameSite);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, IsValidCorsHeaderSet);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, SniffForHTML);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, SniffForXML);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, SniffForJSON);
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, SniffForJS);

  // Returns the representative mime type enum value of the mime type of
  // response. For example, this returns the same value for all text/xml mime
  // type families such as application/xml, application/rss+xml.
  static ResponseMetaData::CanonicalMimeType GetCanonicalMimeType(
      const std::string& mime_type);

  // Returns whether this scheme is a target of cross-site document
  // policy(XSDP). This returns true only for http://* and https://* urls.
  static bool IsBlockableScheme(const GURL& frame_origin);

  // Returns whether the two urls belong to the same sites.
  static bool IsSameSite(const GURL& frame_origin, const GURL& response_url);

  // Returns whether there's a valid CORS header for frame_origin.  This is
  // simliar to CrossOriginAccessControl::passesAccessControlCheck(), but we use
  // sites as our security domain, not origins.
  // TODO(dsjang): this must be improved to be more accurate to the actual CORS
  // specification. For now, this works conservatively, allowing XSDs that are
  // not allowed by actual CORS rules by ignoring 1) credentials and 2)
  // methods. Preflight requests don't matter here since they are not used to
  // decide whether to block a document or not on the client side.
  static bool IsValidCorsHeaderSet(GURL& frame_origin,
                                   GURL& website_origin,
                                   std::string access_control_origin);

  // Returns whether the given frame is navigating. When this is true, the frame
  // is requesting is a web page to be loaded.
  static bool IsFrameNavigating(WebKit::WebFrame* frame);

  static bool SniffForHTML(const char* data, size_t length);
  static bool SniffForXML(const char* data, size_t length);
  static bool SniffForJSON(const char* data, size_t length);

  static bool MatchesSignature(const char* data,
                               size_t length,
                               const char* signatures[],
                               size_t arr_size);

  // TODO(dsjang): this is only needed for collecting UMA stat. Will be deleted
  // when this class is used for actual blocking.
  static bool SniffForJS(const char* data, size_t length);

  // TODO(dsjang): this is only needed for collecting UMA stat. Will be deleted
  // when this class is used for actual blocking.
  static bool IsRenderableStatusCodeForDocument(int status_code);

  // Maintain the bookkeeping data between OnReceivedResponse and
  // OnReceivedData. The key is a request id maintained by ResourceDispatcher.
  static RequestIdToMetaDataMap* GetRequestIdToMetaDataMap();

  // Maintain the bookkeeping data for OnReceivedData. Blocking decision is made
  // when OnReceivedData is called for the first time for a request, and the
  // decision will remain the same for following data. This map maintains the
  // decision. The key is a request id maintained by ResourceDispatcher.
  static RequestIdToResultMap* GetRequestIdToResultMap();

  // This is false by default, but enables UMA logging and cross-site document
  // blocking.
  static bool g_policy_enabled;

  // Never needs to be constructed/destructed.
  SiteIsolationPolicy() {}
  ~SiteIsolationPolicy() {}

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_CHILD_SITE_ISOLATION_POLICY_H_
