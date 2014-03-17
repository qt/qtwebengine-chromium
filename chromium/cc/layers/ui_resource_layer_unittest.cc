// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/ui_resource_layer.h"

#include "cc/debug/overdraw_metrics.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/scheduler/texture_uploader.h"
#include "cc/test/fake_layer_tree_host.h"
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

class UIResourceLayerTest : public testing::Test {
 public:
  UIResourceLayerTest() : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  virtual void SetUp() {
    layer_tree_host_ = FakeLayerTreeHost::Create();
    layer_tree_host_->InitializeSingleThreaded(&fake_client_);
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

  scoped_ptr<FakeLayerTreeHost> layer_tree_host_;
  FakeLayerTreeHostClient fake_client_;
};

TEST_F(UIResourceLayerTest, SetBitmap) {
  scoped_refptr<UIResourceLayer> test_layer = UIResourceLayer::Create();
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

  test_layer->SetBitmap(bitmap);
  test_layer->Update(&queue, &occlusion_tracker);

  EXPECT_TRUE(test_layer->DrawsContent());
}

TEST_F(UIResourceLayerTest, SetUIResourceId) {
  scoped_refptr<UIResourceLayer> test_layer = UIResourceLayer::Create();
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
  test_layer->SetUIResourceId(resource->id());
  test_layer->Update(&queue, &occlusion_tracker);

  EXPECT_TRUE(test_layer->DrawsContent());
}

}  // namespace
}  // namespace cc
