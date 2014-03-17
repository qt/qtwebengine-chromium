// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_IMPL_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_IMPL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/scoped_resource.h"

class SkCanvas;
class SkPaint;
class SkTypeface;
struct SkRect;

namespace cc {

class DebugRectHistory;
class FrameRateCounter;
class PaintTimeCounter;

class CC_EXPORT HeadsUpDisplayLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<HeadsUpDisplayLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                    int id) {
    return make_scoped_ptr(new HeadsUpDisplayLayerImpl(tree_impl, id));
  }
  virtual ~HeadsUpDisplayLayerImpl();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  void UpdateHudTexture(DrawMode draw_mode,
                        ResourceProvider* resource_provider);

  virtual void DidLoseOutputSurface() OVERRIDE;

  virtual bool LayerIsAlwaysDamaged() const OVERRIDE;

 private:
  class Graph {
   public:
    Graph(double indicator_value, double start_upper_bound);

    // Eases the upper bound, which limits what is currently visible in the
    // graph, so that the graph always scales to either it's max or
    // default_upper_bound.
    double UpdateUpperBound();

    double value;
    double min;
    double max;

    double current_upper_bound;
    const double default_upper_bound;
    const double indicator;
  };

  HeadsUpDisplayLayerImpl(LayerTreeImpl* tree_impl, int id);

  virtual const char* LayerTypeAsString() const OVERRIDE;

  void UpdateHudContents();
  void DrawHudContents(SkCanvas* canvas) const;

  void DrawText(SkCanvas* canvas,
                SkPaint* paint,
                const std::string& text,
                SkPaint::Align align,
                int size,
                int x,
                int y) const;
  void DrawText(SkCanvas* canvas,
                SkPaint* paint,
                const std::string& text,
                SkPaint::Align align,
                int size,
                const SkPoint& pos) const;
  void DrawGraphBackground(SkCanvas* canvas,
                           SkPaint* paint,
                           const SkRect& bounds) const;
  void DrawGraphLines(SkCanvas* canvas,
                      SkPaint* paint,
                      const SkRect& bounds,
                      const Graph& graph) const;

  SkRect DrawFPSDisplay(SkCanvas* canvas,
                        const FrameRateCounter* fps_counter,
                        int right,
                        int top) const;
  SkRect DrawMemoryDisplay(SkCanvas* canvas,
                           int top,
                           int right,
                           int width) const;
  SkRect DrawPaintTimeDisplay(SkCanvas* canvas,
                              const PaintTimeCounter* paint_time_counter,
                              int top,
                              int right) const;
  void DrawDebugRects(SkCanvas* canvas,
                      DebugRectHistory* debug_rect_history) const;

  scoped_ptr<ScopedResource> hud_resource_;
  scoped_ptr<SkCanvas> hud_canvas_;

  skia::RefPtr<SkTypeface> typeface_;

  Graph fps_graph_;
  Graph paint_time_graph_;
  MemoryHistory::Entry memory_entry_;

  base::TimeTicks time_of_last_graph_update_;

  DISALLOW_COPY_AND_ASSIGN(HeadsUpDisplayLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_IMPL_H_
