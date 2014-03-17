// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/common/ssl_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class NavigationEntryTest : public testing::Test {
 public:
  NavigationEntryTest() : instance_(NULL) {
  }

  virtual void SetUp() {
    entry1_.reset(new NavigationEntryImpl);

#if !defined(OS_IOS)
    instance_ = static_cast<SiteInstanceImpl*>(SiteInstance::Create(NULL));
#endif
    entry2_.reset(new NavigationEntryImpl(
          instance_, 3,
          GURL("test:url"),
          Referrer(GURL("from"), blink::WebReferrerPolicyDefault),
          ASCIIToUTF16("title"),
          PAGE_TRANSITION_TYPED,
          false));
  }

  virtual void TearDown() {
  }

 protected:
  scoped_ptr<NavigationEntryImpl> entry1_;
  scoped_ptr<NavigationEntryImpl> entry2_;
  // SiteInstances are deleted when their NavigationEntries are gone.
  SiteInstanceImpl* instance_;
};

// Test unique ID accessors
TEST_F(NavigationEntryTest, NavigationEntryUniqueIDs) {
  // Two entries should have different IDs by default
  EXPECT_NE(entry1_->GetUniqueID(), entry2_->GetUniqueID());

  // Can set an entry to have the same ID as another
  entry2_->set_unique_id(entry1_->GetUniqueID());
  EXPECT_EQ(entry1_->GetUniqueID(), entry2_->GetUniqueID());
}

// Test URL accessors
TEST_F(NavigationEntryTest, NavigationEntryURLs) {
  // Start with no virtual_url (even if a url is set)
  EXPECT_FALSE(entry1_->has_virtual_url());
  EXPECT_FALSE(entry2_->has_virtual_url());

  EXPECT_EQ(GURL(), entry1_->GetURL());
  EXPECT_EQ(GURL(), entry1_->GetVirtualURL());
  EXPECT_TRUE(entry1_->GetTitleForDisplay(std::string()).empty());

  // Setting URL affects virtual_url and GetTitleForDisplay
  entry1_->SetURL(GURL("http://www.google.com"));
  EXPECT_EQ(GURL("http://www.google.com"), entry1_->GetURL());
  EXPECT_EQ(GURL("http://www.google.com"), entry1_->GetVirtualURL());
  EXPECT_EQ(ASCIIToUTF16("www.google.com"),
            entry1_->GetTitleForDisplay(std::string()));

  // file:/// URLs should only show the filename.
  entry1_->SetURL(GURL("file:///foo/bar baz.txt"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt"),
            entry1_->GetTitleForDisplay(std::string()));

  // Title affects GetTitleForDisplay
  entry1_->SetTitle(ASCIIToUTF16("Google"));
  EXPECT_EQ(ASCIIToUTF16("Google"), entry1_->GetTitleForDisplay(std::string()));

  // Setting virtual_url doesn't affect URL
  entry2_->SetVirtualURL(GURL("display:url"));
  EXPECT_TRUE(entry2_->has_virtual_url());
  EXPECT_EQ(GURL("test:url"), entry2_->GetURL());
  EXPECT_EQ(GURL("display:url"), entry2_->GetVirtualURL());

  // Having a title set in constructor overrides virtual URL
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_->GetTitleForDisplay(std::string()));

  // User typed URL is independent of the others
  EXPECT_EQ(GURL(), entry1_->GetUserTypedURL());
  EXPECT_EQ(GURL(), entry2_->GetUserTypedURL());
  entry2_->set_user_typed_url(GURL("typedurl"));
  EXPECT_EQ(GURL("typedurl"), entry2_->GetUserTypedURL());
}

// Test Favicon inner class construction.
TEST_F(NavigationEntryTest, NavigationEntryFavicons) {
  EXPECT_EQ(GURL(), entry1_->GetFavicon().url);
  EXPECT_FALSE(entry1_->GetFavicon().valid);
}

