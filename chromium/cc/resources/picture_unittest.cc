// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkTileGridPicture.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

TEST(PictureTest, AsBase64String) {
  SkGraphics::Init();

  gfx::Rect layer_rect(100, 100);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(100, 100);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  scoped_ptr<base::Value> tmp;

  SkPaint red_paint;
  red_paint.setColor(SkColorSetARGB(255, 255, 0, 0));
  SkPaint green_paint;
  green_paint.setColor(SkColorSetARGB(255, 0, 255, 0));

  // Invalid picture (not a dict).
  tmp.reset(new base::StringValue("abc!@#$%"));
  scoped_refptr<Picture> invalid_picture =
      Picture::CreateFromValue(tmp.get());
  EXPECT_TRUE(!invalid_picture.get());

  // Single full-size rect picture.
  content_layer_client.add_draw_rect(layer_rect, red_paint);
  scoped_refptr<Picture> one_rect_picture = Picture::Create(layer_rect);
  one_rect_picture->Record(&content_layer_client,
                           tile_grid_info);
  scoped_ptr<base::Value> serialized_one_rect(
      one_rect_picture->AsValue());

  // Reconstruct the picture.
  scoped_refptr<Picture> one_rect_picture_check =
      Picture::CreateFromValue(serialized_one_rect.get());
  EXPECT_TRUE(!!one_rect_picture_check.get());

  // Check for equivalence.
  unsigned char one_rect_buffer[4 * 100 * 100] = {0};
  DrawPicture(one_rect_buffer, layer_rect, one_rect_picture);
  unsigned char one_rect_buffer_check[4 * 100 * 100] = {0};
  DrawPicture(one_rect_buffer_check, layer_rect, one_rect_picture_check);

  EXPECT_EQ(one_rect_picture->LayerRect(),
            one_rect_picture_check->LayerRect());
  EXPECT_EQ(one_rect_picture->OpaqueRect(),
            one_rect_picture_check->OpaqueRect());
  EXPECT_TRUE(
      memcmp(one_rect_buffer, one_rect_buffer_check, 4 * 100 * 100) == 0);

  // Two rect picture.
  content_layer_client.add_draw_rect(gfx::Rect(25, 25, 50, 50), green_paint);
  scoped_refptr<Picture> two_rect_picture = Picture::Create(layer_rect);
  two_rect_picture->Record(&content_layer_client,
                           tile_grid_info);

  scoped_ptr<base::Value> serialized_two_rect(
      two_rect_picture->AsValue());

  // Reconstruct the picture.
  scoped_refptr<Picture> two_rect_picture_check =
      Picture::CreateFromValue(serialized_two_rect.get());
  EXPECT_TRUE(!!two_rect_picture_check.get());

  // Check for equivalence.
  unsigned char two_rect_buffer[4 * 100 * 100] = {0};
  DrawPicture(two_rect_buffer, layer_rect, two_rect_picture);
  unsigned char two_rect_buffer_check[4 * 100 * 100] = {0};
  DrawPicture(two_rect_buffer_check, layer_rect, two_rect_picture_check);

  EXPECT_EQ(two_rect_picture->LayerRect(),
            two_rect_picture_check->LayerRect());
  EXPECT_EQ(two_rect_picture->OpaqueRect(),
            two_rect_picture_check->OpaqueRect());
  EXPECT_TRUE(
      memcmp(two_rect_buffer, two_rect_buffer_check, 4 * 100 * 100) == 0);
}

TEST(PictureTest, PixelRefIterator) {
  gfx::Rect layer_rect(2048, 2048);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(512, 512);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  // Lazy pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  SkBitmap lazy_bitmap[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        CreateBitmap(gfx::Size(500, 500), "lazy", &lazy_bitmap[y][x]);
        SkPaint paint;
        content_layer_client.add_draw_bitmap(
            lazy_bitmap[y][x],
            gfx::Point(x * 512 + 6, y * 512 + 6), paint);
      }
    }
  }

  scoped_refptr<Picture> picture = Picture::Create(layer_rect);
  picture->Record(&content_layer_client,
                  tile_grid_info);
  picture->GatherPixelRefs(tile_grid_info);

  // Default iterator does not have any pixel refs
  {
    Picture::PixelRefIterator iterator;
    EXPECT_FALSE(iterator);
  }
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      Picture::PixelRefIterator iterator(gfx::Rect(x * 512, y * 512, 500, 500),
                                         picture.get());
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(*iterator == lazy_bitmap[y][x].pixelRef()) << x << " " << y;
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    Picture::PixelRefIterator iterator(gfx::Rect(512, 512, 2048, 2048),
                                       picture.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++iterator);
  }

  // Copy test.
  Picture::PixelRefIterator iterator(gfx::Rect(512, 512, 2048, 2048),
                                     picture.get());
  EXPECT_TRUE(iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());

  // copy now points to the same spot as iterator,
  // but both can be incremented independently.
  Picture::PixelRefIterator copy = iterator;
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
  EXPECT_FALSE(++iterator);

  EXPECT_TRUE(copy);
  EXPECT_TRUE(*copy == lazy_bitmap[2][1].pixelRef());
  EXPECT_TRUE(++copy);
  EXPECT_TRUE(*copy == lazy_bitmap[2][3].pixelRef());
  EXPECT_TRUE(++copy);
  EXPECT_TRUE(*copy == lazy_bitmap[3][2].pixelRef());
  EXPECT_FALSE(++copy);
}

