// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <algorithm>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/debug/test_web_graphics_context_3d.h"
#include "cc/output/output_surface.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/single_release_callback.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"

using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::_;
using WebKit::WGC3Dbyte;
using WebKit::WGC3Denum;
using WebKit::WGC3Dint;
using WebKit::WGC3Dsizei;
using WebKit::WGC3Duint;
using WebKit::WebGLId;

namespace cc {
namespace {

size_t TextureSize(gfx::Size size, ResourceFormat format) {
  unsigned int components_per_pixel = 4;
  unsigned int bytes_per_component = 1;
  return size.width() * size.height() * components_per_pixel *
      bytes_per_component;
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD2(bindTexture, void(WGC3Denum target, WebGLId texture));
  MOCK_METHOD3(texParameteri,
               void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));
  MOCK_METHOD1(waitSyncPoint, void(unsigned sync_point));
  MOCK_METHOD0(insertSyncPoint, unsigned(void));
  MOCK_METHOD2(produceTextureCHROMIUM, void(WGC3Denum target,
                                            const WGC3Dbyte* mailbox));
  MOCK_METHOD2(consumeTextureCHROMIUM, void(WGC3Denum target,
                                            const WGC3Dbyte* mailbox));

  // Force all textures to be "1" so we can test for them.
  virtual WebKit::WebGLId NextTextureId() OVERRIDE { return 1; }
};

struct Texture : public base::RefCounted<Texture> {
  Texture() : format(RGBA_8888),
              filter(GL_NEAREST_MIPMAP_LINEAR) {}

  void Reallocate(gfx::Size size, ResourceFormat format) {
    this->size = size;
    this->format = format;
    this->data.reset(new uint8_t[TextureSize(size, format)]);
  }

  gfx::Size size;
  ResourceFormat format;
  WGC3Denum filter;
  scoped_ptr<uint8_t[]> data;

 private:
  friend class base::RefCounted<Texture>;
  ~Texture() {}
};

// Shared data between multiple ResourceProviderContext. This contains mailbox
// contents as well as information about sync points.
class ContextSharedData {
 public:
  static scoped_ptr<ContextSharedData> Create() {
    return make_scoped_ptr(new ContextSharedData());
  }

  unsigned InsertSyncPoint() { return next_sync_point_++; }

  void GenMailbox(WGC3Dbyte* mailbox) {
    memset(mailbox, 0, sizeof(WGC3Dbyte[64]));
    memcpy(mailbox, &next_mailbox_, sizeof(next_mailbox_));
    ++next_mailbox_;
  }

  void ProduceTexture(const WGC3Dbyte* mailbox_name,
                      unsigned sync_point,
                      scoped_refptr<Texture> texture) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    ASSERT_TRUE(mailbox && mailbox < next_mailbox_);
    textures_[mailbox] = texture;
    ASSERT_LT(sync_point_for_mailbox_[mailbox], sync_point);
    sync_point_for_mailbox_[mailbox] = sync_point;
  }

  scoped_refptr<Texture> ConsumeTexture(const WGC3Dbyte* mailbox_name,
                                     unsigned sync_point) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    DCHECK(mailbox && mailbox < next_mailbox_);

    // If the latest sync point the context has waited on is before the sync
    // point for when the mailbox was set, pretend we never saw that
    // ProduceTexture.
    if (sync_point_for_mailbox_[mailbox] > sync_point) {
      NOTREACHED();
      return scoped_refptr<Texture>();
    }
    return textures_[mailbox];
  }

 private:
  ContextSharedData() : next_sync_point_(1), next_mailbox_(1) {}

  unsigned next_sync_point_;
  unsigned next_mailbox_;
  typedef base::hash_map<unsigned, scoped_refptr<Texture> > TextureMap;
  TextureMap textures_;
  base::hash_map<unsigned, unsigned> sync_point_for_mailbox_;
};

class ResourceProviderContext : public TestWebGraphicsContext3D {
 public:
  static scoped_ptr<ResourceProviderContext> Create(
      ContextSharedData* shared_data) {
    return make_scoped_ptr(
        new ResourceProviderContext(Attributes(), shared_data));
  }

  virtual unsigned insertSyncPoint() OVERRIDE {
    unsigned sync_point = shared_data_->InsertSyncPoint();
    // Commit the produceTextureCHROMIUM calls at this point, so that
    // they're associated with the sync point.
    for (PendingProduceTextureList::iterator it =
             pending_produce_textures_.begin();
         it != pending_produce_textures_.end();
         ++it) {
      shared_data_->ProduceTexture(
          (*it)->mailbox, sync_point, (*it)->texture);
    }
    pending_produce_textures_.clear();
    return sync_point;
  }

  virtual void waitSyncPoint(unsigned sync_point) OVERRIDE {
    last_waited_sync_point_ = std::max(sync_point, last_waited_sync_point_);
  }

  virtual void bindTexture(WGC3Denum target, WebGLId texture) OVERRIDE {
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_TRUE(!texture || textures_.find(texture) != textures_.end());
    current_texture_ = texture;
  }

  virtual WebGLId createTexture() OVERRIDE {
    WebGLId id = TestWebGraphicsContext3D::createTexture();
    textures_[id] = new Texture;
    return id;
  }

  virtual void deleteTexture(WebGLId id) OVERRIDE {
    TextureMap::iterator it = textures_.find(id);
    ASSERT_FALSE(it == textures_.end());
    textures_.erase(it);
    if (current_texture_ == id)
      current_texture_ = 0;
  }