// Test SSLStatus inner class
TEST_F(NavigationEntryTest, NavigationEntrySSLStatus) {
  // Default (unknown)
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry1_->GetSSL().security_style);
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry2_->GetSSL().security_style);
  EXPECT_EQ(0, entry1_->GetSSL().cert_id);
  EXPECT_EQ(0U, entry1_->GetSSL().cert_status);
  EXPECT_EQ(-1, entry1_->GetSSL().security_bits);
  int content_status = entry1_->GetSSL().content_status;
  EXPECT_FALSE(!!(content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_FALSE(!!(content_status & SSLStatus::RAN_INSECURE_CONTENT));
}

// Test other basic accessors
TEST_F(NavigationEntryTest, NavigationEntryAccessors) {
  // SiteInstance
  EXPECT_TRUE(entry1_->site_instance() == NULL);
  EXPECT_EQ(instance_, entry2_->site_instance());
  entry1_->set_site_instance(instance_);
  EXPECT_EQ(instance_, entry1_->site_instance());

  // Page type
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry1_->GetPageType());
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry2_->GetPageType());
  entry2_->set_page_type(PAGE_TYPE_INTERSTITIAL);
  EXPECT_EQ(PAGE_TYPE_INTERSTITIAL, entry2_->GetPageType());

  // Referrer
  EXPECT_EQ(GURL(), entry1_->GetReferrer().url);
  EXPECT_EQ(GURL("from"), entry2_->GetReferrer().url);
  entry2_->SetReferrer(
      Referrer(GURL("from2"), blink::WebReferrerPolicyDefault));
  EXPECT_EQ(GURL("from2"), entry2_->GetReferrer().url);

  // Title
  EXPECT_EQ(base::string16(), entry1_->GetTitle());
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_->GetTitle());
  entry2_->SetTitle(ASCIIToUTF16("title2"));
  EXPECT_EQ(ASCIIToUTF16("title2"), entry2_->GetTitle());

  // State
  EXPECT_FALSE(entry1_->GetPageState().IsValid());
  EXPECT_FALSE(entry2_->GetPageState().IsValid());
  entry2_->SetPageState(PageState::CreateFromEncodedData("state"));
  EXPECT_EQ("state", entry2_->GetPageState().ToEncodedData());

  // Page ID
  EXPECT_EQ(-1, entry1_->GetPageID());
  EXPECT_EQ(3, entry2_->GetPageID());
  entry2_->SetPageID(2);
  EXPECT_EQ(2, entry2_->GetPageID());

  // Transition type
  EXPECT_EQ(PAGE_TRANSITION_LINK, entry1_->GetTransitionType());
  EXPECT_EQ(PAGE_TRANSITION_TYPED, entry2_->GetTransitionType());
  entry2_->SetTransitionType(PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(PAGE_TRANSITION_RELOAD, entry2_->GetTransitionType());

  // Is renderer initiated
  EXPECT_FALSE(entry1_->is_renderer_initiated());
  EXPECT_FALSE(entry2_->is_renderer_initiated());
  entry2_->set_is_renderer_initiated(true);
  EXPECT_TRUE(entry2_->is_renderer_initiated());

  // Post Data
  EXPECT_FALSE(entry1_->GetHasPostData());
  EXPECT_FALSE(entry2_->GetHasPostData());
  entry2_->SetHasPostData(true);
  EXPECT_TRUE(entry2_->GetHasPostData());

  // Restored
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE, entry1_->restore_type());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE, entry2_->restore_type());
  entry2_->set_restore_type(
      NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY);
  EXPECT_EQ(NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY,
            entry2_->restore_type());

  // Original URL
  EXPECT_EQ(GURL(), entry1_->GetOriginalRequestURL());
  EXPECT_EQ(GURL(), entry2_->GetOriginalRequestURL());
  entry2_->SetOriginalRequestURL(GURL("original_url"));
  EXPECT_EQ(GURL("original_url"), entry2_->GetOriginalRequestURL());

  // User agent override
  EXPECT_FALSE(entry1_->GetIsOverridingUserAgent());
  EXPECT_FALSE(entry2_->GetIsOverridingUserAgent());
  entry2_->SetIsOverridingUserAgent(true);
  EXPECT_TRUE(entry2_->GetIsOverridingUserAgent());

  // Browser initiated post data
  EXPECT_EQ(NULL, entry1_->GetBrowserInitiatedPostData());
  EXPECT_EQ(NULL, entry2_->GetBrowserInitiatedPostData());
  const int length = 11;
  const unsigned char* raw_data =
      reinterpret_cast<const unsigned char*>("post\n\n\0data");
  std::vector<unsigned char> post_data_vector(raw_data, raw_data+length);
  scoped_refptr<base::RefCountedBytes> post_data =
      base::RefCountedBytes::TakeVector(&post_data_vector);
  entry2_->SetBrowserInitiatedPostData(post_data.get());
  EXPECT_EQ(post_data->front(),
      entry2_->GetBrowserInitiatedPostData()->front());

 // Frame to navigate.
  EXPECT_TRUE(entry1_->GetFrameToNavigate().empty());
  EXPECT_TRUE(entry2_->GetFrameToNavigate().empty());
}

// Test timestamps.
TEST_F(NavigationEntryTest, NavigationEntryTimestamps) {
  EXPECT_EQ(base::Time(), entry1_->GetTimestamp());
  const base::Time now = base::Time::Now();
  entry1_->SetTimestamp(now);
  EXPECT_EQ(now, entry1_->GetTimestamp());
}

// Test extra data stored in the navigation entry.
TEST_F(NavigationEntryTest, NavigationEntryExtraData) {
  base::string16 test_data = ASCIIToUTF16("my search terms");
  base::string16 output;
  entry1_->SetExtraData("search_terms", test_data);

  EXPECT_FALSE(entry1_->GetExtraData("non_existent_key", &output));
  EXPECT_EQ(ASCIIToUTF16(""), output);
  EXPECT_TRUE(entry1_->GetExtraData("search_terms", &output));
  EXPECT_EQ(test_data, output);
  // Data is cleared.
  entry1_->ClearExtraData("search_terms");
  // Content in |output| is not modified if data is not present at the key.
  EXPECT_FALSE(entry1_->GetExtraData("search_terms", &output));
  EXPECT_EQ(test_data, output);
  // Using an empty string shows that the data is not present in the map.
  base::string16 output2;
  EXPECT_FALSE(entry1_->GetExtraData("search_terms", &output2));
  EXPECT_EQ(ASCIIToUTF16(""), output2);
}

}  // namespace content
