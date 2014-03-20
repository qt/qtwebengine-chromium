// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// URL utility functions for Google Documents List API (aka WAPI).

#ifndef GOOGLE_APIS_DRIVE_GDATA_WAPI_URL_GENERATOR_H_
#define GOOGLE_APIS_DRIVE_GDATA_WAPI_URL_GENERATOR_H_

#include <string>

#include "url/gurl.h"

namespace google_apis {

// The class is used to generate URLs for communicating with the WAPI server.
// for production, and the local server for testing.
class GDataWapiUrlGenerator {
 public:
  // The
  GDataWapiUrlGenerator(const GURL& base_url, const GURL& base_download_url);
  ~GDataWapiUrlGenerator();

  // The base URL for communicating with the WAPI server for production.
  static const char kBaseUrlForProduction[];

  // The base URL for the file download server for production.
  static const char kBaseDownloadUrlForProduction[];

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  static GURL AddStandardUrlParams(const GURL& url);

  // Adds additional parameters for initiate uploading as well as standard
  // url params (as AddStandardUrlParams above does).
  static GURL AddInitiateUploadUrlParams(const GURL& url);

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  static GURL AddFeedUrlParams(const GURL& url,
                               int num_items_to_fetch);

  // Generates a URL for getting the resource list feed.
  //
  // The parameters other than |search_string| are mutually exclusive.
  // If |override_url| is non-empty, other parameters are ignored. Or if
  // |override_url| is empty, others are not used. Besides, |search_string|
  // cannot be set together with |start_changestamp|.
  //
  // override_url:
  //   By default, a hard-coded base URL of the WAPI server is used.
  //   The base URL can be overridden by |override_url|.
  //   This is used for handling continuation of feeds (2nd page and onward).
  //
  // start_changestamp
  //   If |start_changestamp| is 0, URL for a full feed is generated.
  //   If |start_changestamp| is non-zero, URL for a delta feed is generated.
  //
  // search_string
  //   If |search_string| is non-empty, q=... parameter is added, and
  //   max-results=... parameter is adjusted for a search.
  //
  // directory_resource_id:
  //   If |directory_resource_id| is non-empty, a URL for fetching documents in
  //   a particular directory is generated.
  //
  GURL GenerateResourceListUrl(
      const GURL& override_url,
      int64 start_changestamp,
      const std::string& search_string,
      const std::string& directory_resource_id) const;

  // Generates a URL for searching resources by title (exact-match).
  // |directory_resource_id| is optional parameter. When it is empty
  // all the existing resources are target of the search. Otherwise,
  // the search target is just under the directory with it.
  GURL GenerateSearchByTitleUrl(
      const std::string& title,
      const std::string& directory_resource_id) const;

  // Generates a URL for getting or editing the resource entry of
  // the given resource ID.
  GURL GenerateEditUrl(const std::string& resource_id) const;

  // Generates a URL for getting or editing the resource entry of the
  // given resource ID without query params.
  // Note that, in order to access to the WAPI server, it is necessary to
  // append some query parameters to the URL. GenerateEditUrl declared above
  // should be used in such cases. This method is designed for constructing
  // the data, such as xml element/attributes in request body containing
  // edit urls.
  GURL GenerateEditUrlWithoutParams(const std::string& resource_id) const;

  // Generates a URL for getting or editing the resource entry of the given
  // resource ID with additionally passed embed origin. This is used to fetch
  // share urls for the sharing dialog to be embedded with the |embed_origin|
  // origin.
  GURL GenerateEditUrlWithEmbedOrigin(const std::string& resource_id,
                                      const GURL& embed_origin) const;

  // Generates a URL for editing the contents in the directory specified
  // by the given resource ID.
  GURL GenerateContentUrl(const std::string& resource_id) const;

  // Generates a URL to remove an entry specified by |resource_id| from
  // the directory specified by the given |parent_resource_id|.
  GURL GenerateResourceUrlForRemoval(const std::string& parent_resource_id,
                                     const std::string& resource_id) const;

  // Generates a URL to initiate uploading a new file to a directory
  // specified by |parent_resource_id|.
  GURL GenerateInitiateUploadNewFileUrl(
      const std::string& parent_resource_id) const;

  // Generates a URL to initiate uploading file content to overwrite a
  // file specified by |resource_id|.
  GURL GenerateInitiateUploadExistingFileUrl(
      const std::string& resource_id) const;

  // Generates a URL for getting the root resource list feed.
  // Used to make changes in the root directory (ex. create a directory in the
  // root directory)
  GURL GenerateResourceListRootUrl() const;

  // Generates a URL for getting the account metadata feed.
  // If |include_installed_apps| is set to true, the response will include the
  // list of installed third party applications.
  GURL GenerateAccountMetadataUrl(bool include_installed_apps) const;

  // Generates a URL for downloading a file.
  GURL GenerateDownloadFileUrl(const std::string& resource_id) const;

 private:
  const GURL base_url_;
  const GURL base_download_url_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_WAPI_URL_GENERATOR_H_