  virtual void texStorage2DEXT(WGC3Denum target,
                               WGC3Dint levels,
                               WGC3Duint internalformat,
                               WGC3Dint width,
                               WGC3Dint height) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_EQ(1, levels);
    WGC3Denum format = GL_RGBA;
    switch (internalformat) {
      case GL_RGBA8_OES:
        break;
      case GL_BGRA8_EXT:
        format = GL_BGRA_EXT;
        break;
      default:
        NOTREACHED();
    }
    AllocateTexture(gfx::Size(width, height), format);
  }

  virtual void texImage2D(WGC3Denum target,
                          WGC3Dint level,
                          WGC3Denum internalformat,
                          WGC3Dsizei width,
                          WGC3Dsizei height,
                          WGC3Dint border,
                          WGC3Denum format,
                          WGC3Denum type,
                          const void* pixels) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(internalformat, format);
    ASSERT_FALSE(border);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    AllocateTexture(gfx::Size(width, height), format);
    if (pixels)
      SetPixels(0, 0, width, height, pixels);
  }

  virtual void texSubImage2D(WGC3Denum target,
                             WGC3Dint level,
                             WGC3Dint xoffset,
                             WGC3Dint yoffset,
                             WGC3Dsizei width,
                             WGC3Dsizei height,
                             WGC3Denum format,
                             WGC3Denum type,
                             const void* pixels) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_TRUE(textures_[current_texture_].get());
    ASSERT_EQ(
        ResourceProvider::GetGLDataFormat(textures_[current_texture_]->format),
        format);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    ASSERT_TRUE(pixels);
    SetPixels(xoffset, yoffset, width, height, pixels);
  }

  virtual void texParameteri(WGC3Denum target, WGC3Denum param, WGC3Dint value)
      OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    scoped_refptr<Texture> texture = textures_[current_texture_];
    ASSERT_TRUE(texture.get());
    if (param != GL_TEXTURE_MIN_FILTER)
      return;
    texture->filter = value;
  }

  virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox) OVERRIDE {
    return shared_data_->GenMailbox(mailbox);
  }

  virtual void produceTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);

    // Delay moving the texture into the mailbox until the next
    // InsertSyncPoint, so that it is not visible to other contexts that
    // haven't waited on that sync point.
    scoped_ptr<PendingProduceTexture> pending(new PendingProduceTexture);
    memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
    pending->texture = textures_[current_texture_];
    pending_produce_textures_.push_back(pending.Pass());
  }

  virtual void consumeTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    textures_[current_texture_] = shared_data_->ConsumeTexture(
        mailbox, last_waited_sync_point_);
  }

  void GetPixels(gfx::Size size, ResourceFormat format, uint8_t* pixels) {
    ASSERT_TRUE(current_texture_);
    scoped_refptr<Texture> texture = textures_[current_texture_];
    ASSERT_TRUE(texture.get());
    ASSERT_EQ(texture->size, size);
    ASSERT_EQ(texture->format, format);
    memcpy(pixels, texture->data.get(), TextureSize(size, format));
  }

  WGC3Denum GetTextureFilter() {
    DCHECK(current_texture_);
    scoped_refptr<Texture> texture = textures_[current_texture_];
    DCHECK(texture.get());
    return texture->filter;
  }

  int texture_count() { return textures_.size(); }

 protected:
  ResourceProviderContext(const Attributes& attrs,
                          ContextSharedData* shared_data)
      : TestWebGraphicsContext3D(attrs),
        shared_data_(shared_data),
        current_texture_(0),
        last_waited_sync_point_(0) {}

 private:
  void AllocateTexture(gfx::Size size, WGC3Denum format) {
    ASSERT_TRUE(current_texture_);
    scoped_refptr<Texture> texture = textures_[current_texture_];
    ASSERT_TRUE(texture.get());
    ResourceFormat texture_format = RGBA_8888;
    switch (format) {
      case GL_RGBA:
        texture_format = RGBA_8888;
        break;
      case GL_BGRA_EXT:
        texture_format = BGRA_8888;
        break;
    }
    texture->Reallocate(size, texture_format);
  }

  void SetPixels(int xoffset,
                 int yoffset,
                 int width,
                 int height,
                 const void* pixels) {
    ASSERT_TRUE(current_texture_);
    scoped_refptr<Texture> texture = textures_[current_texture_];
    ASSERT_TRUE(texture.get());
    ASSERT_TRUE(texture->data.get());
    ASSERT_TRUE(xoffset >= 0 && xoffset + width <= texture->size.width());
    ASSERT_TRUE(yoffset >= 0 && yoffset + height <= texture->size.height());
    ASSERT_TRUE(pixels);
    size_t in_pitch = TextureSize(gfx::Size(width, 1), texture->format);
    size_t out_pitch =
        TextureSize(gfx::Size(texture->size.width(), 1), texture->format);
    uint8_t* dest = texture->data.get() + yoffset * out_pitch +
                    TextureSize(gfx::Size(xoffset, 1), texture->format);
    const uint8_t* src = static_cast<const uint8_t*>(pixels);
    for (int i = 0; i < height; ++i) {
      memcpy(dest, src, in_pitch);
      dest += out_pitch;
      src += in_pitch;
    }
  }

  typedef base::hash_map<WebGLId, scoped_refptr<Texture> > TextureMap;
  struct PendingProduceTexture {
    WGC3Dbyte mailbox[64];
    scoped_refptr<Texture> texture;
  };
  typedef ScopedPtrDeque<PendingProduceTexture> PendingProduceTextureList;
  ContextSharedData* shared_data_;
  WebGLId current_texture_;
  TextureMap textures_;
  unsigned last_waited_sync_point_;
  PendingProduceTextureList pending_produce_textures_;
};

void GetResourcePixels(ResourceProvider* resource_provider,
                       ResourceProviderContext* context,
                       ResourceProvider::ResourceId id,
                       gfx::Size size,
                       ResourceFormat format,
                       uint8_t* pixels) {
  switch (resource_provider->default_resource_type()) {
    case ResourceProvider::GLTexture: {
      ResourceProvider::ScopedReadLockGL lock_gl(resource_provider, id);
      ASSERT_NE(0U, lock_gl.texture_id());
      context->bindTexture(GL_TEXTURE_2D, lock_gl.texture_id());
      context->GetPixels(size, format, pixels);
      break;
    }
    case ResourceProvider::Bitmap: {
      ResourceProvider::ScopedReadLockSoftware lock_software(resource_provider,
                                                             id);
      memcpy(pixels,
             lock_software.sk_bitmap()->getPixels(),
             lock_software.sk_bitmap()->getSize());
      break;
    }
    case ResourceProvider::InvalidType:
      NOTREACHED();
      break;
  }
}

class ResourceProviderTest
    : public testing::TestWithParam<ResourceProvider::ResourceType> {
 public:
  ResourceProviderTest()
      : shared_data_(ContextSharedData::Create()),
        context3d_(NULL) {
    switch (GetParam()) {
      case ResourceProvider::GLTexture: {
        scoped_ptr<ResourceProviderContext> context3d(
            ResourceProviderContext::Create(shared_data_.get()));
        context3d_ = context3d.get();

        scoped_refptr<TestContextProvider> context_provider =
            TestContextProvider::Create(
                context3d.PassAs<TestWebGraphicsContext3D>());

        output_surface_ = FakeOutputSurface::Create3d(context_provider);
        break;
      }
      case ResourceProvider::Bitmap:
        output_surface_ = FakeOutputSurface::CreateSoftware(
            make_scoped_ptr(new SoftwareOutputDevice));
        break;
      case ResourceProvider::InvalidType:
        NOTREACHED();
        break;
    }
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), 0, false);
  }

  static void CollectResources(ReturnedResourceArray* array,
                               const ReturnedResourceArray& returned) {
    array->insert(array->end(), returned.begin(), returned.end());
  }

  static ReturnCallback GetReturnCallback(ReturnedResourceArray* array) {
    return base::Bind(&ResourceProviderTest::CollectResources, array);
  }

  static void SetResourceFilter(ResourceProvider* resource_provider,
                                ResourceProvider::ResourceId id,
                                WGC3Denum filter) {
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider, id, GL_TEXTURE_2D, filter);
  }

  ResourceProviderContext* context() { return context3d_; }

 protected:
  scoped_ptr<ContextSharedData> shared_data_;
  ResourceProviderContext* context3d_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
};

