// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using base::MD5DigestToBase16;

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  int first_black_row = static_cast<int>(frame->coded_size().height() *
                                         white_to_black);
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int row = 0; row < frame->coded_size().height(); ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, frame->stride(VideoFrame::kYPlane));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (int row = 0; row < frame->coded_size().height(); row += 2) {
    memset(u_plane, 0x80, frame->stride(VideoFrame::kUPlane));
    memset(v_plane, 0x80, frame->stride(VideoFrame::kVPlane));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame, uint32 expect_rgb_color) {
  ASSERT_EQ(VideoFrame::YV12, yv12_frame->format());
  ASSERT_EQ(yv12_frame->stride(VideoFrame::kUPlane),
            yv12_frame->stride(VideoFrame::kVPlane));
  ASSERT_EQ(
      yv12_frame->coded_size().width() & (VideoFrame::kFrameSizeAlignment - 1),
      0);
  ASSERT_EQ(
      yv12_frame->coded_size().height() & (VideoFrame::kFrameSizeAlignment - 1),
      0);

  size_t bytes_per_row = yv12_frame->coded_size().width() * 4u;
  uint8* rgb_data = reinterpret_cast<uint8*>(
      base::AlignedAlloc(bytes_per_row * yv12_frame->coded_size().height() +
                             VideoFrame::kFrameSizePadding,
                         VideoFrame::kFrameAddressAlignment));

  media::ConvertYUVToRGB32(yv12_frame->data(VideoFrame::kYPlane),
                           yv12_frame->data(VideoFrame::kUPlane),
                           yv12_frame->data(VideoFrame::kVPlane),
                           rgb_data,
                           yv12_frame->coded_size().width(),
                           yv12_frame->coded_size().height(),
                           yv12_frame->stride(VideoFrame::kYPlane),
                           yv12_frame->stride(VideoFrame::kUPlane),
                           bytes_per_row,
                           media::YV12);

  for (int row = 0; row < yv12_frame->coded_size().height(); ++row) {
    uint32* rgb_row_data = reinterpret_cast<uint32*>(
        rgb_data + (bytes_per_row * row));
    for (int col = 0; col < yv12_frame->coded_size().width(); ++col) {
      SCOPED_TRACE(
          base::StringPrintf("Checking (%d, %d)", row, col));
      EXPECT_EQ(expect_rgb_color, rgb_row_data[col]);
    }
  }

  base::AlignedFree(rgb_data);
}

// Fill each plane to its reported extents and verify accessors report non
// zero values.  Additionally, for the first plane verify the rows and
// row_bytes values are correct.
void ExpectFrameExtents(VideoFrame::Format format, int planes,
                        int bytes_per_pixel, const char* expected_hash) {
  const unsigned char kFillByte = 0x80;
  const int kWidth = 61;
  const int kHeight = 31;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  gfx::Size size(kWidth, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      format, size, gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());

  for(int plane = 0; plane < planes; plane++) {
    SCOPED_TRACE(base::StringPrintf("Checking plane %d", plane));
    EXPECT_TRUE(frame->data(plane));
    EXPECT_TRUE(frame->stride(plane));
    EXPECT_TRUE(frame->rows(plane));
    EXPECT_TRUE(frame->row_bytes(plane));

    if (plane == 0) {
      EXPECT_EQ(frame->rows(plane), kHeight);
      EXPECT_EQ(frame->row_bytes(plane), kWidth * bytes_per_pixel);
    }

    memset(frame->data(plane), kFillByte,
           frame->stride(plane) * frame->rows(plane));
  }

  base::MD5Context context;
  base::MD5Init(&context);
  frame->HashFrameForTesting(&context);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), expected_hash);
}

TEST(VideoFrame, CreateFrame) {
  const int kWidth = 64;
  const int kHeight = 48;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateFrame(media::VideoFrame::YV12, size, gfx::Rect(size),
                              size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Test VideoFrame implementation.
  EXPECT_EQ(media::VideoFrame::YV12, frame->format());
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 0.0f);
    ExpectFrameColor(frame.get(), 0xFF000000);
  }
  base::MD5Digest digest;
  base::MD5Context context;
  base::MD5Init(&context);
  frame->HashFrameForTesting(&context);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "9065c841d9fca49186ef8b4ef547e79b");
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 1.0f);
    ExpectFrameColor(frame.get(), 0xFFFFFFFF);
  }
  base::MD5Init(&context);
  frame->HashFrameForTesting(&context);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "911991d51438ad2e1a40ed5f6fc7c796");

  // Test an empty frame.
  frame = VideoFrame::CreateEOSFrame();
  EXPECT_TRUE(frame->end_of_stream());
}

