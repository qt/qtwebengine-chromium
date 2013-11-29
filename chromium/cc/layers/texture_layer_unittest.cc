// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <string>

#include "base/callback.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  MOCK_METHOD0(AcquireLayerTextures, void());
  MOCK_METHOD0(SetNeedsCommit, void());
  MOCK_METHOD0(SetNeedsUpdateLayers, void());
  MOCK_METHOD1(StartRateLimiter, void(WebKit::WebGraphicsContext3D* context));
  MOCK_METHOD1(StopRateLimiter, void(WebKit::WebGraphicsContext3D* context));
};

class TextureLayerTest : public testing::Test {
 public:
  TextureLayerTest()
      : fake_client_(
          FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)),
        host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());

    layer_tree_host_->SetRootLayer(NULL);
    layer_tree_host_.reset();
  }

  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostClient fake_client_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(TextureLayerTest, SyncImplWhenChangingTextureId) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(1);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(2);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(0);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, SyncImplWhenDrawing) {
  gfx::RectF dirty_rect(0.f, 0.f, 1.f, 1.f);

  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer.get());
  scoped_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  test_layer->SetTextureId(1);
  test_layer->SetIsDrawable(true);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(1);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(1);
  test_layer->SetNeedsDisplayRect(dirty_rect);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  test_layer->PushPropertiesTo(impl_layer.get());  // fake commit
  test_layer->SetIsDrawable(false);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Verify that non-drawable layers don't signal the compositor,
  // except for the first draw after last commit, which must acquire
  // the texture.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(1);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  test_layer->SetNeedsDisplayRect(dirty_rect);
  test_layer->PushPropertiesTo(impl_layer.get());  // fake commit
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Second draw with layer in non-drawable state: no texture
  // acquisition.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  test_layer->SetNeedsDisplayRect(dirty_rect);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, SyncImplWhenRemovingFromTree) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  ASSERT_TRUE(root_layer.get());
  scoped_refptr<Layer> child_layer = Layer::Create();
  ASSERT_TRUE(child_layer.get());
  root_layer->AddChild(child_layer);
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer.get());
  test_layer->SetTextureId(0);
  child_layer->AddChild(test_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(root_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->RemoveFromParent();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  child_layer->AddChild(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(1);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->RemoveFromParent();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  layer_tree_host_->SetRootLayer(test_layer);

  // Test properties that should call SetNeedsCommit.  All properties need to
  // be set to new values in order for SetNeedsCommit to be called.
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetFlipped(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetUV(
      gfx::PointF(0.25f, 0.25f), gfx::PointF(0.75f, 0.75f)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetVertexOpacity(
      0.5f, 0.5f, 0.5f, 0.5f));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetPremultipliedAlpha(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBlendBackgroundColor(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetTextureId(1));

  // Calling SetTextureId can call AcquireLayerTextures.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
}

TEST_F(TextureLayerTest, VisibleContentOpaqueRegion) {
  const gfx::Size layer_bounds(100, 100);
  const gfx::Rect layer_rect(layer_bounds);
  const Region layer_region(layer_rect);

  scoped_refptr<TextureLayer> layer = TextureLayer::Create(NULL);
  layer->SetBounds(layer_bounds);
  layer->draw_properties().visible_content_rect = layer_rect;
  layer->SetBlendBackgroundColor(true);

  // Verify initial conditions.
  EXPECT_FALSE(layer->contents_opaque());
  EXPECT_EQ(0u, layer->background_color());
  EXPECT_EQ(Region().ToString(),
            layer->VisibleContentOpaqueRegion().ToString());

  // Opaque background.
  layer->SetBackgroundColor(SK_ColorWHITE);
  EXPECT_EQ(layer_region.ToString(),
            layer->VisibleContentOpaqueRegion().ToString());

  // Transparent background.
  layer->SetBackgroundColor(SkColorSetARGB(100, 255, 255, 255));
  EXPECT_EQ(Region().ToString(),
            layer->VisibleContentOpaqueRegion().ToString());
}

class FakeTextureLayerClient : public TextureLayerClient {
 public:
  FakeTextureLayerClient() : context_(TestWebGraphicsContext3D::Create()) {}

  virtual unsigned PrepareTexture() OVERRIDE {
    return 0;
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_.get();
  }

  virtual bool PrepareTextureMailbox(TextureMailbox* mailbox,
                                     bool use_shared_memory) OVERRIDE {
    *mailbox = TextureMailbox();
    return true;
  }

 private:
  scoped_ptr<TestWebGraphicsContext3D> context_;
  DISALLOW_COPY_AND_ASSIGN(FakeTextureLayerClient);
};

TEST_F(TextureLayerTest, RateLimiter) {
  FakeTextureLayerClient client;
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(
      &client);
  test_layer->SetIsDrawable(true);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);

  // Don't rate limit until we invalidate.
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter(_)).Times(0);
  test_layer->SetRateLimitContext(true);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Do rate limit after we invalidate.
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter(client.Context3d()));
  test_layer->SetNeedsDisplay();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Stop rate limiter when we don't want it any more.
  EXPECT_CALL(*layer_tree_host_, StopRateLimiter(client.Context3d()));
  test_layer->SetRateLimitContext(false);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Or we clear the client.
  test_layer->SetRateLimitContext(true);
  EXPECT_CALL(*layer_tree_host_, StopRateLimiter(client.Context3d()));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  test_layer->ClearClient();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Reset to a layer with a client, that started the rate limiter.
  test_layer = TextureLayer::CreateForMailbox(
      &client);
  test_layer->SetIsDrawable(true);
  test_layer->SetRateLimitContext(true);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter(_)).Times(0);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter(client.Context3d()));
  test_layer->SetNeedsDisplay();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Stop rate limiter when we're removed from the tree.
  EXPECT_CALL(*layer_tree_host_, StopRateLimiter(client.Context3d()));
  layer_tree_host_->SetRootLayer(NULL);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