void CheckCreateResource(ResourceProvider::ResourceType expected_default_type,
                         ResourceProvider* resource_provider,
                         ResourceProviderContext* context) {
  DCHECK_EQ(expected_default_type, resource_provider->default_resource_type());

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  EXPECT_EQ(1, static_cast<int>(resource_provider->num_resources()));
  if (expected_default_type == ResourceProvider::GLTexture)
    EXPECT_EQ(0, context->texture_count());

  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());
  if (expected_default_type == ResourceProvider::GLTexture)
    EXPECT_EQ(1, context->texture_count());

  uint8_t result[4] = { 0 };
  GetResourcePixels(resource_provider, context, id, size, format, result);
  EXPECT_EQ(0, memcmp(data, result, pixel_size));

  resource_provider->DeleteResource(id);
  EXPECT_EQ(0, static_cast<int>(resource_provider->num_resources()));
  if (expected_default_type == ResourceProvider::GLTexture)
    EXPECT_EQ(0, context->texture_count());
}

TEST_P(ResourceProviderTest, Basic) {
  CheckCreateResource(GetParam(), resource_provider_.get(), context());
}

TEST_P(ResourceProviderTest, Upload) {
  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(16U, pixel_size);

  ResourceProvider::ResourceId id = resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  uint8_t image[16] = { 0 };
  gfx::Rect image_rect(size);
  resource_provider_->SetPixels(
      id, image, image_rect, image_rect, gfx::Vector2d());

  for (uint8_t i = 0; i < pixel_size; ++i)
    image[i] = i;

  uint8_t result[16] = { 0 };
  {
    gfx::Rect source_rect(0, 0, 1, 1);
    gfx::Vector2d dest_offset(0, 0);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect source_rect(0, 0, 1, 1);
    gfx::Vector2d dest_offset(1, 1);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect source_rect(1, 0, 1, 1);
    gfx::Vector2d dest_offset(0, 1);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 4, 5, 6, 7, 0, 1, 2, 3 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect offset_image_rect(gfx::Point(100, 100), size);
    gfx::Rect source_rect(100, 100, 1, 1);
    gfx::Vector2d dest_offset(1, 0);
    resource_provider_->SetPixels(
        id, image, offset_image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }

  resource_provider_->DeleteResource(id);
}

TEST_P(ResourceProviderTest, TransferResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));
  ResourceProviderContext* child_context = child_context_owned.get();

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(
      FakeOutputSurface::Create3d(
          child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = { 5, 5, 5, 5 };
  child_resource_provider->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));

  uint8_t result[4] = { 0 };
  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id1, size, format, result);
  EXPECT_EQ(0, memcmp(data1, result, pixel_size));

  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id2, size, format, result);
  EXPECT_EQ(0, memcmp(data2, result, pixel_size));
  {
    // Check that transfering again the same resource from the child to the
    // parent works.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    EXPECT_EQ(1u, list.size());
    EXPECT_EQ(id1, list[0].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    child_resource_provider->ReceiveReturnsFromParent(returned);
    // id1 was exported twice, we returned it only once, it should still be
    // in-use.
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
  }
  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(2u, returned_to_child.size());
    EXPECT_NE(0u, returned_to_child[0].sync_point);
    EXPECT_NE(0u, returned_to_child[1].sync_point);
    EXPECT_FALSE(returned_to_child[0].lost);
    EXPECT_FALSE(returned_to_child[1].lost);
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }
  EXPECT_FALSE(child_resource_provider->InUseByConsumer(id1));
  EXPECT_FALSE(child_resource_provider->InUseByConsumer(id2));

  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider.get(), id1);
    ASSERT_NE(0U, lock.texture_id());
    child_context->bindTexture(GL_TEXTURE_2D, lock.texture_id());
    child_context->GetPixels(size, format, result);
    EXPECT_EQ(0, memcmp(data1, result, pixel_size));
  }
  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider.get(), id2);
    ASSERT_NE(0U, lock.texture_id());
    child_context->bindTexture(GL_TEXTURE_2D, lock.texture_id());
    child_context->GetPixels(size, format, result);
    EXPECT_EQ(0, memcmp(data2, result, pixel_size));
  }
  {
    // Transfer resources to the parent again.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(0u, returned_to_child.size());

  EXPECT_EQ(2u, resource_provider_->num_resources());
  resource_provider_->DestroyChild(child_id);
  EXPECT_EQ(0u, resource_provider_->num_resources());

  ASSERT_EQ(2u, returned_to_child.size());
  EXPECT_NE(0u, returned_to_child[0].sync_point);
  EXPECT_NE(0u, returned_to_child[1].sync_point);
  EXPECT_FALSE(returned_to_child[0].lost);
  EXPECT_FALSE(returned_to_child[1].lost);
}

TEST_P(ResourceProviderTest, DeleteExportedResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = {1, 2, 3, 4};
  gfx::Rect rect(size);
  child_resource_provider->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = {5, 5, 5, 5};
  child_resource_provider->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));

  {
    // The parent transfers the resources to the grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id1);
    resource_ids_to_transfer.push_back(mapped_id2);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id2));

    // Release the resource in the parent. Set no resources as being in use. The
    // resources are exported so that can't be transferred back yet.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    EXPECT_EQ(0u, returned_to_child.size());
    EXPECT_EQ(2u, resource_provider_->num_resources());

    // Return the resources from the grandparent to the parent. They should be
    // returned to the child then.
    EXPECT_EQ(2u, list.size());
    EXPECT_EQ(mapped_id1, list[0].id);
    EXPECT_EQ(mapped_id2, list[1].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);

    EXPECT_EQ(0u, resource_provider_->num_resources());
    ASSERT_EQ(2u, returned_to_child.size());
    EXPECT_NE(0u, returned_to_child[0].sync_point);
    EXPECT_NE(0u, returned_to_child[1].sync_point);
    EXPECT_FALSE(returned_to_child[0].lost);
    EXPECT_FALSE(returned_to_child[1].lost);
  }
}

