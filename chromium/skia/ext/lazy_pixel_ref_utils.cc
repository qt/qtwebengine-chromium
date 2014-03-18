// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/lazy_pixel_ref_utils.h"

#include <algorithm>

#include "skia/ext/lazy_pixel_ref.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDraw.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/src/core/SkRasterClip.h"

namespace skia {

namespace {

// URI label for a lazily decoded SkPixelRef.
const char kLabelLazyDecoded[] = "lazy";

class LazyPixelRefSet {
 public:
  LazyPixelRefSet(
      std::vector<LazyPixelRefUtils::PositionLazyPixelRef>* pixel_refs)
      : pixel_refs_(pixel_refs) {}

  void Add(SkPixelRef* pixel_ref, const SkRect& rect) {
    // Only save lazy pixel refs.
    if (pixel_ref->getURI() &&
        !strcmp(pixel_ref->getURI(), kLabelLazyDecoded)) {
      LazyPixelRefUtils::PositionLazyPixelRef position_pixel_ref;
      position_pixel_ref.lazy_pixel_ref =
          static_cast<skia::LazyPixelRef*>(pixel_ref);
      position_pixel_ref.pixel_ref_rect = rect;
      pixel_refs_->push_back(position_pixel_ref);
    }
  }

 private:
  std::vector<LazyPixelRefUtils::PositionLazyPixelRef>* pixel_refs_;
};

class GatherPixelRefDevice : public SkBitmapDevice {
 public:
  GatherPixelRefDevice(const SkBitmap& bm, LazyPixelRefSet* lazy_pixel_ref_set)
      : SkBitmapDevice(bm), lazy_pixel_ref_set_(lazy_pixel_ref_set) {}

  virtual void clear(SkColor color) SK_OVERRIDE {}
  virtual void writePixels(const SkBitmap& bitmap,
                           int x,
                           int y,
                           SkCanvas::Config8888 config8888) SK_OVERRIDE {}

  virtual void drawPaint(const SkDraw& draw, const SkPaint& paint) SK_OVERRIDE {
    SkBitmap bitmap;
    if (GetBitmapFromPaint(paint, &bitmap)) {
      SkRect clip_rect = SkRect::Make(draw.fRC->getBounds());
      AddBitmap(bitmap, clip_rect);
    }
  }