TEST(PictureTest, PixelRefIteratorNonZeroLayer) {
  gfx::Rect layer_rect(1024, 0, 2048, 2048);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(512, 512);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  // Lazy pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  SkBitmap lazy_bitmap[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        CreateBitmap(gfx::Size(500, 500), "lazy", &lazy_bitmap[y][x]);
        SkPaint paint;
        content_layer_client.add_draw_bitmap(
            lazy_bitmap[y][x],
            gfx::Point(1024 + x * 512 + 6, y * 512 + 6), paint);
      }
    }
  }

  scoped_refptr<Picture> picture = Picture::Create(layer_rect);
  picture->Record(&content_layer_client,
                  tile_grid_info);
  picture->GatherPixelRefs(tile_grid_info);

  // Default iterator does not have any pixel refs
  {
    Picture::PixelRefIterator iterator;
    EXPECT_FALSE(iterator);
  }
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      Picture::PixelRefIterator iterator(
          gfx::Rect(1024 + x * 512, y * 512, 500, 500), picture.get());
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(*iterator == lazy_bitmap[y][x].pixelRef());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    Picture::PixelRefIterator iterator(gfx::Rect(1024 + 512, 512, 2048, 2048),
                                       picture.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++iterator);
  }

  // Copy test.
  {
    Picture::PixelRefIterator iterator(gfx::Rect(1024 + 512, 512, 2048, 2048),
                                       picture.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());

    // copy now points to the same spot as iterator,
    // but both can be incremented independently.
    Picture::PixelRefIterator copy = iterator;
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++iterator);

    EXPECT_TRUE(copy);
    EXPECT_TRUE(*copy == lazy_bitmap[2][1].pixelRef());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(*copy == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(*copy == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++copy);
  }

  // Non intersecting rects
  {
    Picture::PixelRefIterator iterator(gfx::Rect(0, 0, 1000, 1000),
                                       picture.get());
    EXPECT_FALSE(iterator);
  }
  {
    Picture::PixelRefIterator iterator(gfx::Rect(3500, 0, 1000, 1000),
                                       picture.get());
    EXPECT_FALSE(iterator);
  }
  {
    Picture::PixelRefIterator iterator(gfx::Rect(0, 1100, 1000, 1000),
                                       picture.get());
    EXPECT_FALSE(iterator);
  }
  {
    Picture::PixelRefIterator iterator(gfx::Rect(3500, 1100, 1000, 1000),
                                       picture.get());
    EXPECT_FALSE(iterator);
  }
}

TEST(PictureTest, PixelRefIteratorOnePixelQuery) {
  gfx::Rect layer_rect(2048, 2048);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(512, 512);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  // Lazy pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  SkBitmap lazy_bitmap[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        CreateBitmap(gfx::Size(500, 500), "lazy", &lazy_bitmap[y][x]);
        SkPaint paint;
        content_layer_client.add_draw_bitmap(
            lazy_bitmap[y][x],
            gfx::Point(x * 512 + 6, y * 512 + 6), paint);
      }
    }
  }

  scoped_refptr<Picture> picture = Picture::Create(layer_rect);
  picture->Record(&content_layer_client,
                  tile_grid_info);
  picture->GatherPixelRefs(tile_grid_info);

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      Picture::PixelRefIterator iterator(
          gfx::Rect(x * 512, y * 512 + 256, 1, 1), picture.get());
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(*iterator == lazy_bitmap[y][x].pixelRef());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
}

TEST(PictureTest, CreateFromSkpValue) {
  SkGraphics::Init();

  gfx::Rect layer_rect(100, 200);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(100, 200);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  scoped_ptr<base::Value> tmp;

  SkPaint red_paint;
  red_paint.setColor(SkColorSetARGB(255, 255, 0, 0));
  SkPaint green_paint;
  green_paint.setColor(SkColorSetARGB(255, 0, 255, 0));

  // Invalid picture (not a dict).
  tmp.reset(new base::StringValue("abc!@#$%"));
  scoped_refptr<Picture> invalid_picture =
      Picture::CreateFromSkpValue(tmp.get());
  EXPECT_TRUE(!invalid_picture.get());

  // Single full-size rect picture.
  content_layer_client.add_draw_rect(layer_rect, red_paint);
  scoped_refptr<Picture> one_rect_picture = Picture::Create(layer_rect);
  one_rect_picture->Record(&content_layer_client,
                           tile_grid_info);
  scoped_ptr<base::Value> serialized_one_rect(
      one_rect_picture->AsValue());

  const base::DictionaryValue* value = NULL;
  EXPECT_TRUE(serialized_one_rect->GetAsDictionary(&value));

  // Decode the picture from base64.
  const base::Value* skp_value;
  EXPECT_TRUE(value->Get("skp64", &skp_value));

  // Reconstruct the picture.
  scoped_refptr<Picture> one_rect_picture_check =
      Picture::CreateFromSkpValue(skp_value);
  EXPECT_TRUE(!!one_rect_picture_check.get());

  EXPECT_EQ(100, one_rect_picture_check->LayerRect().width());
  EXPECT_EQ(200, one_rect_picture_check->LayerRect().height());
  EXPECT_EQ(100, one_rect_picture_check->OpaqueRect().width());
  EXPECT_EQ(200, one_rect_picture_check->OpaqueRect().height());
}
}  // namespace
}  // namespace cc
