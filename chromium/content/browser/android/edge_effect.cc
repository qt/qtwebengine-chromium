// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/edge_effect.h"

#include "cc/layers/layer.h"
#include "ui/gfx/screen.h"

namespace content {

namespace {

enum State {
  STATE_IDLE = 0,
  STATE_PULL,
  STATE_ABSORB,
  STATE_RECEDE,
  STATE_PULL_DECAY
};

// Time it will take the effect to fully recede in ms
const int kRecedeTime = 1000;

// Time it will take before a pulled glow begins receding in ms
const int kPullTime = 167;

// Time it will take in ms for a pulled glow to decay before release
const int kPullDecayTime = 1000;

const float kMaxAlpha = 1.f;
const float kHeldEdgeScaleY = .5f;

const float kMaxGlowHeight = 4.f;

const float kPullGlowBegin = 1.f;
const float kPullEdgeBegin = 0.6f;

// Min/max velocity that will be absorbed
const float kMinVelocity = 100.f;
const float kMaxVelocity = 10000.f;

const float kEpsilon = 0.001f;

// How much dragging should effect the height of the edge image.
// Number determined by user testing.
const int kPullDistanceEdgeFactor = 7;

// How much dragging should effect the height of the glow image.
// Number determined by user testing.
const int kPullDistanceGlowFactor = 7;
const float kPullDistanceAlphaGlowFactor = 1.1f;

const int kVelocityEdgeFactor = 8;
const int kVelocityGlowFactor = 12;

template <typename T>
T Lerp(T a, T b, T t) {
  return a + (b - a) * t;
}

template <typename T>
T Clamp(T value, T low, T high) {
   return value < low ? low : (value > high ? high : value);
}

template <typename T>
T Damp(T input, T factor) {
  T result;
  if (factor == 1) {
    result = 1 - (1 - input) * (1 - input);
  } else {
    result = 1 - std::pow(1 - input, 2 * factor);
  }
  return result;
}

gfx::Transform ComputeTransform(EdgeEffect::Edge edge,
                                gfx::SizeF size, int height) {
  switch (edge) {
    default:
    case EdgeEffect::EDGE_TOP:
      return gfx::Transform(1, 0, 0, 1, 0, 0);
    case EdgeEffect::EDGE_LEFT:
      return gfx::Transform(0, 1, -1, 0,
                            (-size.width() + height) / 2 ,
                            (size.width() - height) / 2);
    case EdgeEffect::EDGE_BOTTOM:
      return gfx::Transform(-1, 0, 0, -1, 0, size.height() - height);
    case EdgeEffect::EDGE_RIGHT:
      return gfx::Transform(0, -1, 1, 0,
                            (-size.width() - height) / 2 + size.height(),
                            (size.width() - height) / 2);
  };
}

void DisableLayer(cc::Layer* layer) {
  DCHECK(layer);
  layer->SetIsDrawable(false);
  layer->SetTransform(gfx::Transform());
  layer->SetOpacity(1.f);
}

void UpdateLayer(cc::Layer* layer,
                 EdgeEffect::Edge edge,
                 gfx::SizeF size,
                 int height,
                 float opacity) {
  DCHECK(layer);
  layer->SetIsDrawable(true);
  layer->SetTransform(ComputeTransform(edge, size, height));
  layer->SetBounds(gfx::Size(size.width(), height));
  layer->SetOpacity(Clamp(opacity, 0.f, 1.f));
}

} // namespace

EdgeEffect::EdgeEffect(scoped_refptr<cc::Layer> edge,
                       scoped_refptr<cc::Layer> glow)
  : edge_(edge)
  , glow_(glow)
  , edge_alpha_(0)
  , edge_scale_y_(0)
  , glow_alpha_(0)
  , glow_scale_y_(0)
  , edge_alpha_start_(0)
  , edge_alpha_finish_(0)
  , edge_scale_y_start_(0)
  , edge_scale_y_finish_(0)
  , glow_alpha_start_(0)
  , glow_alpha_finish_(0)
  , glow_scale_y_start_(0)
  , glow_scale_y_finish_(0)
  , state_(STATE_IDLE)
  , pull_distance_(0)
  , dpi_scale_(1) {
  // Prevent the provided layers from drawing until the effect is activated.
  DisableLayer(edge_.get());
  DisableLayer(glow_.get());

  dpi_scale_ =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().device_scale_factor();
}

EdgeEffect::~EdgeEffect() { }

bool EdgeEffect::IsFinished() const {
  return state_ == STATE_IDLE;
}

void EdgeEffect::Finish() {
  DisableLayer(edge_.get());
  DisableLayer(glow_.get());
  pull_distance_ = 0;
  state_ = STATE_IDLE;
}

void EdgeEffect::Pull(base::TimeTicks current_time, float delta_distance) {
  if (state_ == STATE_PULL_DECAY && current_time - start_time_ < duration_) {
    return;
  }
  if (state_ != STATE_PULL) {
    glow_scale_y_ = kPullGlowBegin;
  }
  state_ = STATE_PULL;

  start_time_ = current_time;
  duration_ = base::TimeDelta::FromMilliseconds(kPullTime);

  delta_distance *= dpi_scale_;
  float abs_delta_distance = std::abs(delta_distance);
  pull_distance_ += delta_distance;
  float distance = std::abs(pull_distance_);

  edge_alpha_ = edge_alpha_start_ = Clamp(distance, kPullEdgeBegin, kMaxAlpha);
  edge_scale_y_ = edge_scale_y_start_
      = Clamp(distance * kPullDistanceEdgeFactor, kHeldEdgeScaleY, 1.f);

  glow_alpha_ = glow_alpha_start_ =
      std::min(kMaxAlpha,
               glow_alpha_ + abs_delta_distance * kPullDistanceAlphaGlowFactor);

  float glow_change = abs_delta_distance;
  if (delta_distance > 0 && pull_distance_ < 0)
    glow_change = -glow_change;
  if (pull_distance_ == 0)
    glow_scale_y_ = 0;

  // Do not allow glow to get larger than kMaxGlowHeight.
  glow_scale_y_ = glow_scale_y_start_ =
      Clamp(glow_scale_y_ + glow_change * kPullDistanceGlowFactor,
            0.f, kMaxGlowHeight);

  edge_alpha_finish_ = edge_alpha_;
  edge_scale_y_finish_ = edge_scale_y_;
  glow_alpha_finish_ = glow_alpha_;
  glow_scale_y_finish_ = glow_scale_y_;
}

void EdgeEffect::Release(base::TimeTicks current_time) {
  pull_distance_ = 0;

  if (state_ != STATE_PULL && state_ != STATE_PULL_DECAY)
    return;

  state_ = STATE_RECEDE;
  edge_alpha_start_ = edge_alpha_;
  edge_scale_y_start_ = edge_scale_y_;
  glow_alpha_start_ = glow_alpha_;
  glow_scale_y_start_ = glow_scale_y_;

  edge_alpha_finish_ = 0.f;
  edge_scale_y_finish_ = 0.f;
  glow_alpha_finish_ = 0.f;
  glow_scale_y_finish_ = 0.f;

  start_time_ = current_time;
  duration_ = base::TimeDelta::FromMilliseconds(kRecedeTime);
}

void EdgeEffect::Absorb(base::TimeTicks current_time, float velocity) {
  state_ = STATE_ABSORB;
  float scaled_velocity =
      dpi_scale_ * Clamp(std::abs(velocity), kMinVelocity, kMaxVelocity);

  start_time_ = current_time;
  // This should never be less than 1 millisecond.
  duration_ = base::TimeDelta::FromMilliseconds(0.15f + (velocity * 0.02f));

  // The edge should always be at least partially visible, regardless
  // of velocity.
  edge_alpha_start_ = 0.f;
  edge_scale_y_ = edge_scale_y_start_ = 0.f;
  // The glow depends more on the velocity, and therefore starts out
  // nearly invisible.
  glow_alpha_start_ = 0.3f;
  glow_scale_y_start_ = 0.f;

  // Factor the velocity by 8. Testing on device shows this works best to
  // reflect the strength of the user's scrolling.
  edge_alpha_finish_ = Clamp(scaled_velocity * kVelocityEdgeFactor, 0.f, 1.f);
  // Edge should never get larger than the size of its asset.
  edge_scale_y_finish_ = Clamp(scaled_velocity * kVelocityEdgeFactor,
                               kHeldEdgeScaleY, 1.f);

  // Growth for the size of the glow should be quadratic to properly
  // respond
  // to a user's scrolling speed. The faster the scrolling speed, the more
  // intense the effect should be for both the size and the saturation.
  glow_scale_y_finish_ = std::min(
      0.025f + (scaled_velocity * (scaled_velocity / 100) * 0.00015f), 1.75f);
  // Alpha should change for the glow as well as size.
  glow_alpha_finish_ = Clamp(glow_alpha_start_,
                             scaled_velocity * kVelocityGlowFactor * .00001f,
                             kMaxAlpha);
}

bool EdgeEffect::Update(base::TimeTicks current_time) {
  if (IsFinished())
    return false;

  const double dt = (current_time - start_time_).InMilliseconds();
  const double t = std::min(dt / duration_.InMilliseconds(), 1.);
  const float interp = static_cast<float>(Damp(t, 1.));

  edge_alpha_ = Lerp(edge_alpha_start_, edge_alpha_finish_, interp);
  edge_scale_y_ = Lerp(edge_scale_y_start_, edge_scale_y_finish_, interp);
  glow_alpha_ = Lerp(glow_alpha_start_, glow_alpha_finish_, interp);
  glow_scale_y_ = Lerp(glow_scale_y_start_, glow_scale_y_finish_, interp);

  if (t >= 1.f - kEpsilon) {
    switch (state_) {
      case STATE_ABSORB:
        state_ = STATE_RECEDE;
        start_time_ = current_time;
        duration_ = base::TimeDelta::FromMilliseconds(kRecedeTime);

        edge_alpha_start_ = edge_alpha_;
        edge_scale_y_start_ = edge_scale_y_;
        glow_alpha_start_ = glow_alpha_;
        glow_scale_y_start_ = glow_scale_y_;

        // After absorb, the glow and edge should fade to nothing.
        edge_alpha_finish_ = 0.f;
        edge_scale_y_finish_ = 0.f;
        glow_alpha_finish_ = 0.f;
        glow_scale_y_finish_ = 0.f;
        break;
      case STATE_PULL:
        state_ = STATE_PULL_DECAY;
        start_time_ = current_time;
        duration_ = base::TimeDelta::FromMilliseconds(kPullDecayTime);

        edge_alpha_start_ = edge_alpha_;
        edge_scale_y_start_ = edge_scale_y_;
        glow_alpha_start_ = glow_alpha_;
        glow_scale_y_start_ = glow_scale_y_;

        // After pull, the glow and edge should fade to nothing.
        edge_alpha_finish_ = 0.f;
        edge_scale_y_finish_ = 0.f;
        glow_alpha_finish_ = 0.f;
        glow_scale_y_finish_ = 0.f;
        break;
      case STATE_PULL_DECAY:
        {
          // When receding, we want edge to decrease more slowly
          // than the glow.
          float factor = glow_scale_y_finish_ != 0 ?
              1 / (glow_scale_y_finish_ * glow_scale_y_finish_) :
              std::numeric_limits<float>::max();
          edge_scale_y_ = edge_scale_y_start_ +
              (edge_scale_y_finish_ - edge_scale_y_start_) * interp * factor;
          state_ = STATE_RECEDE;
        }
        break;
      case STATE_RECEDE:
        Finish();
        break;
      default:
        break;
    }
  }

  if (state_ == STATE_RECEDE && glow_scale_y_ <= 0 && edge_scale_y_ <= 0)
    Finish();

  return !IsFinished();
}

void EdgeEffect::ApplyToLayers(gfx::SizeF size, Edge edge) {
  if (IsFinished())
    return;

  // An empty effect size, while meaningless, is also relatively harmless, and
  // will simply prevent any drawing of the layers.
  if (size.IsEmpty()) {
    DisableLayer(edge_.get());
    DisableLayer(glow_.get());
    return;
  }

  float dummy_scale_x, dummy_scale_y;

  // Glow
  gfx::Size glow_image_bounds;
  glow_->CalculateContentsScale(1.f, 1.f, 1.f, false,
                                &dummy_scale_x, &dummy_scale_y,
                                &glow_image_bounds);
  const int glow_height = glow_image_bounds.height();
  const int glow_width = glow_image_bounds.width();
  const int glow_bottom = static_cast<int>(std::min(
      glow_height * glow_scale_y_ * glow_height / glow_width * 0.6f,
      glow_height * kMaxGlowHeight) * dpi_scale_ + 0.5f);
  UpdateLayer(glow_.get(), edge, size, glow_bottom, glow_alpha_);

  // Edge
  gfx::Size edge_image_bounds;
  edge_->CalculateContentsScale(1.f, 1.f, 1.f, false,
                                &dummy_scale_x, &dummy_scale_y,
                                &edge_image_bounds);
  const int edge_height = edge_image_bounds.height();
  const int edge_bottom = static_cast<int>(
      edge_height * edge_scale_y_ * dpi_scale_);
  UpdateLayer(edge_.get(), edge, size, edge_bottom, edge_alpha_);
}

} // namespace content
