// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

using base::DictionaryValue;

namespace printing {

void PrintSettingsInitializer::InitHeaderFooterStrings(
      const DictionaryValue& job_settings,
      PrintSettings* print_settings) {
  if (!job_settings.GetBoolean(kSettingHeaderFooterEnabled,
                               &print_settings->display_header_footer)) {
    NOTREACHED();
  }
  if (!print_settings->display_header_footer)
    return;

  string16 date = base::TimeFormatShortDateNumeric(base::Time::Now());
  string16 title;
  string16 url;
  if (!job_settings.GetString(kSettingHeaderFooterTitle, &title) ||
      !job_settings.GetString(kSettingHeaderFooterURL, &url)) {
    NOTREACHED();
  }

  print_settings->date = date;
  print_settings->title = title;
  const gfx::FontList& default_fonts =
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BaseFont);
  print_settings->url = gfx::ElideUrl(GURL(url), default_fonts, 0,
                                      std::string());
}

}  // namespace printing
