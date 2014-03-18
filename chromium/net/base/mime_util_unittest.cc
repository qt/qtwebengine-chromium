// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(MimeUtilTest, ExtensionTest) {
  const struct {
    const base::FilePath::CharType* extension;
    const char* mime_type;
    bool valid;
  } tests[] = {
    { FILE_PATH_LITERAL("png"), "image/png", true },
    { FILE_PATH_LITERAL("css"), "text/css", true },
    { FILE_PATH_LITERAL("pjp"), "image/jpeg", true },
    { FILE_PATH_LITERAL("pjpeg"), "image/jpeg", true },
    { FILE_PATH_LITERAL("not an extension / for sure"), "", false },
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    rv = GetMimeTypeFromExtension(tests[i].extension, &mime_type);
    EXPECT_EQ(tests[i].valid, rv);
    if (rv)
      EXPECT_EQ(tests[i].mime_type, mime_type);
  }
}

TEST(MimeUtilTest, FileTest) {
  const struct {
    const base::FilePath::CharType* file_path;
    const char* mime_type;
    bool valid;
  } tests[] = {
    { FILE_PATH_LITERAL("c:\\foo\\bar.css"), "text/css", true },
    { FILE_PATH_LITERAL("c:\\blah"), "", false },
    { FILE_PATH_LITERAL("/usr/local/bin/mplayer"), "", false },
    { FILE_PATH_LITERAL("/home/foo/bar.css"), "text/css", true },
    { FILE_PATH_LITERAL("/blah."), "", false },
    { FILE_PATH_LITERAL("c:\\blah."), "", false },
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    rv = GetMimeTypeFromFile(base::FilePath(tests[i].file_path),
                                  &mime_type);
    EXPECT_EQ(tests[i].valid, rv);
    if (rv)
      EXPECT_EQ(tests[i].mime_type, mime_type);
  }
}

TEST(MimeUtilTest, LookupTypes) {
  EXPECT_FALSE(IsUnsupportedTextMimeType("text/banana"));
  EXPECT_TRUE(IsUnsupportedTextMimeType("text/vcard"));

  EXPECT_TRUE(IsSupportedImageMimeType("image/jpeg"));
  EXPECT_FALSE(IsSupportedImageMimeType("image/lolcat"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/html"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/css"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/banana"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("text/vcard"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("application/virus"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-x509-user-cert"));
#if defined(OS_ANDROID)
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-x509-ca-cert"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-pkcs12"));
#endif

  EXPECT_TRUE(IsSupportedMimeType("image/jpeg"));
  EXPECT_FALSE(IsSupportedMimeType("image/lolcat"));
  EXPECT_TRUE(IsSupportedMimeType("text/html"));
  EXPECT_TRUE(IsSupportedMimeType("text/banana"));
  EXPECT_FALSE(IsSupportedMimeType("text/vcard"));
  EXPECT_FALSE(IsSupportedMimeType("application/virus"));
}