TEST_P(ResourceProviderTest, DestroyChildWithExportedResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = {1, 2, 3, 4};
  gfx::Rect rect(size);
  child_resource_provider->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = {5, 5, 5, 5};
  child_resource_provider->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));

  {
    // The parent transfers the resources to the grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id1);
    resource_ids_to_transfer.push_back(mapped_id2);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id2));

    // Release the resource in the parent. Set no resources as being in use. The
    // resources are exported so that can't be transferred back yet.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Destroy the child, the resources should be returned immediately from the
    // parent and marked as lost.
    EXPECT_EQ(0u, returned_to_child.size());
    EXPECT_EQ(2u, resource_provider_->num_resources());

    resource_provider_->DestroyChild(child_id);

    EXPECT_EQ(0u, resource_provider_->num_resources());
    ASSERT_EQ(2u, returned_to_child.size());
    EXPECT_NE(0u, returned_to_child[0].sync_point);
    EXPECT_NE(0u, returned_to_child[1].sync_point);
    EXPECT_TRUE(returned_to_child[0].lost);
    EXPECT_TRUE(returned_to_child[1].lost);
    returned_to_child.clear();

    // Return the resources from the grandparent to the parent. They should be
    // dropped on the floor since they were already returned to the child.
    EXPECT_EQ(2u, list.size());
    EXPECT_EQ(mapped_id1, list[0].id);
    EXPECT_EQ(mapped_id2, list[1].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);

    EXPECT_EQ(0u, returned_to_child.size());
  }
}

TEST_P(ResourceProviderTest, DeleteTransferredResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(
      FakeOutputSurface::Create3d(
          child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  // Delete textures in the child, while they are transfered.
  child_resource_provider->DeleteResource(id);
  EXPECT_EQ(1u, child_resource_provider->num_resources());
  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_NE(0u, returned_to_child[0].sync_point);
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
  }
  EXPECT_EQ(0u, child_resource_provider->num_resources());
}

class ResourceProviderTestTextureFilters : public ResourceProviderTest {
 public:
  static void RunTest(GLenum child_filter, GLenum parent_filter) {
    scoped_ptr<TextureStateTrackingContext> child_context_owned(
        new TextureStateTrackingContext);
    TextureStateTrackingContext* child_context = child_context_owned.get();

    FakeOutputSurfaceClient child_output_surface_client;
    scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
        child_context_owned.PassAs<TestWebGraphicsContext3D>()));
    CHECK(child_output_surface->BindToClient(&child_output_surface_client));

    scoped_ptr<ResourceProvider> child_resource_provider(
        ResourceProvider::Create(child_output_surface.get(), 0, false));

    scoped_ptr<TextureStateTrackingContext> parent_context_owned(
        new TextureStateTrackingContext);
    TextureStateTrackingContext* parent_context = parent_context_owned.get();

    FakeOutputSurfaceClient parent_output_surface_client;
    scoped_ptr<OutputSurface> parent_output_surface(FakeOutputSurface::Create3d(
        parent_context_owned.PassAs<TestWebGraphicsContext3D>()));
    CHECK(parent_output_surface->BindToClient(&parent_output_surface_client));

    scoped_ptr<ResourceProvider> parent_resource_provider(
        ResourceProvider::Create(parent_output_surface.get(), 0, false));

    gfx::Size size(1, 1);
    ResourceFormat format = RGBA_8888;
    int texture_id = 1;

    size_t pixel_size = TextureSize(size, format);
    ASSERT_EQ(4U, pixel_size);

    ResourceProvider::ResourceId id = child_resource_provider->CreateResource(
        size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

    // The new texture is created with GL_LINEAR.
    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, texture_id))
        .Times(2);  // Once to create and once to allocate.
    EXPECT_CALL(*child_context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*child_context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(
        *child_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(
        *child_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(*child_context,
                texParameteri(GL_TEXTURE_2D,
                              GL_TEXTURE_POOL_CHROMIUM,
                              GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));
    child_resource_provider->AllocateForTesting(id);
    Mock::VerifyAndClearExpectations(child_context);

    uint8_t data[4] = { 1, 2, 3, 4 };
    gfx::Rect rect(size);

    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, texture_id));
    child_resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());
    Mock::VerifyAndClearExpectations(child_context);

    // The texture is set to |child_filter| in the child.
    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, texture_id));
    if (child_filter != GL_LINEAR) {
      EXPECT_CALL(
          *child_context,
          texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, child_filter));
      EXPECT_CALL(
          *child_context,
          texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, child_filter));
    }
    SetResourceFilter(child_resource_provider.get(), id, child_filter);
    Mock::VerifyAndClearExpectations(child_context);

    ReturnedResourceArray returned_to_child;
    int child_id = parent_resource_provider->CreateChild(
        GetReturnCallback(&returned_to_child));
    {
      // Transfer some resource to the parent.
      ResourceProvider::ResourceIdArray resource_ids_to_transfer;
      resource_ids_to_transfer.push_back(id);
      TransferableResourceArray list;

      EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, texture_id));
      EXPECT_CALL(*child_context,
                  produceTextureCHROMIUM(GL_TEXTURE_2D, _));
      EXPECT_CALL(*child_context, insertSyncPoint());
      child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                   &list);
      Mock::VerifyAndClearExpectations(child_context);

      ASSERT_EQ(1u, list.size());
      EXPECT_EQ(static_cast<unsigned>(child_filter), list[0].filter);

      EXPECT_CALL(*parent_context, bindTexture(GL_TEXTURE_2D, texture_id));
      EXPECT_CALL(*parent_context,
                  consumeTextureCHROMIUM(GL_TEXTURE_2D, _));
      parent_resource_provider->ReceiveFromChild(child_id, list);
      Mock::VerifyAndClearExpectations(parent_context);

      parent_resource_provider->DeclareUsedResourcesFromChild(
          child_id, resource_ids_to_transfer);
      Mock::VerifyAndClearExpectations(parent_context);
    }
    ResourceProvider::ResourceIdMap resource_map =
        parent_resource_provider->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId mapped_id = resource_map[id];
    EXPECT_NE(0u, mapped_id);

    // The texture is set to |parent_filter| in the parent.
    EXPECT_CALL(*parent_context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, parent_filter));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, parent_filter));
    SetResourceFilter(parent_resource_provider.get(), mapped_id, parent_filter);
    Mock::VerifyAndClearExpectations(parent_context);

    // The texture should be reset to |child_filter| in the parent when it is
    // returned, since that is how it was received.
    EXPECT_CALL(*parent_context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, child_filter));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, child_filter));

    {
      EXPECT_EQ(0u, returned_to_child.size());

      // Transfer resources back from the parent to the child. Set no resources
      // as being in use.
      ResourceProvider::ResourceIdArray no_resources;
      EXPECT_CALL(*parent_context, insertSyncPoint());
      parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                              no_resources);
      Mock::VerifyAndClearExpectations(parent_context);

      ASSERT_EQ(1u, returned_to_child.size());
      child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    }

    // The child remembers the texture filter is set to |child_filter|.
    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, texture_id));
    SetResourceFilter(child_resource_provider.get(), id, child_filter);
    Mock::VerifyAndClearExpectations(child_context);
  }
};

