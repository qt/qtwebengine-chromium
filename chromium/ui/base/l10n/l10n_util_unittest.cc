// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <cstdlib>
#endif

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if !defined(OS_MACOSX)
#include "ui/base/test/data/resource.h"
#endif

namespace {

class StringWrapper {
 public:
  explicit StringWrapper(const string16& string) : string_(string) {}
  const string16& string() const { return string_; }

 private:
  string16 string_;

  DISALLOW_COPY_AND_ASSIGN(StringWrapper);
};

}  // namespace

class L10nUtilTest : public PlatformTest {
};

#if defined(OS_WIN)
// TODO(beng): disabled until app strings move to app.
TEST_F(L10nUtilTest, DISABLED_GetString) {
  std::string s = l10n_util::GetStringUTF8(IDS_SIMPLE);
  EXPECT_EQ(std::string("Hello World!"), s);

  s = l10n_util::GetStringFUTF8(IDS_PLACEHOLDERS,
                                UTF8ToUTF16("chrome"),
                                UTF8ToUTF16("10"));
  EXPECT_EQ(std::string("Hello, chrome. Your number is 10."), s);

  string16 s16 = l10n_util::GetStringFUTF16Int(IDS_PLACEHOLDERS_2, 20);
  EXPECT_EQ(UTF8ToUTF16("You owe me $20."), s16);
}
#endif  // defined(OS_WIN)

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
// On Mac, we are disabling this test because GetApplicationLocale() as an
// API isn't something that we'll easily be able to unit test in this manner.
// The meaning of that API, on the Mac, is "the locale used by Cocoa's main
// nib file", which clearly can't be stubbed by a test app that doesn't use
// Cocoa.

// On Android, we are disabling this test since GetApplicationLocale() just
// returns the system's locale, which, similarly, is not easily unit tested.

#if defined(OS_POSIX) && defined(USE_GLIB) && !defined(OS_CHROMEOS)
const bool kPlatformHasDefaultLocale = 1;
const bool kUseLocaleFromEnvironment = 1;
const bool kSupportsLocalePreference = 0;
#elif defined(OS_WIN)
const bool kPlatformHasDefaultLocale = 1;
const bool kUseLocaleFromEnvironment = 0;
const bool kSupportsLocalePreference = 1;
#else
const bool kPlatformHasDefaultLocale = 0;
const bool kUseLocaleFromEnvironment = 0;
const bool kSupportsLocalePreference = 1;
#endif

void SetDefaultLocaleForTest(const std::string& tag, base::Environment* env) {
  if (kUseLocaleFromEnvironment)
    env->SetVar("LANGUAGE", tag);
  else
    base::i18n::SetICUDefaultLocale(tag);
}

