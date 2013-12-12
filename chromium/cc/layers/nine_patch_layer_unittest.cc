// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer.h"

#include "cc/debug/overdraw_metrics.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/scheduler/texture_uploader.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class MockLayerTreeHost : public LayerTreeHost {
 public:
  explicit MockLayerTreeHost(LayerTreeHostClient* client)
      : LayerTreeHost(client, LayerTreeSettings()) {
    Initialize(NULL);
  }
};

class NinePatchLayerTest : public testing::Test {
 public:
  NinePatchLayerTest() : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

  cc::Proxy* Proxy() const { return layer_tree_host_->proxy(); }

 protected:
  virtual void SetUp() {
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
  FakeLayerTreeHostClient fake_client_;
};

TEST_F(NinePatchLayerTest, SetBitmap) {
  scoped_refptr<NinePatchLayer> test_layer = NinePatchLayer::Create();
  ASSERT_TRUE(test_layer.get());
  test_layer->SetIsDrawable(true);
  test_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  layer_tree_host_->InitializeOutputSurfaceIfNeeded();

  ResourceUpdateQueue queue;
  OcclusionTracker occlusion_tracker(gfx::Rect(), false);
  test_layer->SavePaintProperties();
  test_layer->Update(&queue, &occlusion_tracker);

  EXPECT_FALSE(test_layer->DrawsContent());

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  bitmap.allocPixels();
  bitmap.setImmutable();

  gfx::Rect aperture(5, 5, 1, 1);
  bool fill_center = false;
  test_layer->SetBitmap(bitmap, aperture);
  test_layer->SetFillCenter(fill_center);
  test_layer->Update(&queue, &occlusion_tracker);

  EXPECT_TRUE(test_layer->DrawsContent());
}

TEST_F(NinePatchLayerTest, SetUIResourceId) {
  scoped_refptr<NinePatchLayer> test_layer = NinePatchLayer::Create();
  ASSERT_TRUE(test_layer.get());
  test_layer->SetIsDrawable(true);
  test_layer->SetBounds(gfx::Size(100, 100));

  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  layer_tree_host_->InitializeOutputSurfaceIfNeeded();

  ResourceUpdateQueue queue;
  OcclusionTracker occlusion_tracker(gfx::Rect(), false);
  test_layer->SavePaintProperties();
  test_layer->Update(&queue, &occlusion_tracker);

  EXPECT_FALSE(test_layer->DrawsContent());

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  bitmap.allocPixels();
  bitmap.setImmutable();

  scoped_ptr<ScopedUIResource> resource = ScopedUIResource::Create(
      layer_tree_host_.get(), UIResourceBitmap(bitmap));
  gfx::Rect aperture(5, 5, 1, 1);
  bool fill_center = true;
  test_layer->SetUIResourceId(resource->id(), aperture);
  test_layer->SetFillCenter(fill_center);
  test_layer->Update(&queue, &occlusion_tracker);

  EXPECT_TRUE(test_layer->DrawsContent());
}

}  // namespace
}  // namespace cc