class MockMailboxCallback {
 public:
  MOCK_METHOD3(Release, void(const std::string& mailbox,
                             unsigned sync_point,
                             bool lost_resource));
  MOCK_METHOD3(Release2, void(base::SharedMemory* shared_memory,
                              unsigned sync_point,
                              bool lost_resource));
};

struct CommonMailboxObjects {
  CommonMailboxObjects()
      : mailbox_name1_(64, '1'),
        mailbox_name2_(64, '2'),
        sync_point1_(1),
        sync_point2_(2),
        shared_memory_(new base::SharedMemory) {
    release_mailbox1_ = base::Bind(&MockMailboxCallback::Release,
                                   base::Unretained(&mock_callback_),
                                   mailbox_name1_);
    release_mailbox2_ = base::Bind(&MockMailboxCallback::Release,
                                   base::Unretained(&mock_callback_),
                                   mailbox_name2_);
    gpu::Mailbox m1;
    m1.SetName(reinterpret_cast<const int8*>(mailbox_name1_.data()));
    mailbox1_ = TextureMailbox(m1, release_mailbox1_, sync_point1_);
    gpu::Mailbox m2;
    m2.SetName(reinterpret_cast<const int8*>(mailbox_name2_.data()));
    mailbox2_ = TextureMailbox(m2, release_mailbox2_, sync_point2_);

    gfx::Size size(128, 128);
    EXPECT_TRUE(shared_memory_->CreateAndMapAnonymous(4 * size.GetArea()));
    release_mailbox3_ = base::Bind(&MockMailboxCallback::Release2,
                                   base::Unretained(&mock_callback_),
                                   shared_memory_.get());
    mailbox3_ = TextureMailbox(shared_memory_.get(), size, release_mailbox3_);
  }

  std::string mailbox_name1_;
  std::string mailbox_name2_;
  MockMailboxCallback mock_callback_;
  TextureMailbox::ReleaseCallback release_mailbox1_;
  TextureMailbox::ReleaseCallback release_mailbox2_;
  TextureMailbox::ReleaseCallback release_mailbox3_;
  TextureMailbox mailbox1_;
  TextureMailbox mailbox2_;
  TextureMailbox mailbox3_;
  unsigned sync_point1_;
  unsigned sync_point2_;
  scoped_ptr<base::SharedMemory> shared_memory_;
};