TEST_P(ResourceProviderTest, TextureFilters_ChildNearestParentLinear) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  ResourceProviderTestTextureFilters::RunTest(GL_NEAREST, GL_LINEAR);
}

TEST_P(ResourceProviderTest, TextureFilters_ChildLinearParentNearest) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  ResourceProviderTestTextureFilters::RunTest(GL_LINEAR, GL_NEAREST);
}

void ReleaseTextureMailbox(unsigned* release_sync_point,
                           bool* release_lost_resource,
                           unsigned sync_point,
                           bool lost_resource) {
  *release_sync_point = sync_point;
  *release_lost_resource = lost_resource;
}

TEST_P(ResourceProviderTest, TransferMailboxResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  uint8_t data[4] = { 1, 2, 3, 4 };
  context()->texImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  unsigned sync_point = context()->insertSyncPoint();

  // All the logic below assumes that the sync points are all positive.
  EXPECT_LT(0u, sync_point);

  unsigned release_sync_point = 0;
  bool lost_resource = false;
  ReleaseCallback callback =
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource);
  ResourceProvider::ResourceId resource =
      resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(mailbox, sync_point),
          SingleReleaseCallback::Create(callback));
  EXPECT_EQ(1, context()->texture_count());
  EXPECT_EQ(0u, release_sync_point);
  {
    // Transfer the resource, expect the sync points to be consistent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_point, list[0].sync_point);
    EXPECT_EQ(0,
              memcmp(mailbox.name, list[0].mailbox.name, sizeof(mailbox.name)));
    EXPECT_EQ(0u, release_sync_point);

    context()->waitSyncPoint(list[0].sync_point);
    unsigned other_texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, other_texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(
        gfx::Size(1, 1), RGBA_8888, test_data);
    EXPECT_EQ(0, memcmp(data, test_data, sizeof(data)));
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(other_texture);
    list[0].sync_point = context()->insertSyncPoint();
    EXPECT_LT(0u, list[0].sync_point);

    // Receive the resource, then delete it, expect the sync points to be
    // consistent.
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);
    EXPECT_EQ(1, context()->texture_count());
    EXPECT_EQ(0u, release_sync_point);

    resource_provider_->DeleteResource(resource);
    EXPECT_LE(list[0].sync_point, release_sync_point);
    EXPECT_FALSE(lost_resource);
  }

  // We're going to do the same thing as above, but testing the case where we
  // delete the resource before we receive it back.
  sync_point = release_sync_point;
  EXPECT_LT(0u, sync_point);
  release_sync_point = 0;
  resource = resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(mailbox, sync_point),
      SingleReleaseCallback::Create(callback));
  EXPECT_EQ(1, context()->texture_count());
  EXPECT_EQ(0u, release_sync_point);
  {
    // Transfer the resource, expect the sync points to be consistent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_point, list[0].sync_point);
    EXPECT_EQ(0,
              memcmp(mailbox.name, list[0].mailbox.name, sizeof(mailbox.name)));
    EXPECT_EQ(0u, release_sync_point);

    context()->waitSyncPoint(list[0].sync_point);
    unsigned other_texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, other_texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(
        gfx::Size(1, 1), RGBA_8888, test_data);
    EXPECT_EQ(0, memcmp(data, test_data, sizeof(data)));
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(other_texture);
    list[0].sync_point = context()->insertSyncPoint();
    EXPECT_LT(0u, list[0].sync_point);

    // Delete the resource, which shouldn't do anything.
    resource_provider_->DeleteResource(resource);
    EXPECT_EQ(1, context()->texture_count());
    EXPECT_EQ(0u, release_sync_point);

    // Then receive the resource which should release the mailbox, expect the
    // sync points to be consistent.
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);
    EXPECT_LE(list[0].sync_point, release_sync_point);
    EXPECT_FALSE(lost_resource);
  }

  context()->waitSyncPoint(release_sync_point);
  context()->bindTexture(GL_TEXTURE_2D, texture);
  context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  context()->deleteTexture(texture);
}

TEST_P(ResourceProviderTest, LostResourceInParent) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId resource =
      child_resource_provider->CreateResource(
          size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  child_resource_provider->AllocateForTesting(resource);

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  // Lose the output surface in the parent.
  resource_provider_->DidLoseOutputSurface();

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect the resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_TRUE(returned_to_child[0].lost);
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // The resource should be lost.
  EXPECT_TRUE(child_resource_provider->IsLost(resource));

  // Lost resources stay in use in the parent forever.
  EXPECT_TRUE(child_resource_provider->InUseByConsumer(resource));
}

