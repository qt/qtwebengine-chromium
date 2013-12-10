// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling_set.h"

#include <limits>

namespace cc {

namespace {

class LargestToSmallestScaleFunctor {
 public:
  bool operator() (PictureLayerTiling* left, PictureLayerTiling* right) {
    return left->contents_scale() > right->contents_scale();
  }
};

}  // namespace


PictureLayerTilingSet::PictureLayerTilingSet(
    PictureLayerTilingClient* client,
    gfx::Size layer_bounds)
    : client_(client),
      layer_bounds_(layer_bounds) {
}

PictureLayerTilingSet::~PictureLayerTilingSet() {
}

void PictureLayerTilingSet::SetClient(PictureLayerTilingClient* client) {
  client_ = client;
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->SetClient(client_);
}

void PictureLayerTilingSet::SyncTilings(
    const PictureLayerTilingSet& other,
    gfx::Size new_layer_bounds,
    const Region& layer_invalidation,
    float minimum_contents_scale) {
  if (new_layer_bounds.IsEmpty()) {
    RemoveAllTilings();
    layer_bounds_ = new_layer_bounds;
    return;
  }

  tilings_.reserve(other.tilings_.size());

  // Remove any tilings that aren't in |other| or don't meet the minimum.
  for (size_t i = 0; i < tilings_.size(); ++i) {
    float scale = tilings_[i]->contents_scale();
    if (scale >= minimum_contents_scale && !!other.TilingAtScale(scale))
      continue;
    // Swap with the last element and remove it.
    tilings_.swap(tilings_.begin() + i, tilings_.end() - 1);
    tilings_.pop_back();
    --i;
  }

  // Add any missing tilings from |other| that meet the minimum.
  for (size_t i = 0; i < other.tilings_.size(); ++i) {
    float contents_scale = other.tilings_[i]->contents_scale();
    if (contents_scale < minimum_contents_scale)
      continue;
    if (PictureLayerTiling* this_tiling = TilingAtScale(contents_scale)) {
      this_tiling->set_resolution(other.tilings_[i]->resolution());

      // These two calls must come before updating the pile, because they may
      // destroy tiles that the new pile cannot raster.
      this_tiling->SetLayerBounds(new_layer_bounds);
      this_tiling->Invalidate(layer_invalidation);

      this_tiling->UpdateTilesToCurrentPile();
      this_tiling->CreateMissingTilesInLiveTilesRect();

      DCHECK(this_tiling->tile_size() ==
             client_->CalculateTileSize(this_tiling->ContentRect().size()));
      continue;
    }
    scoped_ptr<PictureLayerTiling> new_tiling = PictureLayerTiling::Create(
        contents_scale,
        new_layer_bounds,
        client_);
    new_tiling->set_resolution(other.tilings_[i]->resolution());
    tilings_.push_back(new_tiling.Pass());
  }
  tilings_.sort(LargestToSmallestScaleFunctor());

  layer_bounds_ = new_layer_bounds;
}

void PictureLayerTilingSet::SetCanUseLCDText(bool can_use_lcd_text) {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->SetCanUseLCDText(can_use_lcd_text);
}

PictureLayerTiling* PictureLayerTilingSet::AddTiling(float contents_scale) {
  for (size_t i = 0; i < tilings_.size(); ++i)
    DCHECK_NE(tilings_[i]->contents_scale(), contents_scale);

  tilings_.push_back(PictureLayerTiling::Create(contents_scale,
                                                layer_bounds_,
                                                client_));
  PictureLayerTiling* appended = tilings_.back();

  tilings_.sort(LargestToSmallestScaleFunctor());
  return appended;
}

int PictureLayerTilingSet::NumHighResTilings() const {
  int num_high_res = 0;
  for (size_t i = 0; i < tilings_.size(); ++i) {
    if (tilings_[i]->resolution() == HIGH_RESOLUTION)
      num_high_res++;
  }
  return num_high_res;
}

PictureLayerTiling* PictureLayerTilingSet::TilingAtScale(float scale) const {
  for (size_t i = 0; i < tilings_.size(); ++i) {
    if (tilings_[i]->contents_scale() == scale)
      return tilings_[i];
  }
  return NULL;
}

void PictureLayerTilingSet::RemoveAllTilings() {
  tilings_.clear();
}

void PictureLayerTilingSet::Remove(PictureLayerTiling* tiling) {
  ScopedPtrVector<PictureLayerTiling>::iterator iter =
    std::find(tilings_.begin(), tilings_.end(), tiling);
  if (iter == tilings_.end())
    return;
  tilings_.erase(iter);
}

void PictureLayerTilingSet::RemoveAllTiles() {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->Reset();
}

PictureLayerTilingSet::CoverageIterator::CoverageIterator(
    const PictureLayerTilingSet* set,
    float contents_scale,
    gfx::Rect content_rect,
    float ideal_contents_scale)
    : set_(set),
      contents_scale_(contents_scale),
      ideal_contents_scale_(ideal_contents_scale),
      current_tiling_(-1) {
  missing_region_.Union(content_rect);

  for (ideal_tiling_ = 0;
       static_cast<size_t>(ideal_tiling_) < set_->tilings_.size();
       ++ideal_tiling_) {
    PictureLayerTiling* tiling = set_->tilings_[ideal_tiling_];
    if (tiling->contents_scale() < ideal_contents_scale_) {
      if (ideal_tiling_ > 0)
        ideal_tiling_--;
      break;
    }
  }

  DCHECK_LE(set_->tilings_.size(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  int num_tilings = set_->tilings_.size();
  if (ideal_tiling_ == num_tilings && ideal_tiling_ > 0)
    ideal_tiling_--;

  ++(*this);
}

PictureLayerTilingSet::CoverageIterator::~CoverageIterator() {
}

gfx::Rect PictureLayerTilingSet::CoverageIterator::geometry_rect() const {
  if (!tiling_iter_) {
    if (!region_iter_.has_rect())
      return gfx::Rect();
    return region_iter_.rect();
  }
  return tiling_iter_.geometry_rect();
}

gfx::RectF PictureLayerTilingSet::CoverageIterator::texture_rect() const {
  if (!tiling_iter_)
    return gfx::RectF();
  return tiling_iter_.texture_rect();
}

gfx::Size PictureLayerTilingSet::CoverageIterator::texture_size() const {
  if (!tiling_iter_)
    return gfx::Size();
  return tiling_iter_.texture_size();
}

Tile* PictureLayerTilingSet::CoverageIterator::operator->() const {
  if (!tiling_iter_)
    return NULL;
  return *tiling_iter_;
}

Tile* PictureLayerTilingSet::CoverageIterator::operator*() const {
  if (!tiling_iter_)
    return NULL;
  return *tiling_iter_;
}

PictureLayerTiling* PictureLayerTilingSet::CoverageIterator::CurrentTiling() {
  if (current_tiling_ < 0)
    return NULL;
  if (static_cast<size_t>(current_tiling_) >= set_->tilings_.size())
    return NULL;
  return set_->tilings_[current_tiling_];
}

int PictureLayerTilingSet::CoverageIterator::NextTiling() const {
  // Order returned by this method is:
  // 1. Ideal tiling index
  // 2. Tiling index < Ideal in decreasing order (higher res than ideal)
  // 3. Tiling index > Ideal in increasing order (lower res than ideal)
  // 4. Tiling index > tilings.size() (invalid index)
  if (current_tiling_ < 0)
    return ideal_tiling_;
  else if (current_tiling_ > ideal_tiling_)
    return current_tiling_ + 1;
  else if (current_tiling_)
    return current_tiling_ - 1;
  else
    return ideal_tiling_ + 1;
}

PictureLayerTilingSet::CoverageIterator&
PictureLayerTilingSet::CoverageIterator::operator++() {
  bool first_time = current_tiling_ < 0;

  if (!*this && !first_time)
    return *this;

  if (tiling_iter_)
    ++tiling_iter_;

  // Loop until we find a valid place to stop.
  while (true) {
    while (tiling_iter_ &&
           (!*tiling_iter_ || !tiling_iter_->IsReadyToDraw())) {
      missing_region_.Union(tiling_iter_.geometry_rect());
      ++tiling_iter_;
    }
    if (tiling_iter_)
      return *this;

    // If the set of current rects for this tiling is done, go to the next
    // tiling and set up to iterate through all of the remaining holes.
    // This will also happen the first time through the loop.
    if (!region_iter_.has_rect()) {
      current_tiling_ = NextTiling();
      current_region_.Swap(&missing_region_);
      missing_region_.Clear();
      region_iter_ = Region::Iterator(current_region_);

      // All done and all filled.
      if (!region_iter_.has_rect()) {
        current_tiling_ = set_->tilings_.size();
        return *this;
      }

      // No more valid tiles, return this checkerboard rect.
      if (current_tiling_ >= static_cast<int>(set_->tilings_.size()))
        return *this;
    }

    // Pop a rect off.  If there are no more tilings, then these will be
    // treated as geometry with null tiles that the caller can checkerboard.
    gfx::Rect last_rect = region_iter_.rect();
    region_iter_.next();

    // Done, found next checkerboard rect to return.
    if (current_tiling_ >= static_cast<int>(set_->tilings_.size()))
      return *this;

    // Construct a new iterator for the next tiling, but we need to loop
    // again until we get to a valid one.
    tiling_iter_ = PictureLayerTiling::CoverageIterator(
        set_->tilings_[current_tiling_],
        contents_scale_,
        last_rect);
  }

  return *this;
}

PictureLayerTilingSet::CoverageIterator::operator bool() const {
  return current_tiling_ < static_cast<int>(set_->tilings_.size()) ||
      region_iter_.has_rect();
}

void PictureLayerTilingSet::UpdateTilePriorities(
    WhichTree tree,
    gfx::Size device_viewport,
    gfx::Rect viewport_in_content_space,
    gfx::Rect visible_content_rect,
    gfx::Size last_layer_bounds,
    gfx::Size current_layer_bounds,
    float last_layer_contents_scale,
    float current_layer_contents_scale,
    const gfx::Transform& last_screen_transform,
    const gfx::Transform& current_screen_transform,
    double current_frame_time_in_seconds,
    size_t max_tiles_for_interest_area) {
  gfx::Rect viewport_in_layer_space = gfx::ScaleToEnclosingRect(
      viewport_in_content_space,
      1.f / current_layer_contents_scale);
  gfx::Rect visible_layer_rect = gfx::ScaleToEnclosingRect(
      visible_content_rect,
      1.f / current_layer_contents_scale);

  for (size_t i = 0; i < tilings_.size(); ++i) {
    tilings_[i]->UpdateTilePriorities(
        tree,
        device_viewport,
        viewport_in_layer_space,
        visible_layer_rect,
        last_layer_bounds,
        current_layer_bounds,
        last_layer_contents_scale,
        current_layer_contents_scale,
        last_screen_transform,
        current_screen_transform,
        current_frame_time_in_seconds,
        max_tiles_for_interest_area);
  }
}

void PictureLayerTilingSet::DidBecomeActive() {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->DidBecomeActive();
}

scoped_ptr<base::Value> PictureLayerTilingSet::AsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  for (size_t i = 0; i < tilings_.size(); ++i)
    state->Append(tilings_[i]->AsValue().release());
  return state.PassAs<base::Value>();
}

size_t PictureLayerTilingSet::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (size_t i = 0; i < tilings_.size(); ++i)
    amount += tilings_[i]->GPUMemoryUsageInBytes();
  return amount;
}

}  // namespace cc
