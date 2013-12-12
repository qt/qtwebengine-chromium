// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_parse.h"
#include "url/url_test_utils.h"
#include "url/url_util.h"

TEST(URLUtilTest, FindAndCompareScheme) {
  url_parse::Component found_scheme;

  // Simple case where the scheme is found and matches.
  const char kStr1[] = "http://www.com/";
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "http", NULL));
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "http", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(0, 4));

  // A case where the scheme is found and doesn't match.
  EXPECT_FALSE(url_util::FindAndCompareScheme(
      kStr1, static_cast<int>(strlen(kStr1)), "https", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(0, 4));

  // A case where there is no scheme.
  const char kStr2[] = "httpfoobar";
  EXPECT_FALSE(url_util::FindAndCompareScheme(
      kStr2, static_cast<int>(strlen(kStr2)), "http", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component());

  // When there is an empty scheme, it should match the empty scheme.
  const char kStr3[] = ":foo.com/";
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      kStr3, static_cast<int>(strlen(kStr3)), "", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(0, 0));

  // But when there is no scheme, it should fail.
  EXPECT_FALSE(url_util::FindAndCompareScheme("", 0, "", &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component());

  // When there is a whitespace char in scheme, it should canonicalize the url
  // before comparison.
  const char whtspc_str[] = " \r\n\tjav\ra\nscri\tpt:alert(1)";
  EXPECT_TRUE(url_util::FindAndCompareScheme(
      whtspc_str, static_cast<int>(strlen(whtspc_str)), "javascript",
      &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(1, 10));

  // Control characters should be stripped out on the ends, and kept in the
  // middle.
  const char ctrl_str[] = "\02jav\02scr\03ipt:alert(1)";
  EXPECT_FALSE(url_util::FindAndCompareScheme(
      ctrl_str, static_cast<int>(strlen(ctrl_str)), "javascript",
      &found_scheme));
  EXPECT_TRUE(found_scheme == url_parse::Component(1, 11));
}

TEST(URLUtilTest, ReplaceComponents) {
  url_parse::Parsed parsed;
  url_canon::RawCanonOutputT<char> output;
  url_parse::Parsed new_parsed;

  // Check that the following calls do not cause crash
  url_canon::Replacements<char> replacements;
  replacements.SetRef("test", url_parse::Component(0, 4));
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  replacements.ClearRef();
  replacements.SetHost("test", url_parse::Component(0, 4));
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);

  replacements.ClearHost();
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents(NULL, 0, parsed, replacements, NULL, &output,
                              &new_parsed);
  url_util::ReplaceComponents("", 0, parsed, replacements, NULL, &output,
                              &new_parsed);
}

static std::string CheckReplaceScheme(const char* base_url,
                                      const char* scheme) {
  // Make sure the input is canonicalized.
  url_canon::RawCanonOutput<32> original;
  url_parse::Parsed original_parsed;
  url_util::Canonicalize(base_url, strlen(base_url), NULL,
                         &original, &original_parsed);

  url_canon::Replacements<char> replacements;
  replacements.SetScheme(scheme, url_parse::Component(0, strlen(scheme)));

  std::string output_string;
  url_canon::StdStringCanonOutput output(&output_string);
  url_parse::Parsed output_parsed;
  url_util::ReplaceComponents(original.data(), original.length(),
                              original_parsed, replacements, NULL,
                              &output, &output_parsed);

  output.Complete();
  return output_string;
}

TEST(URLUtilTest, ReplaceScheme) {
  EXPECT_EQ("https://google.com/",
            CheckReplaceScheme("http://google.com/", "https"));
  EXPECT_EQ("file://google.com/",
            CheckReplaceScheme("http://google.com/", "file"));
  EXPECT_EQ("http://home/Build",
            CheckReplaceScheme("file:///Home/Build", "http"));
  EXPECT_EQ("javascript:foo",
            CheckReplaceScheme("about:foo", "javascript"));
  EXPECT_EQ("://google.com/",
            CheckReplaceScheme("http://google.com/", ""));
  EXPECT_EQ("http://google.com/",
            CheckReplaceScheme("about:google.com", "http"));
  EXPECT_EQ("http:", CheckReplaceScheme("", "http"));

#ifdef WIN32
  // Magic Windows drive letter behavior when converting to a file URL.
  EXPECT_EQ("file:///E:/foo/",
            CheckReplaceScheme("http://localhost/e:foo/", "file"));
#endif

  // This will probably change to "about://google.com/" when we fix
  // http://crbug.com/160 which should also be an acceptable result.
  EXPECT_EQ("about://google.com/",
            CheckReplaceScheme("http://google.com/", "about"));
}

TEST(URLUtilTest, DecodeURLEscapeSequences) {
  struct DecodeCase {
    const char* input;
    const char* output;
  } decode_cases[] = {
    {"hello, world", "hello, world"},
    {"%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/",
     "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0B\x0C\x0D\x0e\x0f/"},
    {"%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/",
     "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1B\x1C\x1D\x1e\x1f/"},
    {"%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/",
     " !\"#$%&'()*+,-.//"},
    {"%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/",
     "0123456789:;<=>?/"},
    {"%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/",
     "@ABCDEFGHIJKLMNO/"},
    {"%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/",
     "PQRSTUVWXYZ[\\]^_/"},
    {"%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/",
     "`abcdefghijklmno/"},
    {"%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/",
     "pqrstuvwxyz{|}~\x7f/"},
    // Test un-UTF-8-ization.
    {"%e4%bd%a0%e5%a5%bd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(decode_cases); i++) {
    const char* input = decode_cases[i].input;
    url_canon::RawCanonOutputT<base::char16> output;
    url_util::DecodeURLEscapeSequences(input, strlen(input), &output);
    EXPECT_EQ(decode_cases[i].output,
              url_test_utils::ConvertUTF16ToUTF8(
                base::string16(output.data(), output.length())));
  }

  // Our decode should decode %00
  const char zero_input[] = "%00";
  url_canon::RawCanonOutputT<base::char16> zero_output;
  url_util::DecodeURLEscapeSequences(zero_input, strlen(zero_input),
                                     &zero_output);
  EXPECT_NE("%00",
            url_test_utils::ConvertUTF16ToUTF8(
              base::string16(zero_output.data(), zero_output.length())));

  // Test the error behavior for invalid UTF-8.
  const char invalid_input[] = "%e4%a0%e5%a5%bd";
  const base::char16 invalid_expected[4] = {0x00e4, 0x00a0, 0x597d, 0};
  url_canon::RawCanonOutputT<base::char16> invalid_output;
  url_util::DecodeURLEscapeSequences(invalid_input, strlen(invalid_input),
                                     &invalid_output);
  EXPECT_EQ(base::string16(invalid_expected),
            base::string16(invalid_output.data(), invalid_output.length()));
}

TEST(URLUtilTest, TestEncodeURIComponent) {
  struct EncodeCase {
    const char* input;
    const char* output;
  } encode_cases[] = {
    {"hello, world", "hello%2C%20world"},
    {"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
     "%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F"},
    {"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F",
     "%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F"},
    {" !\"#$%&'()*+,-./",
     "%20!%22%23%24%25%26'()*%2B%2C-.%2F"},
    {"0123456789:;<=>?",
     "0123456789%3A%3B%3C%3D%3E%3F"},
    {"@ABCDEFGHIJKLMNO",
     "%40ABCDEFGHIJKLMNO"},
    {"PQRSTUVWXYZ[\\]^_",
     "PQRSTUVWXYZ%5B%5C%5D%5E_"},
    {"`abcdefghijklmno",
     "%60abcdefghijklmno"},
    {"pqrstuvwxyz{|}~\x7f",
     "pqrstuvwxyz%7B%7C%7D~%7F"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(encode_cases); i++) {
    const char* input = encode_cases[i].input;
    url_canon::RawCanonOutputT<char> buffer;
    url_util::EncodeURIComponent(input, strlen(input), &buffer);
    std::string output(buffer.data(), buffer.length());
    EXPECT_EQ(encode_cases[i].output, output);
  }
}

TEST(URLUtilTest, TestResolveRelativeWithNonStandardBase) {
  // This tests non-standard (in the sense that GURL::IsStandard() == false)
  // hierarchical schemes.
  struct ResolveRelativeCase {
    const char* base;
    const char* rel;
    bool is_valid;
    const char* out;
  } resolve_non_standard_cases[] = {
      // Resolving a relative path against a non-hierarchical URL should fail.
    {"scheme:opaque_data", "/path", false, ""},
      // Resolving a relative path against a non-standard authority-based base
      // URL doesn't alter the authority section.
    {"scheme://Authority/", "../path", true, "scheme://Authority/path"},
      // A non-standard hierarchical base is resolved with path URL
      // canoncialization rules.
    {"data:/Blah:Blah/", "file.html", true, "data:/Blah:Blah/file.html"},
    {"data:/Path/../part/part2", "file.html", true,
      "data:/Path/../part/file.html"},
      // Path URL canonicalization rules also apply to non-standard authority-
      // based URLs.
    {"custom://Authority/", "file.html", true,
      "custom://Authority/file.html"},
    {"custom://Authority/", "other://Auth/", true, "other://Auth/"},
    {"custom://Authority/", "../../file.html", true,
      "custom://Authority/file.html"},
    {"custom://Authority/path/", "file.html", true,
      "custom://Authority/path/file.html"},
    {"custom://Authority:NoCanon/path/", "file.html", true,
      "custom://Authority:NoCanon/path/file.html"},
      // It's still possible to get an invalid path URL.
    {"custom://Invalid:!#Auth/", "file.html", false, ""},
      // A path with an authority section gets canonicalized under standard URL
      // rules, even though the base was non-standard.
    {"content://content.Provider/", "//other.Provider", true,
      "content://other.provider/"},
      // Resolving an absolute URL doesn't cause canonicalization of the
      // result.
    {"about:blank", "custom://Authority", true, "custom://Authority"},
      // Fragment URLs can be resolved against a non-standard base.
    {"scheme://Authority/path", "#fragment", true,
      "scheme://Authority/path#fragment"},
    {"scheme://Authority/", "#fragment", true, "scheme://Authority/#fragment"},
      // Resolving should fail if the base URL is authority-based but is
      // missing a path component (the '/' at the end).
    {"scheme://Authority", "path", false, ""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(resolve_non_standard_cases); i++) {
    const ResolveRelativeCase& test_data = resolve_non_standard_cases[i];
    url_parse::Parsed base_parsed;
    url_parse::ParsePathURL(test_data.base, strlen(test_data.base),
                            &base_parsed);

    std::string resolved;
    url_canon::StdStringCanonOutput output(&resolved);
    url_parse::Parsed resolved_parsed;
    bool valid =
        url_util::ResolveRelative(test_data.base, strlen(test_data.base),
                                  base_parsed,
                                  test_data.rel, strlen(test_data.rel),
                                  NULL, &output, &resolved_parsed);
    output.Complete();

    EXPECT_EQ(test_data.is_valid, valid) << i;
    if (test_data.is_valid && valid)
      EXPECT_EQ(test_data.out, resolved) << i;
  }
}