TEST_P(ResourceProviderTest, LostResourceInGrandParent) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId resource =
      child_resource_provider->CreateResource(
          size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  child_resource_provider->AllocateForTesting(resource);

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  {
    ResourceProvider::ResourceIdMap resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId parent_resource = resource_map[resource];
    EXPECT_NE(0u, parent_resource);

    // Transfer to a grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(parent_resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    // Receive back a lost resource from the grandparent.
    EXPECT_EQ(1u, list.size());
    EXPECT_EQ(parent_resource, list[0].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    EXPECT_EQ(1u, returned.size());
    EXPECT_EQ(parent_resource, returned[0].id);
    returned[0].lost = true;
    resource_provider_->ReceiveReturnsFromParent(returned);

    // The resource should be lost.
    EXPECT_TRUE(resource_provider_->IsLost(parent_resource));

    // Lost resources stay in use in the parent forever.
    EXPECT_TRUE(resource_provider_->InUseByConsumer(parent_resource));
  }

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect the resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_TRUE(returned_to_child[0].lost);
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // The resource should be lost.
  EXPECT_TRUE(child_resource_provider->IsLost(resource));

  // Lost resources stay in use in the parent forever.
  EXPECT_TRUE(child_resource_provider->InUseByConsumer(resource));
}

TEST_P(ResourceProviderTest, LostMailboxInParent) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));
  ResourceProviderContext* child_context = child_context_owned.get();

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  unsigned texture = child_context->createTexture();
  gpu::Mailbox gpu_mailbox;
  child_context->bindTexture(GL_TEXTURE_2D, texture);
  child_context->genMailboxCHROMIUM(gpu_mailbox.name);
  child_context->produceTextureCHROMIUM(GL_TEXTURE_2D, gpu_mailbox.name);

  unsigned release_sync_point = 0;
  bool lost_resource = false;
  ReleaseCallback callback =
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource);
  ResourceProvider::ResourceId resource =
      child_resource_provider->CreateResourceFromTextureMailbox(
          TextureMailbox(gpu_mailbox), SingleReleaseCallback::Create(callback));

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  // Lose the output surface in the parent.
  resource_provider_->DidLoseOutputSurface();

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect the resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_TRUE(returned_to_child[0].lost);
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // Delete the resource in the child. Expect the resource to be lost.
  child_resource_provider->DeleteResource(resource);
  EXPECT_TRUE(lost_resource);

  child_context->waitSyncPoint(release_sync_point);
  child_context->deleteTexture(texture);
}

TEST_P(ResourceProviderTest, LostMailboxInGrandParent) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));
  ResourceProviderContext* child_context = child_context_owned.get();

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get(), 0, false));

  unsigned texture = child_context->createTexture();
  gpu::Mailbox gpu_mailbox;
  child_context->bindTexture(GL_TEXTURE_2D, texture);
  child_context->genMailboxCHROMIUM(gpu_mailbox.name);
  child_context->produceTextureCHROMIUM(GL_TEXTURE_2D, gpu_mailbox.name);

  unsigned release_sync_point = 0;
  bool lost_resource = false;
  ReleaseCallback callback =
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource);
  ResourceProvider::ResourceId resource =
      child_resource_provider->CreateResourceFromTextureMailbox(
          TextureMailbox(gpu_mailbox), SingleReleaseCallback::Create(callback));

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  {
    ResourceProvider::ResourceIdMap resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId parent_resource = resource_map[resource];
    EXPECT_NE(0u, parent_resource);

    // Transfer to a grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(parent_resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    // Receive back a lost resource from the grandparent.
    EXPECT_EQ(1u, list.size());
    EXPECT_EQ(parent_resource, list[0].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    EXPECT_EQ(1u, returned.size());
    EXPECT_EQ(parent_resource, returned[0].id);
    returned[0].lost = true;
    resource_provider_->ReceiveReturnsFromParent(returned);
  }

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect the resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_TRUE(returned_to_child[0].lost);
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // Delete the resource in the child. Expect the resource to be lost.
  child_resource_provider->DeleteResource(resource);
  EXPECT_TRUE(lost_resource);

  child_context->waitSyncPoint(release_sync_point);
  child_context->deleteTexture(texture);
}

TEST_P(ResourceProviderTest, Shutdown) {
  // TextureMailbox callbacks only exist for GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  unsigned sync_point = context()->insertSyncPoint();

  EXPECT_LT(0u, sync_point);

  unsigned release_sync_point = 0;
  bool lost_resource = false;
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource));
  resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(mailbox, sync_point),
      callback.Pass());

  EXPECT_EQ(0u, release_sync_point);
  EXPECT_FALSE(lost_resource);

  resource_provider_.reset();

  EXPECT_LE(sync_point, release_sync_point);
  EXPECT_FALSE(lost_resource);
}

static scoped_ptr<base::SharedMemory> CreateAndFillSharedMemory(
    gfx::Size size, uint32_t value) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  CHECK(shared_memory->CreateAndMapAnonymous(4 * size.GetArea()));
  uint32_t* pixels = reinterpret_cast<uint32_t*>(shared_memory->memory());
  CHECK(pixels);
  std::fill_n(pixels, size.GetArea(), value);
  return shared_memory.Pass();
}

static void ReleaseSharedMemoryCallback(
    bool* release_called,
    unsigned sync_point, bool lost_resource) {
  *release_called = true;
}

TEST_P(ResourceProviderTest, ShutdownSharedMemory) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;

  gfx::Size size(64, 64);
  scoped_ptr<base::SharedMemory> shared_memory(
      CreateAndFillSharedMemory(size, 0));

  bool release_called = false;
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(ReleaseSharedMemoryCallback, &release_called));
  resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(shared_memory.get(), size),
      callback.Pass());

  resource_provider_.reset();

  EXPECT_TRUE(release_called);
}

TEST_P(ResourceProviderTest, ShutdownWithExportedResource) {
  // TextureMailbox callbacks only exist for GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  unsigned sync_point = context()->insertSyncPoint();

  EXPECT_LT(0u, sync_point);

  unsigned release_sync_point = 0;
  bool lost_resource = false;
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource));
  ResourceProvider::ResourceId resource =
      resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(mailbox, sync_point),
          callback.Pass());

  // Transfer the resource, so we can't release it properly on shutdown.
  ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource);
  TransferableResourceArray list;
  resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

  EXPECT_EQ(0u, release_sync_point);
  EXPECT_FALSE(lost_resource);

  resource_provider_.reset();

  // Since the resource is in the parent, the child considers it lost.
  EXPECT_EQ(0u, release_sync_point);
  EXPECT_TRUE(lost_resource);
}

TEST_P(ResourceProviderTest, LostContext) {
  // TextureMailbox callbacks only exist for GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  unsigned sync_point = context()->insertSyncPoint();

  EXPECT_LT(0u, sync_point);

  unsigned release_sync_point = 0;
  bool lost_resource = false;
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource));
  resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(mailbox, sync_point),
      callback.Pass());

  EXPECT_EQ(0u, release_sync_point);
  EXPECT_FALSE(lost_resource);

  resource_provider_->DidLoseOutputSurface();
  resource_provider_.reset();

  EXPECT_LE(sync_point, release_sync_point);
  EXPECT_TRUE(lost_resource);
}

TEST_P(ResourceProviderTest, ScopedSampler) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  int texture_id = 1;

  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  // Check that the texture gets created with the right sampler settings.
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id))
      .Times(2);  // Once to create and once to allocate.
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_POOL_CHROMIUM,
                            GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));

  resource_provider->AllocateForTesting(id);
  Mock::VerifyAndClearExpectations(context);

  // Creating a sampler with the default filter should not change any texture
  // parameters.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    Mock::VerifyAndClearExpectations(context);
  }

  // Using a different filter should be reflected in the texture parameters.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_NEAREST);
    Mock::VerifyAndClearExpectations(context);
  }

  // Test resetting to the default filter.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    Mock::VerifyAndClearExpectations(context);
  }
}