TEST(MimeUtilTest, MatchesMimeType) {
  EXPECT_TRUE(MatchesMimeType("*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/*"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("application/*+xml",
                                   "application/html+xml"));
  EXPECT_TRUE(MatchesMimeType("application/*+xml", "application/+xml"));
  EXPECT_TRUE(MatchesMimeType("aaa*aaa", "aaaaaa"));
  EXPECT_TRUE(MatchesMimeType("*", std::string()));
  EXPECT_FALSE(MatchesMimeType("video/", "video/x-mpeg"));
  EXPECT_FALSE(MatchesMimeType(std::string(), "video/x-mpeg"));
  EXPECT_FALSE(MatchesMimeType(std::string(), std::string()));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg", std::string()));
  EXPECT_FALSE(MatchesMimeType("application/*+xml", "application/xml"));
  EXPECT_FALSE(MatchesMimeType("application/*+xml",
                                    "application/html+xmlz"));
  EXPECT_FALSE(MatchesMimeType("application/*+xml",
                                    "applcation/html+xml"));
  EXPECT_FALSE(MatchesMimeType("aaa*aaa", "aaaaa"));

  EXPECT_TRUE(MatchesMimeType("*", "video/x-mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/mpeg"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/mpeg;param=other"));
  EXPECT_TRUE(MatchesMimeType("video/*;param=val", "video/mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg", "video/x-mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val",
                              "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param2=val2",
                               "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param2=val2",
                               "video/x-mpeg;param2=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val;param2=val2",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param2=val2;param=val",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param3=val3;param=val",
                               "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val ;param2=val2 ",
                              "video/x-mpeg;param=val;param2=val2"));

  EXPECT_TRUE(MatchesMimeType("*/*;param=val", "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("*/*;param=val", "video/x-mpeg;param=val2"));

  EXPECT_TRUE(MatchesMimeType("*", "*"));
  EXPECT_TRUE(MatchesMimeType("*", "*/*"));
  EXPECT_TRUE(MatchesMimeType("*/*", "*/*"));
  EXPECT_TRUE(MatchesMimeType("*/*", "*"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/*"));
  EXPECT_FALSE(MatchesMimeType("video/*", "*/*"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/*"));
  EXPECT_TRUE(MatchesMimeType("video/*;param=val", "video/*;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/*;param=val2"));

  EXPECT_TRUE(MatchesMimeType("ab*cd", "abxxxcd"));
  EXPECT_TRUE(MatchesMimeType("ab*cd", "abx/xcd"));
  EXPECT_TRUE(MatchesMimeType("ab/*cd", "ab/xxxcd"));
}

// Note: codecs should only be a list of 2 or fewer; hence the restriction of
// results' length to 2.
TEST(MimeUtilTest, ParseCodecString) {
  const struct {
    const char* original;
    size_t expected_size;
    const char* results[2];
  } tests[] = {
    { "\"bogus\"",                  1, { "bogus" }            },
    { "0",                          1, { "0" }                },
    { "avc1.42E01E, mp4a.40.2",     2, { "avc1",   "mp4a" }   },
    { "\"mp4v.20.240, mp4a.40.2\"", 2, { "mp4v",   "mp4a" }   },
    { "mp4v.20.8, samr",            2, { "mp4v",   "samr" }   },
    { "\"theora, vorbis\"",         2, { "theora", "vorbis" } },
    { "",                           0, { }                    },
    { "\"\"",                       0, { }                    },
    { "\"   \"",                    0, { }                    },
    { ",",                          2, { "", "" }             },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::vector<std::string> codecs_out;
    ParseCodecString(tests[i].original, &codecs_out, true);
    ASSERT_EQ(tests[i].expected_size, codecs_out.size());
    for (size_t j = 0; j < tests[i].expected_size; ++j)
      EXPECT_EQ(tests[i].results[j], codecs_out[j]);
  }

  // Test without stripping the codec type.
  std::vector<std::string> codecs_out;
  ParseCodecString("avc1.42E01E, mp4a.40.2", &codecs_out, false);
  ASSERT_EQ(2u, codecs_out.size());
  EXPECT_EQ("avc1.42E01E", codecs_out[0]);
  EXPECT_EQ("mp4a.40.2", codecs_out[1]);
}

TEST(MimeUtilTest, TestIsMimeType) {
  std::string nonAscii("application/nonutf8");
  EXPECT_TRUE(IsMimeType(nonAscii));
#if defined(OS_WIN)
  nonAscii.append(WideToUTF8(std::wstring(L"\u2603")));
#else
  nonAscii.append("\u2603");  // unicode snowman
#endif
  EXPECT_FALSE(IsMimeType(nonAscii));

  EXPECT_TRUE(IsMimeType("application/mime"));
  EXPECT_TRUE(IsMimeType("audio/mime"));
  EXPECT_TRUE(IsMimeType("example/mime"));
  EXPECT_TRUE(IsMimeType("image/mime"));
  EXPECT_TRUE(IsMimeType("message/mime"));
  EXPECT_TRUE(IsMimeType("model/mime"));
  EXPECT_TRUE(IsMimeType("multipart/mime"));
  EXPECT_TRUE(IsMimeType("text/mime"));
  EXPECT_TRUE(IsMimeType("TEXT/mime"));
  EXPECT_TRUE(IsMimeType("Text/mime"));
  EXPECT_TRUE(IsMimeType("TeXt/mime"));
  EXPECT_TRUE(IsMimeType("video/mime"));
  EXPECT_TRUE(IsMimeType("video/mime;parameter"));
  EXPECT_TRUE(IsMimeType("*/*"));
  EXPECT_TRUE(IsMimeType("*"));

  EXPECT_TRUE(IsMimeType("x-video/mime"));
  EXPECT_TRUE(IsMimeType("X-Video/mime"));
  EXPECT_FALSE(IsMimeType("x-video/"));
  EXPECT_FALSE(IsMimeType("x-/mime"));
  EXPECT_FALSE(IsMimeType("mime/looking"));
  EXPECT_FALSE(IsMimeType("text/"));
}

TEST(MimeUtilTest, TestToIANAMediaType) {
  EXPECT_EQ("", GetIANAMediaType("texting/driving"));
  EXPECT_EQ("", GetIANAMediaType("ham/sandwich"));
  EXPECT_EQ("", GetIANAMediaType(std::string()));
  EXPECT_EQ("", GetIANAMediaType("/application/hamsandwich"));

  EXPECT_EQ("application", GetIANAMediaType("application/poodle-wrestler"));
  EXPECT_EQ("audio", GetIANAMediaType("audio/mpeg"));
  EXPECT_EQ("example", GetIANAMediaType("example/yomomma"));
  EXPECT_EQ("image", GetIANAMediaType("image/png"));
  EXPECT_EQ("message", GetIANAMediaType("message/sipfrag"));
  EXPECT_EQ("model", GetIANAMediaType("model/vrml"));
  EXPECT_EQ("multipart", GetIANAMediaType("multipart/mixed"));
  EXPECT_EQ("text", GetIANAMediaType("text/plain"));
  EXPECT_EQ("video", GetIANAMediaType("video/H261"));
}

TEST(MimeUtilTest, TestGetExtensionsForMimeType) {
  const struct {
    const char* mime_type;
    size_t min_expected_size;
    const char* contained_result;
  } tests[] = {
    { "text/plain", 2, "txt" },
    { "*",          0, NULL  },
    { "message/*",  1, "eml" },
    { "MeSsAge/*",  1, "eml" },
    { "image/bmp",  1, "bmp" },
    { "video/*",    6, "mp4" },
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_IOS)
    { "video/*",    6, "mpg" },
#else
    { "video/*",    6, "mpeg" },
#endif
    { "audio/*",    6, "oga" },
    { "aUDIo/*",    6, "wav" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::vector<base::FilePath::StringType> extensions;
    GetExtensionsForMimeType(tests[i].mime_type, &extensions);
    ASSERT_TRUE(tests[i].min_expected_size <= extensions.size());

    if (!tests[i].contained_result)
      continue;

    bool found = false;
    for (size_t j = 0; !found && j < extensions.size(); ++j) {
#if defined(OS_WIN)
      if (extensions[j] == UTF8ToWide(tests[i].contained_result))
        found = true;
#else
      if (extensions[j] == tests[i].contained_result)
        found = true;
#endif
    }
    ASSERT_TRUE(found) << "Must find at least the contained result within "
                       << tests[i].mime_type;
  }
}

TEST(MimeUtilTest, TestGetCertificateMimeTypeForMimeType) {
  EXPECT_EQ(CERTIFICATE_MIME_TYPE_X509_USER_CERT,
            GetCertificateMimeTypeForMimeType("application/x-x509-user-cert"));
#if defined(OS_ANDROID)
  // Only Android supports CA Certs and PKCS12 archives.
  EXPECT_EQ(CERTIFICATE_MIME_TYPE_X509_CA_CERT,
            GetCertificateMimeTypeForMimeType("application/x-x509-ca-cert"));
  EXPECT_EQ(CERTIFICATE_MIME_TYPE_PKCS12_ARCHIVE,
            GetCertificateMimeTypeForMimeType("application/x-pkcs12"));
#else
  EXPECT_EQ(CERTIFICATE_MIME_TYPE_UNKNOWN,
            GetCertificateMimeTypeForMimeType("application/x-x509-ca-cert"));
  EXPECT_EQ(CERTIFICATE_MIME_TYPE_UNKNOWN,
            GetCertificateMimeTypeForMimeType("application/x-pkcs12"));
#endif
  EXPECT_EQ(CERTIFICATE_MIME_TYPE_UNKNOWN,
            GetCertificateMimeTypeForMimeType("text/plain"));
}

TEST(MimeUtilTest, TestAddMultipartValueForUpload) {
  const char* ref_output = "--boundary\r\nContent-Disposition: form-data;"
                           " name=\"value name\"\r\nContent-Type: content type"
                           "\r\n\r\nvalue\r\n"
                           "--boundary\r\nContent-Disposition: form-data;"
                           " name=\"value name\"\r\n\r\nvalue\r\n"
                           "--boundary--\r\n";
  std::string post_data;
  AddMultipartValueForUpload("value name", "value", "boundary",
                             "content type", &post_data);
  AddMultipartValueForUpload("value name", "value", "boundary",
                             "", &post_data);
  AddMultipartFinalDelimiterForUpload("boundary", &post_data);
  EXPECT_STREQ(ref_output, post_data.c_str());
}

}  // namespace net