class TextureLayerWithMailboxTest : public TextureLayerTest {
 protected:
  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
    EXPECT_CALL(test_data_.mock_callback_,
                Release(test_data_.mailbox_name1_,
                        test_data_.sync_point1_,
                        false)).Times(1);
    TextureLayerTest::TearDown();
  }

  CommonMailboxObjects test_data_;
};

TEST_F(TextureLayerWithMailboxTest, ReplaceMailboxOnMainThreadBeforeCommit) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(NULL);
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(test_data_.mailbox1_);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(1);
  test_layer->SetTextureMailbox(test_data_.mailbox2_);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name2_,
                      test_data_.sync_point2_,
                      false))
      .Times(1);
  test_layer->SetTextureMailbox(TextureMailbox());
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  test_layer->SetTextureMailbox(test_data_.mailbox3_);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_memory_.get(),
                       0, false))
      .Times(1);
  test_layer->SetTextureMailbox(TextureMailbox());
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(test_data_.mailbox1_);
}

class TextureLayerImplWithMailboxThreadedCallback : public LayerTreeTest {
 public:
  TextureLayerImplWithMailboxThreadedCallback()
      : callback_count_(0),
        commit_count_(0) {}

  // Make sure callback is received on main and doesn't block the impl thread.
  void ReleaseCallback(unsigned sync_point, bool lost_resource) {
    EXPECT_EQ(true, proxy()->IsMainThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
  }

  void SetMailbox(char mailbox_char) {
    TextureMailbox mailbox(
        std::string(64, mailbox_char),
        base::Bind(
            &TextureLayerImplWithMailboxThreadedCallback::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox);
  }

  virtual void BeginTest() OVERRIDE {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    // Case #1: change mailbox before the commit. The old mailbox should be
    // released immediately.
    SetMailbox('2');
    EXPECT_EQ(1, callback_count_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Case #2: change mailbox after the commit (and draw), where the
        // layer draws. The old mailbox should be released during the next
        // commit.
        SetMailbox('3');
        EXPECT_EQ(1, callback_count_);
        break;
      case 2:
        // Old mailbox was released, task was posted, but won't execute
        // until this DidCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(1, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        EXPECT_EQ(2, callback_count_);
        // Case #3: change mailbox when the layer doesn't draw. The old
        // mailbox should be released during the next commit.
        layer_->SetBounds(gfx::Size());
        SetMailbox('4');
        break;
      case 4:
        // Old mailbox was released, task was posted, but won't execute
        // until this DidCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(2, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        EXPECT_EQ(3, callback_count_);
        // Case #4: release mailbox that was committed but never drawn. The
        // old mailbox should be released during the next commit.
        layer_->SetTextureMailbox(TextureMailbox());
        break;
      case 6:
        // Old mailbox was released, task was posted, but won't execute
        // until this DidCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(3, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 7:
        EXPECT_EQ(4, callback_count_);
        // Restore a mailbox for the next step.
        SetMailbox('5');
        break;
      case 8:
        // Case #5: remove layer from tree. Callback should *not* be called, the
        // mailbox is returned to the main thread.
        EXPECT_EQ(4, callback_count_);
        layer_->RemoveFromParent();
        break;
      case 9:
        // Mailbox was released to the main thread, task was posted, but won't
        // execute until this DidCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(4, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 10:
        EXPECT_EQ(4, callback_count_);
        // Resetting the mailbox will call the callback now.
        layer_->SetTextureMailbox(TextureMailbox());
        EXPECT_EQ(5, callback_count_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int callback_count_;
  int commit_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerImplWithMailboxThreadedCallback);

class TextureLayerImplWithMailboxTest : public TextureLayerTest {
 protected:
  TextureLayerImplWithMailboxTest()
      : fake_client_(
          FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)) {}

  virtual void SetUp() {
    TextureLayerTest::SetUp();
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
    EXPECT_TRUE(host_impl_.InitializeRenderer(CreateFakeOutputSurface()));
  }

  bool WillDraw(TextureLayerImpl* layer, DrawMode mode) {
    bool will_draw = layer->WillDraw(
        mode, host_impl_.active_tree()->resource_provider());
    if (will_draw)
      layer->DidDraw(host_impl_.active_tree()->resource_provider());
    return will_draw;
  }

  CommonMailboxObjects test_data_;
  FakeLayerTreeHostClient fake_client_;
};

// Test conditions for results of TextureLayerImpl::WillDraw under
// different configurations of different mailbox, texture_id, and draw_mode.
TEST_F(TextureLayerImplWithMailboxTest, TestWillDraw) {
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(AnyNumber());
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_memory_.get(), 0, false))
      .Times(AnyNumber());
  // Hardware mode.
  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(test_data_.mailbox1_);
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(TextureMailbox());
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    // Software resource.
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(test_data_.mailbox3_);
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    unsigned texture =
        host_impl_.output_surface()->context3d()->createTexture();
    impl_layer->set_texture_id(texture);
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->set_texture_id(0);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  // Software mode.
  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(test_data_.mailbox1_);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(TextureMailbox());
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    // Software resource.
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(test_data_.mailbox3_);
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    unsigned texture =
        host_impl_.output_surface()->context3d()->createTexture();
    impl_layer->set_texture_id(texture);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->set_texture_id(0);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  // Resourceless software mode.
  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetTextureMailbox(test_data_.mailbox1_);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_RESOURCELESS_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    unsigned texture =
        host_impl_.output_surface()->context3d()->createTexture();
    impl_layer->set_texture_id(texture);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_RESOURCELESS_SOFTWARE));
  }
}

