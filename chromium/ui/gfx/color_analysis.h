// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_ANALYSIS_H_
#define UI_GFX_COLOR_ANALYSIS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/matrix3_f.h"

class SkBitmap;

namespace color_utils {

// This class exposes the sampling method to the caller, which allows
// stubbing out for things like unit tests. Might be useful to pass more
// arguments into the GetSample method in the future (such as which
// cluster is being worked on, etc.).
//
// Note: Samplers should be deterministic, as the same image may be analyzed
// twice with two sampler instances and the results displayed side-by-side
// to the user.
class UI_EXPORT KMeanImageSampler {
 public:
  virtual int GetSample(int width, int height) = 0;

 protected:
  KMeanImageSampler();
  virtual ~KMeanImageSampler();
};

// This sampler will pick pixels from an evenly spaced grid.
class UI_EXPORT GridSampler : public KMeanImageSampler {
  public:
   GridSampler();
   virtual ~GridSampler();

   virtual int GetSample(int width, int height) OVERRIDE;

  private:
   // The number of times GetSample has been called.
   int calls_;
};

// Returns the color in an ARGB |image| that is closest in RGB-space to the
// provided |color|. Exported for testing.
UI_EXPORT SkColor FindClosestColor(const uint8_t* image, int width, int height,
                                   SkColor color);

// Returns an SkColor that represents the calculated dominant color in the png.
// This uses a KMean clustering algorithm to find clusters of pixel colors in
// RGB space.
// |png| represents the data of a png encoded image.
// |darkness_limit| represents the minimum sum of the RGB components that is
// acceptable as a color choice. This can be from 0 to 765.
// |brightness_limit| represents the maximum sum of the RGB components that is
// acceptable as a color choice. This can be from 0 to 765.
//
// RGB KMean Algorithm (N clusters, M iterations):
// 1.Pick N starting colors by randomly sampling the pixels. If you see a
//   color you already saw keep sampling. After a certain number of tries
//   just remove the cluster and continue with N = N-1 clusters (for an image
//   with just one color this should devolve to N=1). These colors are the
//   centers of your N clusters.
// 2.For each pixel in the image find the cluster that it is closest to in RGB
//   space. Add that pixel's color to that cluster (we keep a sum and a count
//   of all of the pixels added to the space, so just add it to the sum and
//   increment count).
// 3.Calculate the new cluster centroids by getting the average color of all of
//   the pixels in each cluster (dividing the sum by the count).
// 4.See if the new centroids are the same as the old centroids.
//     a) If this is the case for all N clusters than we have converged and
//        can move on.
//     b) If any centroid moved, repeat step 2 with the new centroids for up
//        to M iterations.
// 5.Once the clusters have converged or M iterations have been tried, sort
//   the clusters by weight (where weight is the number of pixels that make up
//   this cluster).
// 6.Going through the sorted list of clusters, pick the first cluster with the
//   largest weight that's centroid fulfills the equation
//   |darkness_limit| < SUM(R, G, B) < |brightness_limit|. Return that color.
//   If no color fulfills that requirement return the color with the largest
//   weight regardless of whether or not it fulfills the equation above.
//
// Note: Switching to HSV space did not improve the results of this algorithm
// for typical favicon images.
UI_EXPORT SkColor CalculateKMeanColorOfPNG(
    scoped_refptr<base::RefCountedMemory> png,
    uint32_t darkness_limit,
    uint32_t brightness_limit,
    KMeanImageSampler* sampler);

// Computes a dominant color for an SkBitmap using the above algorithm and
// reasonable defaults for |darkness_limit|, |brightness_limit| and |sampler|.
UI_EXPORT SkColor CalculateKMeanColorOfBitmap(const SkBitmap& bitmap);

// Compute color covariance matrix for the input bitmap.
UI_EXPORT gfx::Matrix3F ComputeColorCovariance(const SkBitmap& bitmap);

// Apply a color reduction transform defined by |color_transform| vector to
// |source_bitmap|. The result is put into |target_bitmap|, which is expected
// to be initialized to the required size and type (SkBitmap::kA8_Config).
// If |fit_to_range|, result is transfored linearly to fit 0-0xFF range.
// Otherwise, data is clipped.
// Returns true if the target has been computed.
UI_EXPORT bool ApplyColorReduction(const SkBitmap& source_bitmap,
                                   const gfx::Vector3dF& color_transform,
                                   bool fit_to_range,
                                   SkBitmap* target_bitmap);

// Compute a monochrome image representing the principal color component of
// the |source_bitmap|. The result is stored in |target_bitmap|, which must be
// initialized to the required size and type (SkBitmap::kA8_Config).
// Returns true if the conversion succeeded. Note that there might be legitimate
// reasons for the process to fail even if all input was correct. This is a
// condition the caller must be able to handle.
UI_EXPORT bool ComputePrincipalComponentImage(const SkBitmap& source_bitmap,
                                              SkBitmap* target_bitmap);

}  // namespace color_utils

#endif  // UI_GFX_COLOR_ANALYSIS_H_