  virtual void drawPoints(const SkDraw& draw,
                          SkCanvas::PointMode mode,
                          size_t count,
                          const SkPoint points[],
                          const SkPaint& paint) SK_OVERRIDE {
    SkBitmap bitmap;
    if (!GetBitmapFromPaint(paint, &bitmap))
      return;

    if (count == 0)
      return;

    SkPoint min_point = points[0];
    SkPoint max_point = points[0];
    for (size_t i = 1; i < count; ++i) {
      const SkPoint& point = points[i];
      min_point.set(std::min(min_point.x(), point.x()),
                    std::min(min_point.y(), point.y()));
      max_point.set(std::max(max_point.x(), point.x()),
                    std::max(max_point.y(), point.y()));
    }

    SkRect bounds = SkRect::MakeLTRB(
        min_point.x(), min_point.y(), max_point.x(), max_point.y());

    GatherPixelRefDevice::drawRect(draw, bounds, paint);
  }
  virtual void drawRect(const SkDraw& draw,
                        const SkRect& rect,
                        const SkPaint& paint) SK_OVERRIDE {
    SkBitmap bitmap;
    if (GetBitmapFromPaint(paint, &bitmap)) {
      SkRect mapped_rect;
      draw.fMatrix->mapRect(&mapped_rect, rect);
      mapped_rect.intersect(SkRect::Make(draw.fRC->getBounds()));
      AddBitmap(bitmap, mapped_rect);
    }
  }
  virtual void drawOval(const SkDraw& draw,
                        const SkRect& rect,
                        const SkPaint& paint) SK_OVERRIDE {
    GatherPixelRefDevice::drawRect(draw, rect, paint);
  }
  virtual void drawRRect(const SkDraw& draw,
                         const SkRRect& rect,
                         const SkPaint& paint) SK_OVERRIDE {
    GatherPixelRefDevice::drawRect(draw, rect.rect(), paint);
  }
  virtual void drawPath(const SkDraw& draw,
                        const SkPath& path,
                        const SkPaint& paint,
                        const SkMatrix* pre_path_matrix,
                        bool path_is_mutable) SK_OVERRIDE {
    SkBitmap bitmap;
    if (!GetBitmapFromPaint(paint, &bitmap))
      return;

    SkRect path_bounds = path.getBounds();
    SkRect final_rect;
    if (pre_path_matrix != NULL)
      pre_path_matrix->mapRect(&final_rect, path_bounds);
    else
      final_rect = path_bounds;

    GatherPixelRefDevice::drawRect(draw, final_rect, paint);
  }
  virtual void drawBitmap(const SkDraw& draw,
                          const SkBitmap& bitmap,
                          const SkMatrix& matrix,
                          const SkPaint& paint) SK_OVERRIDE {
    SkMatrix total_matrix;
    total_matrix.setConcat(*draw.fMatrix, matrix);

    SkRect bitmap_rect = SkRect::MakeWH(bitmap.width(), bitmap.height());
    SkRect mapped_rect;
    total_matrix.mapRect(&mapped_rect, bitmap_rect);
    AddBitmap(bitmap, mapped_rect);

    SkBitmap paint_bitmap;
    if (GetBitmapFromPaint(paint, &paint_bitmap))
      AddBitmap(paint_bitmap, mapped_rect);
  }
  virtual void drawBitmapRect(const SkDraw& draw,
                              const SkBitmap& bitmap,
                              const SkRect* src_or_null,
                              const SkRect& dst,
                              const SkPaint& paint,
                              SkCanvas::DrawBitmapRectFlags flags) SK_OVERRIDE {
    SkRect bitmap_rect = SkRect::MakeWH(bitmap.width(), bitmap.height());
    SkMatrix matrix;
    matrix.setRectToRect(bitmap_rect, dst, SkMatrix::kFill_ScaleToFit);
    GatherPixelRefDevice::drawBitmap(draw, bitmap, matrix, paint);
  }
  virtual void drawSprite(const SkDraw& draw,
                          const SkBitmap& bitmap,
                          int x,
                          int y,
                          const SkPaint& paint) SK_OVERRIDE {
    // Sprites aren't affected by current matrix, so we can't reuse drawRect.
    SkMatrix matrix;
    matrix.setTranslate(x, y);

    SkRect bitmap_rect = SkRect::MakeWH(bitmap.width(), bitmap.height());
    SkRect mapped_rect;
    matrix.mapRect(&mapped_rect, bitmap_rect);

    AddBitmap(bitmap, mapped_rect);
    SkBitmap paint_bitmap;
    if (GetBitmapFromPaint(paint, &paint_bitmap))
      AddBitmap(paint_bitmap, mapped_rect);
  }
  virtual void drawText(const SkDraw& draw,
                        const void* text,
                        size_t len,
                        SkScalar x,
                        SkScalar y,
                        const SkPaint& paint) SK_OVERRIDE {
    SkBitmap bitmap;
    if (!GetBitmapFromPaint(paint, &bitmap))
      return;

    // Math is borrowed from SkBBoxRecord
    SkRect bounds;
    paint.measureText(text, len, &bounds);
    SkPaint::FontMetrics metrics;
    paint.getFontMetrics(&metrics);

    if (paint.isVerticalText()) {
      SkScalar h = bounds.fBottom - bounds.fTop;
      if (paint.getTextAlign() == SkPaint::kCenter_Align) {
        bounds.fTop -= h / 2;
        bounds.fBottom -= h / 2;
      }
      bounds.fBottom += metrics.fBottom;
      bounds.fTop += metrics.fTop;
    } else {
      SkScalar w = bounds.fRight - bounds.fLeft;
      if (paint.getTextAlign() == SkPaint::kCenter_Align) {
        bounds.fLeft -= w / 2;
        bounds.fRight -= w / 2;
      } else if (paint.getTextAlign() == SkPaint::kRight_Align) {
        bounds.fLeft -= w;
        bounds.fRight -= w;
      }
      bounds.fTop = metrics.fTop;
      bounds.fBottom = metrics.fBottom;
    }

    SkScalar pad = (metrics.fBottom - metrics.fTop) / 2;
    bounds.fLeft -= pad;
    bounds.fRight += pad;
    bounds.fLeft += x;
    bounds.fRight += x;
    bounds.fTop += y;
    bounds.fBottom += y;

    GatherPixelRefDevice::drawRect(draw, bounds, paint);
  }
  virtual void drawPosText(const SkDraw& draw,
                           const void* text,
                           size_t len,
                           const SkScalar pos[],
                           SkScalar const_y,
                           int scalars_per_pos,
                           const SkPaint& paint) SK_OVERRIDE {
    SkBitmap bitmap;
    if (!GetBitmapFromPaint(paint, &bitmap))
      return;

    if (len == 0)
      return;

    // Similar to SkDraw asserts.
    SkASSERT(scalars_per_pos == 1 || scalars_per_pos == 2);

    SkPoint min_point;
    SkPoint max_point;
    if (scalars_per_pos == 1) {
      min_point.set(pos[0], const_y);
      max_point.set(pos[0], const_y);
    } else if (scalars_per_pos == 2) {
      min_point.set(pos[0], const_y + pos[1]);
      max_point.set(pos[0], const_y + pos[1]);
    }

    for (size_t i = 0; i < len; ++i) {
      SkScalar x = pos[i * scalars_per_pos];
      SkScalar y = const_y;
      if (scalars_per_pos == 2)
        y += pos[i * scalars_per_pos + 1];

      min_point.set(std::min(x, min_point.x()), std::min(y, min_point.y()));
      max_point.set(std::max(x, max_point.x()), std::max(y, max_point.y()));
    }

    SkRect bounds = SkRect::MakeLTRB(
        min_point.x(), min_point.y(), max_point.x(), max_point.y());

    // Math is borrowed from SkBBoxRecord
    SkPaint::FontMetrics metrics;
    paint.getFontMetrics(&metrics);

    bounds.fTop += metrics.fTop;
    bounds.fBottom += metrics.fBottom;

    SkScalar pad = (metrics.fTop - metrics.fBottom) / 2;
    bounds.fLeft += pad;
    bounds.fRight -= pad;

    GatherPixelRefDevice::drawRect(draw, bounds, paint);
  }
  virtual void drawTextOnPath(const SkDraw& draw,
                              const void* text,
                              size_t len,
                              const SkPath& path,
                              const SkMatrix* matrix,
                              const SkPaint& paint) SK_OVERRIDE {
    SkBitmap bitmap;
    if (!GetBitmapFromPaint(paint, &bitmap))
      return;

    // Math is borrowed from SkBBoxRecord
    SkRect bounds = path.getBounds();
    SkPaint::FontMetrics metrics;
    paint.getFontMetrics(&metrics);

    SkScalar pad = metrics.fTop;
    bounds.fLeft += pad;
    bounds.fRight -= pad;
    bounds.fTop += pad;
    bounds.fBottom -= pad;

    GatherPixelRefDevice::drawRect(draw, bounds, paint);
  }
  virtual void drawVertices(const SkDraw& draw,
                            SkCanvas::VertexMode,
                            int vertex_count,
                            const SkPoint verts[],
                            const SkPoint texs[],
                            const SkColor colors[],
                            SkXfermode* xmode,
                            const uint16_t indices[],
                            int index_count,
                            const SkPaint& paint) SK_OVERRIDE {
    GatherPixelRefDevice::drawPoints(
        draw, SkCanvas::kPolygon_PointMode, vertex_count, verts, paint);
  }
  virtual void drawDevice(const SkDraw&,
                          SkBaseDevice*,
                          int x,
                          int y,
                          const SkPaint&) SK_OVERRIDE {}