TEST_P(ResourceProviderTest, ManagedResource) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  int texture_id = 1;

  // Check that the texture gets created with the right sampler settings.
  ResourceProvider::ResourceId id = resource_provider->CreateManagedResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_POOL_CHROMIUM,
                            GL_TEXTURE_POOL_MANAGED_CHROMIUM));
  resource_provider->CreateForTesting(id);
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, TextureWrapMode) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  int texture_id = 1;
  GLenum texture_pool = GL_TEXTURE_POOL_UNMANAGED_CHROMIUM;

  for (int i = 0; i < 2; ++i) {
    GLint wrap_mode = i ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    // Check that the texture gets created with the right sampler settings.
    ResourceProvider::ResourceId id =
        resource_provider->CreateGLTexture(size,
                                           texture_pool,
                                           wrap_mode,
                                           ResourceProvider::TextureUsageAny,
                                           format);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D,
                              GL_TEXTURE_POOL_CHROMIUM,
                              GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));
    resource_provider->CreateForTesting(id);
    EXPECT_NE(0u, id);

    Mock::VerifyAndClearExpectations(context);
  }
}

static void EmptyReleaseCallback(unsigned sync_point, bool lost_resource) {}

TEST_P(ResourceProviderTest, TextureMailbox_SharedMemory) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;

  gfx::Size size(64, 64);
  const uint32_t kBadBeef = 0xbadbeef;
  scoped_ptr<base::SharedMemory> shared_memory(
      CreateAndFillSharedMemory(size, kBadBeef));

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateSoftware(make_scoped_ptr(
          new SoftwareOutputDevice)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(&EmptyReleaseCallback));
  TextureMailbox mailbox(shared_memory.get(), size);

  ResourceProvider::ResourceId id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, callback.Pass());
  EXPECT_NE(0u, id);

  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider.get(), id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(16, 16), kBadBeef);
  }
}

TEST_P(ResourceProviderTest, TextureMailbox_GLTexture2D) {
  // Mailboxing is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  unsigned texture_id = 1;
  unsigned sync_point = 30;
  unsigned target = GL_TEXTURE_2D;

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
  EXPECT_CALL(*context, insertSyncPoint()).Times(0);
  EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);

  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(&EmptyReleaseCallback));

  TextureMailbox mailbox(gpu_mailbox, sync_point);

  ResourceProvider::ResourceId id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, callback.Pass());
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);

  {
    // Using the texture does a consume of the mailbox.
    EXPECT_CALL(*context, bindTexture(target, texture_id));
    EXPECT_CALL(*context, waitSyncPoint(sync_point));
    EXPECT_CALL(*context, consumeTextureCHROMIUM(target, _));

    EXPECT_CALL(*context, insertSyncPoint()).Times(0);
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    ResourceProvider::ScopedReadLockGL lock(resource_provider.get(), id);
    Mock::VerifyAndClearExpectations(context);

    // When done with it, a sync point should be inserted, but no produce is
    // necessary.
    EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
    EXPECT_CALL(*context, insertSyncPoint());
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
    EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);
  }
}

TEST_P(ResourceProviderTest, TextureMailbox_GLTextureExternalOES) {
  // Mailboxing is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  unsigned texture_id = 1;
  unsigned sync_point = 30;
  unsigned target = GL_TEXTURE_EXTERNAL_OES;

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
  EXPECT_CALL(*context, insertSyncPoint()).Times(0);
  EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);

  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(&EmptyReleaseCallback));

  TextureMailbox mailbox(gpu_mailbox, target, sync_point);

  ResourceProvider::ResourceId id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, callback.Pass());
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);

  {
    // Using the texture does a consume of the mailbox.
    EXPECT_CALL(*context, bindTexture(target, texture_id));
    EXPECT_CALL(*context, waitSyncPoint(sync_point));
    EXPECT_CALL(*context, consumeTextureCHROMIUM(target, _));

    EXPECT_CALL(*context, insertSyncPoint()).Times(0);
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    ResourceProvider::ScopedReadLockGL lock(resource_provider.get(), id);
    Mock::VerifyAndClearExpectations(context);

    // When done with it, a sync point should be inserted, but no produce is
    // necessary.
    EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
    EXPECT_CALL(*context, insertSyncPoint());
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
    EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);
  }
}

class AllocationTrackingContext3D : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD0(createTexture, WebGLId());
  MOCK_METHOD1(deleteTexture, void(WebGLId texture_id));
  MOCK_METHOD2(bindTexture, void(WGC3Denum target, WebGLId texture));
  MOCK_METHOD9(texImage2D,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Denum internalformat,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Dint border,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD9(texSubImage2D,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Dint xoffset,
                    WGC3Dint yoffset,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD9(asyncTexImage2DCHROMIUM,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Denum internalformat,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Dint border,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD9(asyncTexSubImage2DCHROMIUM,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Dint xoffset,
                    WGC3Dint yoffset,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD1(waitAsyncTexImage2DCHROMIUM, void(WGC3Denum));
  MOCK_METHOD3(createImageCHROMIUM, WGC3Duint(WGC3Dsizei, WGC3Dsizei,
                                              WGC3Denum));
  MOCK_METHOD1(destroyImageCHROMIUM, void(WGC3Duint));
  MOCK_METHOD2(mapImageCHROMIUM, void*(WGC3Duint, WGC3Denum));
  MOCK_METHOD3(getImageParameterivCHROMIUM, void(WGC3Duint, WGC3Denum,
                                                 GLint*));
  MOCK_METHOD1(unmapImageCHROMIUM, void(WGC3Duint));
  MOCK_METHOD2(bindTexImage2DCHROMIUM, void(WGC3Denum, WGC3Dint));
  MOCK_METHOD2(releaseTexImage2DCHROMIUM, void(WGC3Denum, WGC3Dint));
};

TEST_P(ResourceProviderTest, TextureAllocation) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  gfx::Size size(2, 2);
  gfx::Vector2d offset(0, 0);
  gfx::Rect rect(0, 0, 2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  uint8_t pixels[16] = { 0 };
  int texture_id = 123;

  // Lazy allocation. Don't allocate when creating the resource.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(1);
  resource_provider->CreateForTesting(id);

  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);

  // Do allocate when we set the pixels.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(3);
  EXPECT_CALL(*context, texImage2D(_, _, _, 2, 2, _, _, _, _)).Times(1);
  EXPECT_CALL(*context, texSubImage2D(_, _, _, _, 2, 2, _, _, _)).Times(1);
  resource_provider->SetPixels(id, pixels, rect, rect, offset);

  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);

  // Same for async version.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelBuffer(id);

  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  resource_provider->BeginSetPixels(id);
  ASSERT_TRUE(resource_provider->DidSetPixelsComplete(id));

  resource_provider->ReleasePixelBuffer(id);

  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, PixelBuffer_GLTexture) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelBuffer(id);

  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  resource_provider->BeginSetPixels(id);

  EXPECT_TRUE(resource_provider->DidSetPixelsComplete(id));

  resource_provider->ReleasePixelBuffer(id);

  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, PixelBuffer_Bitmap) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateSoftware(make_scoped_ptr(
          new SoftwareOutputDevice)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  const uint32_t kBadBeef = 0xbadbeef;

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelBuffer(id);

  void* data = resource_provider->MapPixelBuffer(id);
  ASSERT_TRUE(!!data);
  memcpy(data, &kBadBeef, sizeof(kBadBeef));
  resource_provider->UnmapPixelBuffer(id);

  resource_provider->BeginSetPixels(id);
  EXPECT_TRUE(resource_provider->DidSetPixelsComplete(id));

  resource_provider->ReleasePixelBuffer(id);

  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider.get(), id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(0, 0), kBadBeef);
  }

  resource_provider->DeleteResource(id);
}