TEST_F(L10nUtilTest, GetAppLocale) {
  scoped_ptr<base::Environment> env;
  // Use a temporary locale dir so we don't have to actually build the locale
  // pak files for this test.
  base::ScopedPathOverride locale_dir_override(ui::DIR_LOCALES);
  base::FilePath new_locale_dir;
  ASSERT_TRUE(PathService::Get(ui::DIR_LOCALES, &new_locale_dir));
  // Make fake locale files.
  std::string filenames[] = {
    "en-US",
    "en-GB",
    "fr",
    "es-419",
    "es",
    "zh-TW",
    "zh-CN",
    "he",
    "fil",
    "nb",
    "am",
    "ca",
    "ca@valencia",
  };

  for (size_t i = 0; i < arraysize(filenames); ++i) {
    base::FilePath filename = new_locale_dir.AppendASCII(
        filenames[i] + ".pak");
    file_util::WriteFile(filename, "", 0);
  }

  // Keep a copy of ICU's default locale before we overwrite it.
  const std::string original_locale = base::i18n::GetConfiguredLocale();

  if (kPlatformHasDefaultLocale && kUseLocaleFromEnvironment) {
    env.reset(base::Environment::Create());

    // Test the support of LANGUAGE environment variable.
    base::i18n::SetICUDefaultLocale("en-US");
    env->SetVar("LANGUAGE", "xx:fr_CA");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale(std::string()));

    env->SetVar("LANGUAGE", "xx:yy:en_gb.utf-8@quot");
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));

    env->SetVar("LANGUAGE", "xx:zh-hk");
    EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(std::string()));

    // We emulate gettext's behavior here, which ignores LANG/LC_MESSAGES/LC_ALL
    // when LANGUAGE is specified. If no language specified in LANGUAGE is
    // valid,
    // then just fallback to the default language, which is en-US for us.
    base::i18n::SetICUDefaultLocale("fr-FR");
    env->SetVar("LANGUAGE", "xx:yy");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string()));

    env->SetVar("LANGUAGE", "/fr:zh_CN");
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(std::string()));

    // Test prioritization of the different environment variables.
    env->SetVar("LANGUAGE", "fr");
    env->SetVar("LC_ALL", "es");
    env->SetVar("LC_MESSAGES", "he");
    env->SetVar("LANG", "nb");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale(std::string()));
    env->UnSetVar("LANGUAGE");
    EXPECT_EQ("es", l10n_util::GetApplicationLocale(std::string()));
    env->UnSetVar("LC_ALL");
    EXPECT_EQ("he", l10n_util::GetApplicationLocale(std::string()));
    env->UnSetVar("LC_MESSAGES");
    EXPECT_EQ("nb", l10n_util::GetApplicationLocale(std::string()));
    env->UnSetVar("LANG");

    SetDefaultLocaleForTest("ca", env.get());
    EXPECT_EQ("ca", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("ca-ES", env.get());
    EXPECT_EQ("ca", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("ca@valencia", env.get());
    EXPECT_EQ("ca@valencia", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("ca_ES@valencia", env.get());
    EXPECT_EQ("ca@valencia", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("ca_ES.UTF8@valencia", env.get());
    EXPECT_EQ("ca@valencia", l10n_util::GetApplicationLocale(std::string()));
  }

  SetDefaultLocaleForTest("en-US", env.get());
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string()));

  SetDefaultLocaleForTest("xx", env.get());
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string()));

  if (!kPlatformHasDefaultLocale) {
    // ChromeOS & embedded use only browser prefs in GetApplicationLocale(),
    // ignoring the environment, and default to en-US. Other platforms honor
    // the default locale from the OS or environment.
    SetDefaultLocaleForTest("en-GB", env.get());
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-GB"));

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-AU"));

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-NZ"));

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-CA"));

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-ZA"));
  } else {
    // Most platforms have an OS-provided locale. This locale is preferred.
    SetDefaultLocaleForTest("en-GB", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("fr-CA", env.get());
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("es-MX", env.get());
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("es-AR", env.get());
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("es-ES", env.get());
    EXPECT_EQ("es", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("es", env.get());
    EXPECT_EQ("es", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("zh-HK", env.get());
    EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("zh-MO", env.get());
    EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("zh-SG", env.get());
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("en-CA", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("en-AU", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("en-NZ", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));

    SetDefaultLocaleForTest("en-ZA", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
  }

  if (kSupportsLocalePreference) {
    // On windows, the user can override the locale in preferences.
    base::i18n::SetICUDefaultLocale("en-US");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr"));
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr-CA"));

    base::i18n::SetICUDefaultLocale("en-US");
    // Aliases iw, no, tl to he, nb, fil.
    EXPECT_EQ("he", l10n_util::GetApplicationLocale("iw"));
    EXPECT_EQ("nb", l10n_util::GetApplicationLocale("no"));
    EXPECT_EQ("fil", l10n_util::GetApplicationLocale("tl"));
    // es-419 and es-XX (where XX is not Spain) should be
    // mapped to es-419 (Latin American Spanish).
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale("es-419"));
    EXPECT_EQ("es", l10n_util::GetApplicationLocale("es-ES"));
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale("es-AR"));

    base::i18n::SetICUDefaultLocale("es-AR");
    EXPECT_EQ("es", l10n_util::GetApplicationLocale("es"));

    base::i18n::SetICUDefaultLocale("zh-HK");
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale("zh-CN"));

    base::i18n::SetICUDefaultLocale("he");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("en"));
  }

#if defined(OS_WIN)
  // Amharic should be blocked unless OS is Vista or newer.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    base::i18n::SetICUDefaultLocale("am");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));
    base::i18n::SetICUDefaultLocale("en-GB");
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("am"));
  } else {
    base::i18n::SetICUDefaultLocale("am");
    EXPECT_EQ("am", l10n_util::GetApplicationLocale(""));
    base::i18n::SetICUDefaultLocale("en-GB");
    EXPECT_EQ("am", l10n_util::GetApplicationLocale("am"));
  }
#endif  // defined(OS_WIN)

  // Clean up.
  base::i18n::SetICUDefaultLocale(original_locale);
}
#endif  // !defined(OS_MACOSX)

TEST_F(L10nUtilTest, SortStringsUsingFunction) {
  std::vector<StringWrapper*> strings;
  strings.push_back(new StringWrapper(UTF8ToUTF16("C")));
  strings.push_back(new StringWrapper(UTF8ToUTF16("d")));
  strings.push_back(new StringWrapper(UTF8ToUTF16("b")));
  strings.push_back(new StringWrapper(UTF8ToUTF16("a")));
  l10n_util::SortStringsUsingMethod("en-US",
                                    &strings,
                                    &StringWrapper::string);
  ASSERT_TRUE(UTF8ToUTF16("a") == strings[0]->string());
  ASSERT_TRUE(UTF8ToUTF16("b") == strings[1]->string());
  ASSERT_TRUE(UTF8ToUTF16("C") == strings[2]->string());
  ASSERT_TRUE(UTF8ToUTF16("d") == strings[3]->string());
  STLDeleteElements(&strings);
}

/**
 * Helper method for validating strings that require direcitonal markup.
 * Checks that parentheses are enclosed in appropriate direcitonal markers.
 */
