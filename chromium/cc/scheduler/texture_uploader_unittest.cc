// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/texture_uploader.h"

#include "cc/base/util.h"
#include "cc/debug/test_web_graphics_context_3d.h"
#include "cc/resources/prioritized_resource.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using WebKit::WGC3Denum;
using WebKit::WGC3Dint;
using WebKit::WGC3Dsizei;
using WebKit::WebGLId;
using WebKit::WGC3Duint;

namespace cc {
namespace {

class TestWebGraphicsContext3DTextureUpload : public TestWebGraphicsContext3D {
 public:
  TestWebGraphicsContext3DTextureUpload()
      : result_available_(0),
        unpack_alignment_(4) {}

  virtual void pixelStorei(WGC3Denum pname, WGC3Dint param) OVERRIDE {
    switch (pname) {
      case GL_UNPACK_ALIGNMENT:
        // Param should be a power of two <= 8.
        EXPECT_EQ(0, param & (param - 1));
        EXPECT_GE(8, param);
        switch (param) {
          case 1:
          case 2:
          case 4:
          case 8:
            unpack_alignment_ = param;
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }

  virtual void getQueryObjectuivEXT(WebGLId, WGC3Denum type, WGC3Duint* value)
      OVERRIDE {
    switch (type) {
      case GL_QUERY_RESULT_AVAILABLE_EXT:
        *value = result_available_;
        break;
      default:
        *value = 0;
        break;
    }
  }

  virtual void texSubImage2D(WGC3Denum target,
                             WGC3Dint level,
                             WGC3Dint xoffset,
                             WGC3Dint yoffset,
                             WGC3Dsizei width,
                             WGC3Dsizei height,
                             WGC3Denum format,
                             WGC3Denum type,
                             const void* pixels) OVERRIDE {
    EXPECT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    EXPECT_EQ(0, level);
    EXPECT_LE(0, width);
    EXPECT_LE(0, height);
    EXPECT_LE(0, xoffset);
    EXPECT_LE(0, yoffset);
    EXPECT_LE(0, width);
    EXPECT_LE(0, height);

    // Check for allowed format/type combination.
    unsigned int bytes_per_pixel = 0;
    switch (format) {
      case GL_ALPHA:
        EXPECT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
        bytes_per_pixel = 1;
        break;
      case GL_RGB:
        EXPECT_NE(static_cast<unsigned>(GL_UNSIGNED_SHORT_4_4_4_4), type);
        EXPECT_NE(static_cast<unsigned>(GL_UNSIGNED_SHORT_5_5_5_1), type);
        switch (type) {
          case GL_UNSIGNED_BYTE:
            bytes_per_pixel = 3;
            break;
          case GL_UNSIGNED_SHORT_5_6_5:
            bytes_per_pixel = 2;
            break;
        }
        break;
      case GL_RGBA:
        EXPECT_NE(static_cast<unsigned>(GL_UNSIGNED_SHORT_5_6_5), type);
        switch (type) {
          case GL_UNSIGNED_BYTE:
            bytes_per_pixel = 4;
            break;
          case GL_UNSIGNED_SHORT_4_4_4_4:
            bytes_per_pixel = 2;
            break;
          case GL_UNSIGNED_SHORT_5_5_5_1:
            bytes_per_pixel = 2;
            break;
        }
        break;
      case GL_LUMINANCE:
        EXPECT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
        bytes_per_pixel = 1;
        break;
      case GL_LUMINANCE_ALPHA:
        EXPECT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
        bytes_per_pixel = 2;
        break;
    }

    // If NULL, we aren't checking texture contents.
    if (pixels == NULL)
      return;

    const uint8* bytes = static_cast<const uint8*>(pixels);
    // We'll expect the first byte of every row to be 0x1, and the last byte to
    // be 0x2.
    const unsigned int stride =
        RoundUp(bytes_per_pixel * width, unpack_alignment_);
    for (WGC3Dsizei row = 0; row < height; ++row) {
      const uint8* row_bytes =
          bytes + (xoffset * bytes_per_pixel + (yoffset + row) * stride);
      EXPECT_EQ(0x1, row_bytes[0]);
      EXPECT_EQ(0x2, row_bytes[width * bytes_per_pixel - 1]);
    }
  }

  void SetResultAvailable(unsigned result_available) {
    result_available_ = result_available;
  }

 private:
  unsigned result_available_;
  unsigned unpack_alignment_;

  DISALLOW_COPY_AND_ASSIGN(TestWebGraphicsContext3DTextureUpload);
};

void UploadTexture(TextureUploader* uploader,
                   ResourceFormat format,
                   gfx::Size size,
                   const uint8* data) {
  uploader->Upload(data,
                   gfx::Rect(size),
                   gfx::Rect(size),
                   gfx::Vector2d(),
                   format,
                   size);
}

TEST(TextureUploaderTest, NumBlockingUploads) {
  scoped_ptr<TestWebGraphicsContext3DTextureUpload> fake_context(
      new TestWebGraphicsContext3DTextureUpload);
  scoped_ptr<TextureUploader> uploader =
      TextureUploader::Create(fake_context.get(), false, false);

  fake_context->SetResultAvailable(0);
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  EXPECT_EQ(1u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  EXPECT_EQ(2u, uploader->NumBlockingUploads());

  fake_context->SetResultAvailable(1);
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
}

TEST(TextureUploaderTest, MarkPendingUploadsAsNonBlocking) {
  scoped_ptr<TestWebGraphicsContext3DTextureUpload> fake_context(
      new TestWebGraphicsContext3DTextureUpload);
  scoped_ptr<TextureUploader> uploader =
      TextureUploader::Create(fake_context.get(), false, false);

  fake_context->SetResultAvailable(0);
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  EXPECT_EQ(2u, uploader->NumBlockingUploads());

  uploader->MarkPendingUploadsAsNonBlocking();
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  EXPECT_EQ(1u, uploader->NumBlockingUploads());

  fake_context->SetResultAvailable(1);
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(), NULL);
  uploader->MarkPendingUploadsAsNonBlocking();
  EXPECT_EQ(0u, uploader->NumBlockingUploads());
}

TEST(TextureUploaderTest, UploadContentsTest) {
  scoped_ptr<TestWebGraphicsContext3DTextureUpload> fake_context(
      new TestWebGraphicsContext3DTextureUpload);
  scoped_ptr<TextureUploader> uploader =
      TextureUploader::Create(fake_context.get(), false, false);
  uint8 buffer[256 * 256 * 4];

  // Upload a tightly packed 256x256 RGBA texture.
  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < 256; ++i) {
    // Mark the beginning and end of each row, for the test.
    buffer[i * 4 * 256] = 0x1;
    buffer[(i + 1) * 4 * 256 - 1] = 0x2;
  }
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(256, 256), buffer);

  // Upload a tightly packed 41x43 RGBA texture.
  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < 43; ++i) {
    // Mark the beginning and end of each row, for the test.
    buffer[i * 4 * 41] = 0x1;
    buffer[(i + 1) * 4 * 41 - 1] = 0x2;
  }
  UploadTexture(uploader.get(), RGBA_8888, gfx::Size(41, 43), buffer);

  // Upload a tightly packed 82x86 LUMINANCE texture.
  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < 86; ++i) {
    // Mark the beginning and end of each row, for the test.
    buffer[i * 1 * 82] = 0x1;
    buffer[(i + 1) * 82 - 1] = 0x2;
  }
  UploadTexture(uploader.get(), LUMINANCE_8, gfx::Size(82, 86), buffer);
}

}  // namespace
}  // namespace cc
