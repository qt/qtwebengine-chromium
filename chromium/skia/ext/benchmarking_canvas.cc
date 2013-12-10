// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "skia/ext/benchmarking_canvas.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/utils/SkProxyCanvas.h"

namespace skia {

class AutoStamper {
public:
  AutoStamper(TimingCanvas* timing_canvas);
  ~AutoStamper();

private:
  TimingCanvas* timing_canvas_;
  base::TimeTicks start_ticks_;
};

class TimingCanvas : public SkProxyCanvas {
public:
  TimingCanvas(int width, int height, const BenchmarkingCanvas* track_canvas)
      : tracking_canvas_(track_canvas) {
    skia::RefPtr<SkBaseDevice> device = skia::AdoptRef(
        SkNEW_ARGS(SkBitmapDevice, (SkBitmap::kARGB_8888_Config, width, height)));
    canvas_ = skia::AdoptRef(SkNEW_ARGS(SkCanvas, (device.get())));

    setProxy(canvas_.get());
  }

  virtual ~TimingCanvas() {
  }

  double GetTime(size_t index) {
    TimingsMap::const_iterator timing_info = timings_map_.find(index);
    return timing_info != timings_map_.end()
        ? timing_info->second.InMillisecondsF()
        : 0.0;
  }

  // SkCanvas overrides.
  virtual int save(SaveFlags flags = kMatrixClip_SaveFlag) OVERRIDE {
    AutoStamper stamper(this);
    return SkProxyCanvas::save(flags);
  }

  virtual int saveLayer(const SkRect* bounds, const SkPaint* paint,
                        SaveFlags flags = kARGB_ClipLayer_SaveFlag) OVERRIDE {
    AutoStamper stamper(this);
    return SkProxyCanvas::saveLayer(bounds, paint, flags);
  }

  virtual void restore() OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::restore();
  }

  virtual bool clipRect(const SkRect& rect, SkRegion::Op op,
                        bool doAa) OVERRIDE {
    AutoStamper stamper(this);
    return SkProxyCanvas::clipRect(rect, op, doAa);
  }

  virtual bool clipRRect(const SkRRect& rrect, SkRegion::Op op,
                         bool doAa) OVERRIDE {
    AutoStamper stamper(this);
    return SkProxyCanvas::clipRRect(rrect, op, doAa);
  }

  virtual bool clipPath(const SkPath& path, SkRegion::Op op,
                        bool doAa) OVERRIDE {
    AutoStamper stamper(this);
    return SkProxyCanvas::clipPath(path, op, doAa);
  }

  virtual bool clipRegion(const SkRegion& region,
                          SkRegion::Op op = SkRegion::kIntersect_Op) OVERRIDE {
    AutoStamper stamper(this);
    return SkProxyCanvas::clipRegion(region, op);
  }

  virtual void drawPaint(const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawPaint(paint);
  }

  virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
                          const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawPoints(mode, count, pts, paint);
  }

  virtual void drawOval(const SkRect& rect, const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawOval(rect, paint);
  }

  virtual void drawRect(const SkRect& rect, const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawRect(rect, paint);
  }

  virtual void drawRRect(const SkRRect& rrect, const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawRRect(rrect, paint);
  }

  virtual void drawPath(const SkPath& path, const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawPath(path, paint);
  }

  virtual void drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top,
                          const SkPaint* paint = NULL) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawBitmap(bitmap, left, top, paint);
  }

  virtual void drawBitmapRectToRect(const SkBitmap& bitmap, const SkRect* src,
                                    const SkRect& dst,
                                    const SkPaint* paint,
                                    DrawBitmapRectFlags flags) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawBitmapRectToRect(bitmap, src, dst, paint, flags);
  }

  virtual void drawBitmapMatrix(const SkBitmap& bitmap, const SkMatrix& m,
                                const SkPaint* paint = NULL) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawBitmapMatrix(bitmap, m, paint);
  }

  virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
                          const SkPaint* paint = NULL) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawSprite(bitmap, left, top, paint);
  }

  virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                        SkScalar y, const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawText(text, byteLength, x, y, paint);
  }

  virtual void drawPosText(const void* text, size_t byteLength,
                           const SkPoint pos[],
                           const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawPosText(text, byteLength, pos, paint);
  }

  virtual void drawPosTextH(const void* text, size_t byteLength,
                            const SkScalar xpos[], SkScalar constY,
                            const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawPosTextH(text, byteLength, xpos, constY, paint);
  }

  virtual void drawTextOnPath(const void* text, size_t byteLength,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawTextOnPath(text, byteLength, path, matrix, paint);
  }

  virtual void drawPicture(SkPicture& picture) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawPicture(picture);
  }

  virtual void drawVertices(VertexMode vmode, int vertexCount,
                            const SkPoint vertices[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawVertices(vmode, vertexCount, vertices, texs, colors,
                                xmode, indices, indexCount, paint);
  }

  virtual void drawData(const void* data, size_t length) OVERRIDE {
    AutoStamper stamper(this);
    SkProxyCanvas::drawData(data, length);
  }

private:
  typedef base::hash_map<size_t, base::TimeDelta> TimingsMap;
  TimingsMap timings_map_;

  skia::RefPtr<SkCanvas> canvas_;

  friend class AutoStamper;
  const BenchmarkingCanvas* tracking_canvas_;
};

AutoStamper::AutoStamper(TimingCanvas *timing_canvas)
    : timing_canvas_(timing_canvas) {
  start_ticks_ = base::TimeTicks::HighResNow();
}

AutoStamper::~AutoStamper() {
  base::TimeDelta delta = base::TimeTicks::HighResNow() - start_ticks_;
  int command_index = timing_canvas_->tracking_canvas_->CommandCount() - 1;
  DCHECK_GE(command_index, 0);
  timing_canvas_->timings_map_[command_index] = delta;
}

BenchmarkingCanvas::BenchmarkingCanvas(int width, int height)
    : SkNWayCanvas(width, height) {
  debug_canvas_ = skia::AdoptRef(SkNEW_ARGS(SkDebugCanvas, (width, height)));
  timing_canvas_ = skia::AdoptRef(SkNEW_ARGS(TimingCanvas, (width, height, this)));

  addCanvas(debug_canvas_.get());
  addCanvas(timing_canvas_.get());
}

BenchmarkingCanvas::~BenchmarkingCanvas() {
  removeAll();
}

size_t BenchmarkingCanvas::CommandCount() const {
  return debug_canvas_->getSize();
}

SkDrawCommand* BenchmarkingCanvas::GetCommand(size_t index) {
  DCHECK_LT(index, static_cast<size_t>(debug_canvas_->getSize()));
  return debug_canvas_->getDrawCommandAt(index);
}

double BenchmarkingCanvas::GetTime(size_t index) {
  DCHECK_LT(index,  static_cast<size_t>(debug_canvas_->getSize()));
  return timing_canvas_->GetTime(index);
}

} // namespace skia
