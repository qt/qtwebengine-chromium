// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

// Whether the element is inside an iframe.
bool IsInIFrame(const HTMLAnchorElement& anchor_element) {
  Frame* frame = anchor_element.GetDocument().GetFrame();
  return frame && !frame->IsMainFrame();
}

// Whether the anchor element contains an image element.
bool ContainsImage(const HTMLAnchorElement& anchor_element) {
  for (Node* node = FlatTreeTraversal::FirstChild(anchor_element); node;
       node = FlatTreeTraversal::Next(*node, &anchor_element)) {
    if (IsA<HTMLImageElement>(*node))
      return true;
  }
  return false;
}

// Whether the link target has the same host as the root document.
bool IsSameHost(const HTMLAnchorElement& anchor_element) {
  Document* top_document = GetTopDocument(anchor_element);
  if (!top_document) {
    return false;
  }
  String source_host = top_document->Url().Host();
  String target_host = anchor_element.Href().Host();
  return source_host == target_host;
}

// Returns true if the two strings only differ by one number, and
// the second number equals the first number plus one. Examples:
// example.com/page9/cat5, example.com/page10/cat5 => true
// example.com/page9/cat5, example.com/page10/cat10 => false
bool IsStringIncrementedByOne(const String& source, const String& target) {
  // Consecutive numbers should differ in length by at most 1.
  int length_diff = target.length() - source.length();
  if (length_diff < 0 || length_diff > 1)
    return false;

  // The starting position of difference.
  unsigned int left = 0;
  while (left < source.length() && left < target.length() &&
         source[left] == target[left]) {
    left++;
  }

  // There is no difference, or the difference is not a digit.
  if (left == source.length() || left == target.length() ||
      !u_isdigit(source[left]) || !u_isdigit(target[left])) {
    return false;
  }

  // Expand towards right to extract the numbers.
  unsigned int source_right = left + 1;
  while (source_right < source.length() && u_isdigit(source[source_right]))
    source_right++;

  unsigned int target_right = left + 1;
  while (target_right < target.length() && u_isdigit(target[target_right]))
    target_right++;

  int source_number = source.Substring(left, source_right - left).ToInt();
  int target_number = target.Substring(left, target_right - left).ToInt();

  // The second number should increment by one and the rest of the strings
  // should be the same.
  return source_number + 1 == target_number &&
         source.Substring(source_right) == target.Substring(target_right);
}

// Extract source and target link url, and return IsStringIncrementedByOne().
bool IsUrlIncrementedByOne(const HTMLAnchorElement& anchor_element) {
  if (!IsSameHost(anchor_element)) {
    return false;
  }

  Document* top_document = GetTopDocument(anchor_element);
  if (!top_document) {
    return false;
  }

  String source_url = top_document->Url().GetString();
  String target_url = anchor_element.Href().GetString();
  return IsStringIncrementedByOne(source_url, target_url);
}

// Returns the bounding box rect of a layout object, including visual
// overflows.
gfx::Rect AbsoluteElementBoundingBoxRect(const LayoutObject& layout_object) {
  Vector<PhysicalRect> rects = layout_object.OutlineRects(
      nullptr, PhysicalOffset(), OutlineType::kIncludeBlockInkOverflow);
  return ToEnclosingRect(layout_object.LocalToAbsoluteRect(UnionRect(rects)));
}

bool IsNonEmptyTextNode(Node* node) {
  if (!node) {
    return false;
  }
  if (!node->IsTextNode()) {
    return false;
  }
  return !To<Text>(node)->wholeText().ContainsOnlyWhitespaceOrEmpty();
}

}  // anonymous namespace

Document* GetTopDocument(const HTMLAnchorElement& anchor) {
  LocalFrame* frame = anchor.GetDocument().GetFrame();
  if (!frame) {
    return nullptr;
  }

  LocalFrame* local_main_frame = DynamicTo<LocalFrame>(frame->Tree().Top());
  if (!local_main_frame) {
    return nullptr;
  }

  return local_main_frame->GetDocument();
}

// Computes a unique ID for the anchor. We hash the pointer address of the
// object. Note that this implementation can lead to collisions if an element is
// destroyed and a new one is created with the same address. We don't mind this
// issue as the anchor ID is only used for metric collection.
uint32_t AnchorElementId(const HTMLAnchorElement& element) {
  return WTF::GetHash(&element);
}

mojom::blink::AnchorElementMetricsPtr CreateAnchorElementMetrics(
    const HTMLAnchorElement& anchor_element) {
  LocalFrame* local_frame = anchor_element.GetDocument().GetFrame();
  if (!local_frame) {
    return nullptr;
  }

  mojom::blink::AnchorElementMetricsPtr metrics =
      mojom::blink::AnchorElementMetrics::New();
  metrics->anchor_id = AnchorElementId(anchor_element);
  metrics->is_in_iframe = IsInIFrame(anchor_element);
  metrics->contains_image = ContainsImage(anchor_element);
  metrics->is_same_host = IsSameHost(anchor_element);
  metrics->is_url_incremented_by_one = IsUrlIncrementedByOne(anchor_element);
  metrics->target_url = anchor_element.Href();

  metrics->has_text_sibling =
      IsNonEmptyTextNode(anchor_element.nextSibling()) ||
      IsNonEmptyTextNode(anchor_element.previousSibling());

  const ComputedStyle& computed_style = anchor_element.ComputedStyleRef();
  metrics->font_weight =
      static_cast<uint32_t>(computed_style.GetFontWeight() + 0.5f);
  metrics->font_size_px = computed_style.FontSize();

  // Don't record size metrics for subframe document Anchors.
  if (metrics->is_in_iframe) {
    return metrics;
  }

  LayoutObject* layout_object = anchor_element.GetLayoutObject();
  if (!layout_object) {
    return metrics;
  }

  DCHECK(local_frame->IsLocalRoot());
  LocalFrameView* root_frame_view = local_frame->View();
  if (!root_frame_view) {
    return metrics;
  }
  DCHECK(!root_frame_view->ParentFrameView());

  gfx::Rect viewport = root_frame_view->LayoutViewport()->VisibleContentRect();
  if (viewport.IsEmpty()) {
    return metrics;
  }
  metrics->viewport_size = viewport.size();

  // Use the viewport size to normalize anchor element metrics.
  float base_height = static_cast<float>(viewport.height());
  float base_width = static_cast<float>(viewport.width());

  gfx::Rect target = AbsoluteElementBoundingBoxRect(*layout_object);

  // Limit the element size to the viewport size.
  float ratio_area = std::min(1.0f, target.height() / base_height) *
                     std::min(1.0f, target.width() / base_width);
  DCHECK_GE(1.0, ratio_area);
  metrics->ratio_area = ratio_area;

  float ratio_distance_top_to_visible_top = target.y() / base_height;
  metrics->ratio_distance_top_to_visible_top =
      ratio_distance_top_to_visible_top;

  float ratio_distance_root_top =
      (target.y() + root_frame_view->LayoutViewport()->ScrollOffsetInt().y()) /
      base_height;
  metrics->ratio_distance_root_top = ratio_distance_root_top;

  return metrics;
}

}  // namespace blink