TEST_P(ResourceProviderTest, ForcingAsyncUploadToComplete) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelBuffer(id);

  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  resource_provider->BeginSetPixels(id);

  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(1);
  EXPECT_CALL(*context, waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, 0)).Times(1);
  resource_provider->ForceSetPixelsToComplete(id);

  resource_provider->ReleasePixelBuffer(id);

  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, PixelBufferLostContext) {
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new NiceMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  EXPECT_CALL(*context, createTexture()).WillRepeatedly(Return(texture_id));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  context->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                               GL_INNOCENT_CONTEXT_RESET_ARB);
  resource_provider->AcquirePixelBuffer(id);
  uint8_t* buffer = resource_provider->MapPixelBuffer(id);
  EXPECT_TRUE(buffer == NULL);
  resource_provider->UnmapPixelBuffer(id);
  resource_provider->ReleasePixelBuffer(id);
  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, Image_GLTexture) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  const int kWidth = 2;
  const int kHeight = 2;
  gfx::Size size(kWidth, kHeight);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  const unsigned kTextureId = 123u;
  const unsigned kImageId = 234u;

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  EXPECT_CALL(*context, createImageCHROMIUM(kWidth, kHeight, GL_RGBA8_OES))
      .WillOnce(Return(kImageId))
      .RetiresOnSaturation();
  resource_provider->AcquireImage(id);

  void* dummy_mapped_buffer_address = NULL;
  EXPECT_CALL(*context, mapImageCHROMIUM(kImageId, GL_READ_WRITE))
      .WillOnce(Return(dummy_mapped_buffer_address))
      .RetiresOnSaturation();
  resource_provider->MapImage(id);

  const int kStride = 4;
  EXPECT_CALL(*context, getImageParameterivCHROMIUM(kImageId,
                                                    GL_IMAGE_ROWBYTES_CHROMIUM,
                                                    _))
      .WillOnce(SetArgPointee<2>(kStride))
      .RetiresOnSaturation();
  int stride = resource_provider->GetImageStride(id);
  EXPECT_EQ(kStride, stride);

  EXPECT_CALL(*context, unmapImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->UnmapImage(id);

  EXPECT_CALL(*context, createTexture())
      .WillOnce(Return(kTextureId))
      .RetiresOnSaturation();
  // Once in CreateTextureId and once in BindForSampling
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, kTextureId)).Times(2)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, bindTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, releaseTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, deleteTexture(kTextureId))
      .Times(1)
      .RetiresOnSaturation();
  {
    ResourceProvider::ScopedSamplerGL lock_gl(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    EXPECT_EQ(kTextureId, lock_gl.texture_id());
  }

  EXPECT_CALL(*context, destroyImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->ReleaseImage(id);
}

TEST_P(ResourceProviderTest, Image_Bitmap) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateSoftware(make_scoped_ptr(
          new SoftwareOutputDevice)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  const uint32_t kBadBeef = 0xbadbeef;

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquireImage(id);

  const int kStride = 0;
  int stride = resource_provider->GetImageStride(id);
  EXPECT_EQ(kStride, stride);

  void* data = resource_provider->MapImage(id);
  ASSERT_TRUE(!!data);
  memcpy(data, &kBadBeef, sizeof(kBadBeef));
  resource_provider->UnmapImage(id);

  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider.get(), id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(0, 0), kBadBeef);
  }

  resource_provider->ReleaseImage(id);
  resource_provider->DeleteResource(id);
}

void InitializeGLAndCheck(ContextSharedData* shared_data,
                          ResourceProvider* resource_provider,
                          FakeOutputSurface* output_surface) {
  scoped_ptr<ResourceProviderContext> context_owned =
      ResourceProviderContext::Create(shared_data);
  ResourceProviderContext* context = context_owned.get();

  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create(
          context_owned.PassAs<TestWebGraphicsContext3D>());
  output_surface->InitializeAndSetContext3d(context_provider, NULL);
  EXPECT_TRUE(resource_provider->InitializeGL());

  CheckCreateResource(ResourceProvider::GLTexture, resource_provider, context);
}

TEST(ResourceProviderTest, BasicInitializeGLSoftware) {
  scoped_ptr<ContextSharedData> shared_data = ContextSharedData::Create();
  FakeOutputSurfaceClient client;
  scoped_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::CreateDeferredGL(
          scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice)));
  EXPECT_TRUE(output_surface->BindToClient(&client));
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0, false));

  CheckCreateResource(ResourceProvider::Bitmap, resource_provider.get(), NULL);

  InitializeGLAndCheck(shared_data.get(),
                       resource_provider.get(),
                       output_surface.get());

  resource_provider->InitializeSoftware();
  output_surface->ReleaseGL();
  CheckCreateResource(ResourceProvider::Bitmap, resource_provider.get(), NULL);

  InitializeGLAndCheck(shared_data.get(),
                       resource_provider.get(),
                       output_surface.get());
}

INSTANTIATE_TEST_CASE_P(
    ResourceProviderTests,
    ResourceProviderTest,
    ::testing::Values(ResourceProvider::GLTexture, ResourceProvider::Bitmap));

}  // namespace
}  // namespace cc