 protected:
  virtual bool onReadPixels(const SkBitmap& bitmap,
                            int x,
                            int y,
                            SkCanvas::Config8888 config8888) SK_OVERRIDE {
    return false;
  }

 private:
  LazyPixelRefSet* lazy_pixel_ref_set_;

  void AddBitmap(const SkBitmap& bm, const SkRect& rect) {
    SkRect canvas_rect = SkRect::MakeWH(width(), height());
    SkRect paint_rect = SkRect::MakeEmpty();
    paint_rect.intersect(rect, canvas_rect);
    lazy_pixel_ref_set_->Add(bm.pixelRef(), paint_rect);
  }

  bool GetBitmapFromPaint(const SkPaint& paint, SkBitmap* bm) {
    SkShader* shader = paint.getShader();
    if (shader) {
      // Check whether the shader is a gradient in order to prevent generation
      // of bitmaps from gradient shaders, which implement asABitmap.
      if (SkShader::kNone_GradientType == shader->asAGradient(NULL))
        return shader->asABitmap(bm, NULL, NULL);
    }
    return false;
  }
};

class NoSaveLayerCanvas : public SkCanvas {
 public:
  NoSaveLayerCanvas(SkBaseDevice* device) : INHERITED(device) {}

  // Turn saveLayer() into save() for speed, should not affect correctness.
  virtual int saveLayer(const SkRect* bounds,
                        const SkPaint* paint,
                        SaveFlags flags) SK_OVERRIDE {

    // Like SkPictureRecord, we don't want to create layers, but we do need
    // to respect the save and (possibly) its rect-clip.
    int count = this->INHERITED::save(flags);
    if (bounds) {
      this->INHERITED::clipRectBounds(bounds, flags, NULL);
    }
    return count;
  }

  // Disable aa for speed.
  virtual bool clipRect(const SkRect& rect, SkRegion::Op op, bool doAA)
      SK_OVERRIDE {
    return this->INHERITED::clipRect(rect, op, false);
  }

  virtual bool clipPath(const SkPath& path, SkRegion::Op op, bool doAA)
      SK_OVERRIDE {
    return this->updateClipConservativelyUsingBounds(
        path.getBounds(), op, path.isInverseFillType());
  }
  virtual bool clipRRect(const SkRRect& rrect, SkRegion::Op op, bool doAA)
      SK_OVERRIDE {
    return this->updateClipConservativelyUsingBounds(
        rrect.getBounds(), op, false);
  }

 private:
  typedef SkCanvas INHERITED;
};

}  // namespace

void LazyPixelRefUtils::GatherPixelRefs(
    SkPicture* picture,
    std::vector<PositionLazyPixelRef>* lazy_pixel_refs) {
  lazy_pixel_refs->clear();
  LazyPixelRefSet pixel_ref_set(lazy_pixel_refs);

  SkBitmap empty_bitmap;
  empty_bitmap.setConfig(
      SkBitmap::kNo_Config, picture->width(), picture->height());

  GatherPixelRefDevice device(empty_bitmap, &pixel_ref_set);
  NoSaveLayerCanvas canvas(&device);

  canvas.clipRect(SkRect::MakeWH(picture->width(), picture->height()),
                  SkRegion::kIntersect_Op,
                  false);
  canvas.drawPicture(*picture);
}

}  // namespace skia