TEST_F(TextureLayerImplWithMailboxTest, TestImplLayerCallbacks) {
  host_impl_.CreatePendingTree();
  scoped_ptr<TextureLayerImpl> pending_layer;
  pending_layer = TextureLayerImpl::Create(host_impl_.pending_tree(), 1, true);
  ASSERT_TRUE(pending_layer);

  scoped_ptr<LayerImpl> active_layer(
      pending_layer->CreateLayerImpl(host_impl_.active_tree()));
  ASSERT_TRUE(active_layer);

  pending_layer->SetTextureMailbox(test_data_.mailbox1_);

  // Test multiple commits without an activation.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(1);
  pending_layer->SetTextureMailbox(test_data_.mailbox2_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test callback after activation.
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  pending_layer->SetTextureMailbox(test_data_.mailbox1_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name2_, _, false))
      .Times(1);
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test resetting the mailbox.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  pending_layer->SetTextureMailbox(TextureMailbox());
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(1);
  pending_layer->SetTextureMailbox(test_data_.mailbox1_);
}

TEST_F(TextureLayerImplWithMailboxTest,
       TestDestructorCallbackOnCreatedResource) {
  scoped_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  impl_layer->SetTextureMailbox(test_data_.mailbox1_);
  impl_layer->DidBecomeActive();
  EXPECT_TRUE(impl_layer->WillDraw(
      DRAW_MODE_HARDWARE, host_impl_.active_tree()->resource_provider()));
  impl_layer->DidDraw(host_impl_.active_tree()->resource_provider());
  impl_layer->SetTextureMailbox(TextureMailbox());
}

TEST_F(TextureLayerImplWithMailboxTest, TestCallbackOnInUseResource) {
  ResourceProvider* provider = host_impl_.active_tree()->resource_provider();
  ResourceProvider::ResourceId id =
      provider->CreateResourceFromTextureMailbox(test_data_.mailbox1_);
  provider->AllocateForTesting(id);

  // Transfer some resources to the parent.
  ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id);
  TransferableResourceArray list;
  provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  EXPECT_TRUE(provider->InUseByConsumer(id));
  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  provider->ReceiveFromParent(list);
}

