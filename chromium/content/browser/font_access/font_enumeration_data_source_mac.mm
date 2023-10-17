// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_data_source_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

base::ScopedCFTypeRef<CFStringRef> GetLocalizedString(CTFontDescriptorRef fd,
                                                      CFStringRef attribute) {
  return base::ScopedCFTypeRef<CFStringRef>(base::mac::CFCast<CFStringRef>(
      CTFontDescriptorCopyLocalizedAttribute(fd, attribute, nullptr)));
}

base::ScopedCFTypeRef<CFStringRef> GetString(CTFontDescriptorRef fd,
                                             CFStringRef attribute) {
  return base::ScopedCFTypeRef<CFStringRef>(base::mac::CFCast<CFStringRef>(
      CTFontDescriptorCopyAttribute(fd, attribute)));
}

}  // namespace

FontEnumerationDataSourceMac::FontEnumerationDataSourceMac() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FontEnumerationDataSourceMac::~FontEnumerationDataSourceMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FontEnumerationDataSourceMac::IsValidFontMac(
    const CTFontDescriptorRef& fd) {
  base::ScopedCFTypeRef<CFStringRef> cf_postscript_name =
      GetString(fd, kCTFontNameAttribute);
  base::ScopedCFTypeRef<CFStringRef> cf_full_name =
      GetLocalizedString(fd, kCTFontDisplayNameAttribute);
  base::ScopedCFTypeRef<CFStringRef> cf_family =
      GetString(fd, kCTFontFamilyNameAttribute);
  base::ScopedCFTypeRef<CFStringRef> cf_style =
      GetString(fd, kCTFontStyleNameAttribute);

  if (!cf_postscript_name || !cf_full_name || !cf_family || !cf_style) {
    // Check for invalid attribute returns as MacOS may allow
    // OS-level installation of fonts for some of these.
    return false;
  }
  this->cf_postscript_name_ = cf_postscript_name;
  this->cf_full_name_ = cf_full_name;
  this->cf_family_ = cf_family;
  this->cf_style_ = cf_style;
  return true;
}

blink::FontEnumerationTable FontEnumerationDataSourceMac::GetFonts(
    const std::string& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::FontEnumerationTable font_enumeration_table;

  @autoreleasepool {
    CFTypeRef values[1] = {kCFBooleanTrue};
    base::ScopedCFTypeRef<CFDictionaryRef> options(CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void**)kCTFontCollectionRemoveDuplicatesOption,
        (const void**)&values,
        /*numValues=*/1, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    base::ScopedCFTypeRef<CTFontCollectionRef> collection(
        CTFontCollectionCreateFromAvailableFonts(options));

    base::ScopedCFTypeRef<CFArrayRef> font_descs(
        CTFontCollectionCreateMatchingFontDescriptors(collection));

    // Used to filter duplicates.
    std::set<std::string> fonts_seen;

    for (CFIndex i = 0; i < CFArrayGetCount(font_descs); ++i) {
      CTFontDescriptorRef fd = base::mac::CFCast<CTFontDescriptorRef>(
          CFArrayGetValueAtIndex(font_descs, i));
      if (!IsValidFontMac(fd)) {
        // Skip invalid fonts.
        continue;
      }

      std::string postscript_name =
          base::SysCFStringRefToUTF8(cf_postscript_name_.get());

      auto it_and_success = fonts_seen.emplace(postscript_name);
      if (!it_and_success.second) {
        // Skip duplicate.
        continue;
      }

      blink::FontEnumerationTable_FontData* data =
          font_enumeration_table.add_fonts();
      data->set_postscript_name(std::move(postscript_name));
      data->set_full_name(base::SysCFStringRefToUTF8(cf_full_name_.get()));
      data->set_family(base::SysCFStringRefToUTF8(cf_family_.get()));
      data->set_style(base::SysCFStringRefToUTF8(cf_style_.get()));
    }

    return font_enumeration_table;
  }
}

}  // namespace content