TEST(VideoFrame, CreateBlackFrame) {
  const int kWidth = 2;
  const int kHeight = 2;
  const uint8 kExpectedYRow[] = { 0, 0 };
  const uint8 kExpectedUVRow[] = { 128 };

  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame.get());

  // Test basic properties.
  EXPECT_EQ(0, frame->GetTimestamp().InMicroseconds());
  EXPECT_FALSE(frame->end_of_stream());

  // Test |frame| properties.
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  EXPECT_EQ(kWidth, frame->coded_size().width());
  EXPECT_EQ(kHeight, frame->coded_size().height());

  // Test frames themselves.
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int y = 0; y < frame->coded_size().height(); ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, y_plane, arraysize(kExpectedYRow)));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (int y = 0; y < frame->coded_size().height() / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, u_plane, arraysize(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, v_plane, arraysize(kExpectedUVRow)));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Ensure each frame is properly sized and allocated.  Will trigger OOB reads
// and writes as well as incorrect frame hashes otherwise.
TEST(VideoFrame, CheckFrameExtents) {
  // Each call consists of a VideoFrame::Format, # of planes, bytes per pixel,
  // and the expected hash of all planes if filled with kFillByte (defined in
  // ExpectFrameExtents).
  ExpectFrameExtents(
      VideoFrame::YV12,   3, 1, "71113bdfd4c0de6cf62f48fb74f7a0b1");
  ExpectFrameExtents(
      VideoFrame::YV16,   3, 1, "9bb99ac3ff350644ebff4d28dc01b461");
}

static void TextureCallback(uint32* called_sync_point, uint32 sync_point) {
  *called_sync_point = sync_point;
}

// Verify the TextureNoLongerNeededCallback is called when VideoFrame is
// destroyed with the original sync point.
TEST(VideoFrame, TextureNoLongerNeededCallbackIsCalled) {
  uint32 sync_point = 7;
  uint32 called_sync_point = 0;

  {
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(new VideoFrame::MailboxHolder(
            gpu::Mailbox(),
            sync_point,
            base::Bind(&TextureCallback, &called_sync_point))),
        5,                                        // texture_target
        gfx::Size(10, 10),                        // coded_size
        gfx::Rect(10, 10),                        // visible_rect
        gfx::Size(10, 10),                        // natural_size
        base::TimeDelta(),                        // timestamp
        base::Callback<void(const SkBitmap&)>(),  // read_pixels_cb
        base::Closure());                         // no_longer_needed_cb

    EXPECT_EQ(0u, called_sync_point);
  }
  EXPECT_EQ(sync_point, called_sync_point);
}

// Verify the TextureNoLongerNeededCallback is called when VideoFrame is
// destroyed with the new sync point, when the mailbox is accessed by a caller.
TEST(VideoFrame, TextureNoLongerNeededCallbackAfterTakingAndReleasingMailbox) {
  uint32 called_sync_point = 0;

  gpu::Mailbox mailbox;
  mailbox.name[0] = 50;
  uint32 sync_point = 7;
  uint32 target = 9;

  {
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(new VideoFrame::MailboxHolder(
            mailbox,
            sync_point,
            base::Bind(&TextureCallback, &called_sync_point))),
        target,
        gfx::Size(10, 10),                        // coded_size
        gfx::Rect(10, 10),                        // visible_rect
        gfx::Size(10, 10),                        // natural_size
        base::TimeDelta(),                        // timestamp
        base::Callback<void(const SkBitmap&)>(),  // read_pixels_cb
        base::Closure());                         // no_longer_needed_cb

    VideoFrame::MailboxHolder* mailbox_holder = frame->texture_mailbox();

    EXPECT_EQ(mailbox.name[0], mailbox_holder->mailbox().name[0]);
    EXPECT_EQ(sync_point, mailbox_holder->sync_point());
    EXPECT_EQ(target, frame->texture_target());

    // Finish using the mailbox_holder and drop our reference.
    sync_point = 10;
    mailbox_holder->Resync(sync_point);
  }
  EXPECT_EQ(sync_point, called_sync_point);
}

}  // namespace media