// Check that ClearClient correctly clears the state so that the impl side
// doesn't try to use a texture that could have been destroyed.
class TextureLayerClientTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerClientTest()
      : context_(NULL),
        texture_(0),
        commit_count_(0),
        expected_used_textures_on_draw_(0),
        expected_used_textures_on_commit_(0) {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE {
    scoped_ptr<TestWebGraphicsContext3D> context(
        TestWebGraphicsContext3D::Create());
    context_ = context.get();
    texture_ = context->createTexture();
    return FakeOutputSurface::Create3d(
        context.PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
  }

  virtual unsigned PrepareTexture() OVERRIDE {
    return texture_;
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_;
  }

  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox, bool use_shared_memory) OVERRIDE {
    return false;
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    texture_layer_ = TextureLayer::Create(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetAnchorPoint(gfx::PointF());
    texture_layer_->SetIsDrawable(true);
    root->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    {
      base::AutoLock lock(lock_);
      expected_used_textures_on_commit_ = 1;
    }
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        texture_layer_->ClearClient();
        texture_layer_->SetNeedsDisplay();
        {
          base::AutoLock lock(lock_);
          expected_used_textures_on_commit_ = 0;
        }
        texture_ = 0;
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    base::AutoLock lock(lock_);
    expected_used_textures_on_draw_ = expected_used_textures_on_commit_;
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    context_->ResetUsedTextures();
    return true;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ASSERT_TRUE(result);
    EXPECT_EQ(expected_used_textures_on_draw_, context_->NumUsedTextures());
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<TextureLayer> texture_layer_;
  TestWebGraphicsContext3D* context_;
  unsigned texture_;
  int commit_count_;

  // Used only on thread.
  unsigned expected_used_textures_on_draw_;

  // Used on either thread, protected by lock_.
  base::Lock lock_;
  unsigned expected_used_textures_on_commit_;
};

// The TextureLayerClient does not use mailboxes, so can't use a delegating
// renderer.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TextureLayerClientTest);

// Test recovering from a lost context.
class TextureLayerLostContextTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerLostContextTest()
      : texture_(0),
        draw_count_(0) {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE {
    texture_context_ = TestWebGraphicsContext3D::Create();
    texture_ = texture_context_->createTexture();
    return CreateFakeOutputSurface();
  }

  virtual unsigned PrepareTexture() OVERRIDE {
    if (draw_count_ == 0) {
      texture_context_->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
          GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    return texture_;
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return texture_context_.get();
  }

  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox, bool use_shared_memory) OVERRIDE {
    return false;
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    texture_layer_ = TextureLayer::Create(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetIsDrawable(true);
    root->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    LayerImpl* root = host_impl->RootLayer();
    TextureLayerImpl* texture_layer =
        static_cast<TextureLayerImpl*>(root->children()[0]);
    if (++draw_count_ == 1)
      EXPECT_EQ(0u, texture_layer->texture_id());
    else
      EXPECT_EQ(texture_, texture_layer->texture_id());
    return true;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<TextureLayer> texture_layer_;
  scoped_ptr<TestWebGraphicsContext3D> texture_context_;
  unsigned texture_;
  int draw_count_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TextureLayerLostContextTest);

class TextureLayerWithMailboxMainThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(unsigned sync_point, bool lost_resource) {
    EXPECT_EQ(true, proxy()->IsMainThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    TextureMailbox mailbox(
        std::string(64, mailbox_char),
        base::Bind(
            &TextureLayerWithMailboxMainThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox);
  }

  virtual void SetupTree() OVERRIDE {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
  }

  virtual void BeginTest() OVERRIDE {
    callback_count_ = 0;

    // Set the mailbox on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Delete the TextureLayer on the main thread while the mailbox is in
        // the impl tree.
        layer_->RemoveFromParent();
        layer_ = NULL;
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, callback_count_);
  }

 private:
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerWithMailboxMainThreadDeleted);

class TextureLayerWithMailboxImplThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(unsigned sync_point, bool lost_resource) {
    EXPECT_EQ(true, proxy()->IsMainThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    TextureMailbox mailbox(
        std::string(64, mailbox_char),
        base::Bind(
            &TextureLayerWithMailboxImplThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox);
  }

  virtual void SetupTree() OVERRIDE {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
  }

  virtual void BeginTest() OVERRIDE {
    callback_count_ = 0;

    // Set the mailbox on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Remove the TextureLayer on the main thread while the mailbox is in
        // the impl tree, but don't delete the TextureLayer until after the impl
        // tree side is deleted.
        layer_->RemoveFromParent();
        break;
      case 2:
        layer_ = NULL;
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, callback_count_);
  }

 private:
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerWithMailboxImplThreadDeleted);

}  // namespace
}  // namespace cc
