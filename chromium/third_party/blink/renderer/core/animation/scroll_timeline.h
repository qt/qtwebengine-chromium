// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "cc/animation/scroll_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_axis.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_attachment.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class Element;
class PaintLayerScrollableArea;
class ScrollTimelineOptions;
class ScrollTimelineAttachment;

// Implements the ScrollTimeline concept from the Scroll-linked Animations spec.
//
// A ScrollTimeline is a special form of AnimationTimeline whose time values are
// not determined by wall-clock time but instead the progress of scrolling in a
// scroll container. The user is able to specify which scroll container to
// track, the direction of scroll they care about, and various attributes to
// control the conversion of scroll amount to time output.
//
// Spec: https://wicg.github.io/scroll-animations/#scroll-timelines
class CORE_EXPORT ScrollTimeline : public ScrollSnapshotTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using ReferenceType = ScrollTimelineAttachment::ReferenceType;

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions*,
                                ExceptionState&);

  static ScrollTimeline* Create(Document* document,
                                Element* source,
                                ScrollAxis axis);

  // Construct ScrollTimeline objects through one of the Create methods, which
  // perform initial snapshots, as it can't be done during the constructor due
  // to possibly depending on overloaded functions.
  ScrollTimeline(Document*,
                 ReferenceType reference_type,
                 Element* reference,
                 ScrollAxis axis);

  bool IsScrollTimeline() const override { return true; }

  // IDL API implementation.
  Element* source() const;
  const V8ScrollAxis axis() const { return V8ScrollAxis(GetAxis()); }

  bool Matches(ReferenceType, Element* reference_element, ScrollAxis) const;

  ScrollAxis GetAxis() const override;

  void AnimationAttached(Animation*) override;
  void AnimationDetached(Animation*) override;

  void Trace(Visitor*) const override;

  // Duration is the maximum value a timeline may generate for current time.
  // Used to convert time values to proportional values.
  absl::optional<AnimationTimeDelta> GetDuration() const override {
    // Any arbitrary value should be able to be used here.
    return absl::make_optional(ANIMATION_TIME_DELTA_FROM_SECONDS(100));
  }

  ScrollTimelineAttachment* CurrentAttachment() {
    return (attachments_.size() == 1u) ? attachments_.back().Get() : nullptr;
  }

  const ScrollTimelineAttachment* CurrentAttachment() const {
    return const_cast<ScrollTimeline*>(this)->CurrentAttachment();
  }

 protected:
  ScrollTimeline(Document*, ScrollTimelineAttachment*);

  Node* ComputeResolvedSource() const;

  // Scroll offsets corresponding to 0% and 100% progress. By default, these
  // correspond to the scroll range of the container.
  virtual void CalculateOffsets(PaintLayerScrollableArea* scrollable_area,
                                ScrollOrientation physical_orientation,
                                TimelineState* state) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScrollTimelineTest, MultipleScrollOffsetsClamping);
  FRIEND_TEST_ALL_PREFIXES(ScrollTimelineTest, ResolveScrollOffsets);

  // The retaining element is the element responsible for keeping
  // the timeline alive while animations are attached.
  //
  // See Node::[Un]RegisterScrollTimeline.
  Element* RetainingElement() const;

  TimelineState ComputeTimelineState() const override;

  HeapVector<Member<ScrollTimelineAttachment>, 1> attachments_;
};

template <>
struct DowncastTraits<ScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsScrollTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