void CheckUiDisplayNameForLocale(const std::string& locale,
                                 const std::string& display_locale,
                                 bool is_rtl) {
  EXPECT_EQ(true, base::i18n::IsRTL());
  string16 result = l10n_util::GetDisplayNameForLocale(locale,
                                                       display_locale,
                                                       /* is_for_ui */ true);

  bool rtl_direction = true;
  for (size_t i = 0; i < result.length() - 1; i++) {
    char16 ch = result.at(i);
    switch (ch) {
    case base::i18n::kLeftToRightMark:
    case base::i18n::kLeftToRightEmbeddingMark:
      rtl_direction = false;
      break;
    case base::i18n::kRightToLeftMark:
    case base::i18n::kRightToLeftEmbeddingMark:
      rtl_direction = true;
      break;
    case '(':
    case ')':
      EXPECT_EQ(is_rtl, rtl_direction);
    }
  }
}

TEST_F(L10nUtilTest, GetDisplayNameForLocale) {
  // TODO(jungshik): Make this test more extensive.
  // Test zh-CN and zh-TW are treated as zh-Hans and zh-Hant.
  string16 result = l10n_util::GetDisplayNameForLocale("zh-CN", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Chinese (Simplified Han)"), result);

  result = l10n_util::GetDisplayNameForLocale("zh-TW", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Chinese (Traditional Han)"), result);

  result = l10n_util::GetDisplayNameForLocale("pt-BR", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Portuguese (Brazil)"), result);

  result = l10n_util::GetDisplayNameForLocale("es-419", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Spanish (Latin America)"), result);

  result = l10n_util::GetDisplayNameForLocale("-BR", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Brazil"), result);

  result = l10n_util::GetDisplayNameForLocale("xyz-xyz", "en", false);
  EXPECT_EQ(ASCIIToUTF16("xyz (XYZ)"), result);

#if !defined(TOOLKIT_GTK)
  // Check for directional markers when using RTL languages to ensure that
  // direction neutral characters such as parentheses are properly formatted.

  // Keep a copy of ICU's default locale before we overwrite it.
  const std::string original_locale = base::i18n::GetConfiguredLocale();

  base::i18n::SetICUDefaultLocale("he");
  CheckUiDisplayNameForLocale("en-US", "en", false);
  CheckUiDisplayNameForLocale("en-US", "he", true);

  // Clean up.
  base::i18n::SetICUDefaultLocale(original_locale);
#endif

  // ToUpper and ToLower should work with embedded NULLs.
  const size_t length_with_null = 4;
  char16 buf_with_null[length_with_null] = { 0, 'a', 0, 'b' };
  string16 string16_with_null(buf_with_null, length_with_null);

  string16 upper_with_null = base::i18n::ToUpper(string16_with_null);
  ASSERT_EQ(length_with_null, upper_with_null.size());
  EXPECT_TRUE(upper_with_null[0] == 0 && upper_with_null[1] == 'A' &&
              upper_with_null[2] == 0 && upper_with_null[3] == 'B');

  string16 lower_with_null = base::i18n::ToLower(upper_with_null);
  ASSERT_EQ(length_with_null, upper_with_null.size());
  EXPECT_TRUE(lower_with_null[0] == 0 && lower_with_null[1] == 'a' &&
              lower_with_null[2] == 0 && lower_with_null[3] == 'b');
}

TEST_F(L10nUtilTest, GetDisplayNameForCountry) {
  string16 result = l10n_util::GetDisplayNameForCountry("BR", "en");
  EXPECT_EQ(ASCIIToUTF16("Brazil"), result);

  result = l10n_util::GetDisplayNameForCountry("419", "en");
  EXPECT_EQ(ASCIIToUTF16("Latin America"), result);

  result = l10n_util::GetDisplayNameForCountry("xyz", "en");
  EXPECT_EQ(ASCIIToUTF16("XYZ"), result);
}

TEST_F(L10nUtilTest, GetParentLocales) {
  std::vector<std::string> locales;
  const std::string top_locale("sr_Cyrl_RS");
  l10n_util::GetParentLocales(top_locale, &locales);

  ASSERT_EQ(3U, locales.size());
  EXPECT_EQ("sr_Cyrl_RS", locales[0]);
  EXPECT_EQ("sr_Cyrl", locales[1]);
  EXPECT_EQ("sr", locales[2]);
}

TEST_F(L10nUtilTest, IsValidLocaleSyntax) {
  // Test valid locales.
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fr"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("de"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("pt"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fil"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("haw"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en-US"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_US"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_GB"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("pt-BR"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_CN"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hans"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hans_CN"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hant"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hant_TW"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fr_CA"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("i-klingon"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("es-419"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_IE_PREEURO"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_IE_u_cu_IEP"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_IE@currency=IEP"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fr@x=y"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zn_CN@foo=bar"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax(
      "fr@collation=phonebook;calendar=islamic-civil"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax(
      "sr_Latn_RS_REVISED@currency=USD"));

  // Test invalid locales.
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax(std::string()));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("x"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("12"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("456"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("a1"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("enUS"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("zhcn"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en.US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en#US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("-en-US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US-"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("123-en-US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("Latin"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("German"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("pt--BR"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("sl-macedonia"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("@"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@x"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@x="));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@=y"));
}
