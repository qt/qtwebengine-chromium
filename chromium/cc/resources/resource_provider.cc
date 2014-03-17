// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <algorithm>
#include <limits>

#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cc/base/util.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "cc/resources/platform_color.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/resources/transferable_resource.h"
#include "cc/scheduler/texture_uploader.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

using gpu::gles2::GLES2Interface;

namespace cc {

class IdAllocator {
 public:
  virtual ~IdAllocator() {}

  virtual GLuint NextId() = 0;

 protected:
  IdAllocator(GLES2Interface* gl, size_t id_allocation_chunk_size)
      : gl_(gl),
        id_allocation_chunk_size_(id_allocation_chunk_size),
        ids_(new GLuint[id_allocation_chunk_size]),
        next_id_index_(id_allocation_chunk_size) {
    DCHECK(id_allocation_chunk_size_);
  }

  GLES2Interface* gl_;
  const size_t id_allocation_chunk_size_;
  scoped_ptr<GLuint[]> ids_;
  size_t next_id_index_;
};

namespace {

// Measured in seconds.
const double kSoftwareUploadTickRate = 0.000250;
const double kTextureUploadTickRate = 0.004;

GLenum TextureToStorageFormat(ResourceFormat format) {
  GLenum storage_format = GL_RGBA8_OES;
  switch (format) {
    case RGBA_8888:
      break;
    case BGRA_8888:
      storage_format = GL_BGRA8_EXT;
      break;
    case RGBA_4444:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
      NOTREACHED();
      break;
  }

  return storage_format;
}

bool IsFormatSupportedForStorage(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
    case BGRA_8888:
      return true;
    case RGBA_4444:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
      return false;
  }
  return false;
}

class ScopedSetActiveTexture {
 public:
  ScopedSetActiveTexture(GLES2Interface* gl, GLenum unit)
      : gl_(gl), unit_(unit) {
    DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(gl_));

    if (unit_ != GL_TEXTURE0)
      GLC(gl_, gl_->ActiveTexture(unit_));
  }

  ~ScopedSetActiveTexture() {
    // Active unit being GL_TEXTURE0 is effectively the ground state.
    if (unit_ != GL_TEXTURE0)
      GLC(gl_, gl_->ActiveTexture(GL_TEXTURE0));
  }

 private:
  GLES2Interface* gl_;
  GLenum unit_;
};

class TextureIdAllocator : public IdAllocator {
 public:
  TextureIdAllocator(GLES2Interface* gl,
                     size_t texture_id_allocation_chunk_size)
      : IdAllocator(gl, texture_id_allocation_chunk_size) {}
  virtual ~TextureIdAllocator() {
    gl_->DeleteTextures(id_allocation_chunk_size_ - next_id_index_,
                        ids_.get() + next_id_index_);
  }

  // Overridden from IdAllocator:
  virtual GLuint NextId() OVERRIDE {
    if (next_id_index_ == id_allocation_chunk_size_) {
      gl_->GenTextures(id_allocation_chunk_size_, ids_.get());
      next_id_index_ = 0;
    }

    return ids_[next_id_index_++];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TextureIdAllocator);
};

class BufferIdAllocator : public IdAllocator {
 public:
  BufferIdAllocator(GLES2Interface* gl, size_t buffer_id_allocation_chunk_size)
      : IdAllocator(gl, buffer_id_allocation_chunk_size) {}
  virtual ~BufferIdAllocator() {
    gl_->DeleteBuffers(id_allocation_chunk_size_ - next_id_index_,
                       ids_.get() + next_id_index_);
  }

  // Overridden from IdAllocator:
  virtual GLuint NextId() OVERRIDE {
    if (next_id_index_ == id_allocation_chunk_size_) {
      gl_->GenBuffers(id_allocation_chunk_size_, ids_.get());
      next_id_index_ = 0;
    }

    return ids_[next_id_index_++];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferIdAllocator);
};

}  // namespace

ResourceProvider::Resource::Resource()
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      pixels(NULL),
      pixel_buffer(NULL),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      locked_for_write(false),
      external(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      read_lock_fence(NULL),
      size(),
      target(0),
      original_filter(0),
      filter(0),
      image_id(0),
      bound_image_id(0),
      dirty_image(false),
      texture_pool(0),
      wrap_mode(0),
      lost(false),
      hint(TextureUsageAny),
      type(static_cast<ResourceType>(0)),
      format(RGBA_8888),
      shared_bitmap(NULL) {}

ResourceProvider::Resource::~Resource() {}

ResourceProvider::Resource::Resource(GLuint texture_id,
                                     gfx::Size size,
                                     GLenum target,
                                     GLenum filter,
                                     GLenum texture_pool,
                                     GLint wrap_mode,
                                     TextureUsageHint hint,
                                     ResourceFormat format)
    : child_id(0),
      gl_id(texture_id),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      pixels(NULL),
      pixel_buffer(NULL),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      locked_for_write(false),
      external(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      read_lock_fence(NULL),
      size(size),
      target(target),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      dirty_image(false),
      texture_pool(texture_pool),
      wrap_mode(wrap_mode),
      lost(false),
      hint(hint),
      type(GLTexture),
      format(format),
      shared_bitmap(NULL) {
  DCHECK(wrap_mode == GL_CLAMP_TO_EDGE || wrap_mode == GL_REPEAT);
}

ResourceProvider::Resource::Resource(uint8_t* pixels,
                                     SharedBitmap* bitmap,
                                     gfx::Size size,
                                     GLenum filter,
                                     GLint wrap_mode)
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      pixels(pixels),
      pixel_buffer(NULL),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      locked_for_write(false),
      external(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      read_lock_fence(NULL),
      size(size),
      target(0),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      dirty_image(false),
      texture_pool(0),
      wrap_mode(wrap_mode),
      lost(false),
      hint(TextureUsageAny),
      type(Bitmap),
      format(RGBA_8888),
      shared_bitmap(bitmap) {
  DCHECK(wrap_mode == GL_CLAMP_TO_EDGE || wrap_mode == GL_REPEAT);
}

ResourceProvider::Child::Child() : marked_for_deletion(false) {}

ResourceProvider::Child::~Child() {}

scoped_ptr<ResourceProvider> ResourceProvider::Create(
    OutputSurface* output_surface,
    SharedBitmapManager* shared_bitmap_manager,
    int highp_threshold_min,
    bool use_rgba_4444_texture_format,
    size_t id_allocation_chunk_size) {
  scoped_ptr<ResourceProvider> resource_provider(
      new ResourceProvider(output_surface,
                           shared_bitmap_manager,
                           highp_threshold_min,
                           use_rgba_4444_texture_format,
                           id_allocation_chunk_size));

  bool success = false;
  if (resource_provider->ContextGL()) {
    success = resource_provider->InitializeGL();
  } else {
    resource_provider->InitializeSoftware();
    success = true;
  }

  if (!success)
    return scoped_ptr<ResourceProvider>();

  DCHECK_NE(InvalidType, resource_provider->default_resource_type());
  return resource_provider.Pass();
}

ResourceProvider::~ResourceProvider() {
  while (!children_.empty())
    DestroyChildInternal(children_.begin(), ForShutdown);
  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin(), ForShutdown);

  CleanUpGLIfNeeded();
}

bool ResourceProvider::InUseByConsumer(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->lock_for_read_count > 0 || resource->exported_count > 0 ||
         resource->lost;
}

bool ResourceProvider::IsLost(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->lost;
}

ResourceProvider::ResourceId ResourceProvider::CreateResource(
    gfx::Size size,
    GLint wrap_mode,
    TextureUsageHint hint,
    ResourceFormat format) {
  DCHECK(!size.IsEmpty());
  switch (default_resource_type_) {
    case GLTexture:
      return CreateGLTexture(size,
                             GL_TEXTURE_2D,
                             GL_TEXTURE_POOL_UNMANAGED_CHROMIUM,
                             wrap_mode,
                             hint,
                             format);
    case Bitmap:
      DCHECK_EQ(RGBA_8888, format);
      return CreateBitmap(size, wrap_mode);
    case InvalidType:
      break;
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceProvider::ResourceId ResourceProvider::CreateManagedResource(
    gfx::Size size,
    GLenum target,
    GLint wrap_mode,
    TextureUsageHint hint,
    ResourceFormat format) {
  DCHECK(!size.IsEmpty());
  switch (default_resource_type_) {
    case GLTexture:
      return CreateGLTexture(size,
                             target,
                             GL_TEXTURE_POOL_MANAGED_CHROMIUM,
                             wrap_mode,
                             hint,
                             format);
    case Bitmap:
      DCHECK_EQ(RGBA_8888, format);
      return CreateBitmap(size, wrap_mode);
    case InvalidType:
      break;
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceProvider::ResourceId ResourceProvider::CreateGLTexture(
    gfx::Size size,
    GLenum target,
    GLenum texture_pool,
    GLint wrap_mode,
    TextureUsageHint hint,
    ResourceFormat format) {
  DCHECK_LE(size.width(), max_texture_size_);
  DCHECK_LE(size.height(), max_texture_size_);
  DCHECK(thread_checker_.CalledOnValidThread());

  ResourceId id = next_id_++;
  Resource resource(
      0, size, target, GL_LINEAR, texture_pool, wrap_mode, hint, format);
  resource.allocated = false;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateBitmap(
    gfx::Size size, GLint wrap_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<SharedBitmap> bitmap;
  if (shared_bitmap_manager_)
    bitmap = shared_bitmap_manager_->AllocateSharedBitmap(size);

  uint8_t* pixels;
  if (bitmap)
    pixels = bitmap->pixels();
  else
    pixels = new uint8_t[4 * size.GetArea()];

  ResourceId id = next_id_++;
  Resource resource(
      pixels, bitmap.release(), size, GL_LINEAR, wrap_mode);
  resource.allocated = true;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId
ResourceProvider::CreateResourceFromExternalTexture(
    GLuint texture_target,
    GLuint texture_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  GLC(gl, gl->BindTexture(texture_target, texture_id));
  GLC(gl, gl->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GLC(gl, gl->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GLC(gl,
      gl->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GLC(gl,
      gl->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  ResourceId id = next_id_++;
  Resource resource(texture_id,
                    gfx::Size(),
                    texture_target,
                    GL_LINEAR,
                    0,
                    GL_CLAMP_TO_EDGE,
                    TextureUsageAny,
                    RGBA_8888);
  resource.external = true;
  resource.allocated = true;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateResourceFromTextureMailbox(
    const TextureMailbox& mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Just store the information. Mailbox will be consumed in LockForRead().
  ResourceId id = next_id_++;
  DCHECK(mailbox.IsValid());
  Resource& resource = resources_[id];
  if (mailbox.IsTexture()) {
    resource = Resource(0,
                        gfx::Size(),
                        mailbox.target(),
                        GL_LINEAR,
                        0,
                        GL_CLAMP_TO_EDGE,
                        TextureUsageAny,
                        RGBA_8888);
  } else {
    DCHECK(mailbox.IsSharedMemory());
    base::SharedMemory* shared_memory = mailbox.shared_memory();
    DCHECK(shared_memory->memory());
    uint8_t* pixels = reinterpret_cast<uint8_t*>(shared_memory->memory());
    scoped_ptr<SharedBitmap> shared_bitmap;
    if (shared_bitmap_manager_) {
      shared_bitmap =
          shared_bitmap_manager_->GetBitmapForSharedMemory(shared_memory);
    }
    resource = Resource(pixels,
                        shared_bitmap.release(),
                        mailbox.shared_memory_size(),
                        GL_LINEAR,
                        GL_CLAMP_TO_EDGE);
  }
  resource.external = true;
  resource.allocated = true;
  resource.mailbox = mailbox;
  resource.release_callback =
      base::Bind(&SingleReleaseCallback::Run,
                 base::Owned(release_callback.release()));
  return id;
}

void ResourceProvider::DeleteResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->lock_for_read_count);
  DCHECK(!resource->marked_for_deletion);
  DCHECK_EQ(resource->imported_count, 0);
  DCHECK(resource->pending_set_pixels || !resource->locked_for_write);

  if (resource->exported_count > 0) {
    resource->marked_for_deletion = true;
    return;
  } else {
    DeleteResourceInternal(it, Normal);
  }
}

void ResourceProvider::DeleteResourceInternal(ResourceMap::iterator it,
                                              DeleteStyle style) {
  TRACE_EVENT0("cc", "ResourceProvider::DeleteResourceInternal");
  Resource* resource = &it->second;
  bool lost_resource = resource->lost;

  DCHECK(resource->exported_count == 0 || style != Normal);
  if (style == ForShutdown && resource->exported_count > 0)
    lost_resource = true;

  if (resource->image_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DestroyImageCHROMIUM(resource->image_id));
  }

  if (resource->gl_id && !resource->external) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteTextures(1, &resource->gl_id));
  }
  if (resource->gl_upload_query_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteQueriesEXT(1, &resource->gl_upload_query_id));
  }
  if (resource->gl_pixel_buffer_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteBuffers(1, &resource->gl_pixel_buffer_id));
  }
  if (resource->mailbox.IsValid() && resource->external) {
    GLuint sync_point = resource->mailbox.sync_point();
    if (resource->mailbox.IsTexture()) {
      lost_resource |= lost_output_surface_;
      GLES2Interface* gl = ContextGL();
      DCHECK(gl);
      if (resource->gl_id)
        GLC(gl, gl->DeleteTextures(1, &resource->gl_id));
      if (!lost_resource && resource->gl_id)
        sync_point = gl->InsertSyncPointCHROMIUM();
    } else {
      DCHECK(resource->mailbox.IsSharedMemory());
      base::SharedMemory* shared_memory = resource->mailbox.shared_memory();
      if (resource->pixels && shared_memory) {
        DCHECK(shared_memory->memory() == resource->pixels);
        resource->pixels = NULL;
        delete resource->shared_bitmap;
        resource->shared_bitmap = NULL;
      }
    }
    resource->release_callback.Run(sync_point, lost_resource);
  }
  if (resource->shared_bitmap) {
    delete resource->shared_bitmap;
    resource->pixels = NULL;
  }
  if (resource->pixels)
    delete[] resource->pixels;
  if (resource->pixel_buffer)
    delete[] resource->pixel_buffer;

  resources_.erase(it);
}

ResourceProvider::ResourceType ResourceProvider::GetResourceType(
    ResourceId id) {
  return GetResource(id)->type;
}

void ResourceProvider::SetPixels(ResourceId id,
                                 const uint8_t* image,
                                 gfx::Rect image_rect,
                                 gfx::Rect source_rect,
                                 gfx::Vector2d dest_offset) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  if (resource->gl_id) {
    DCHECK(!resource->pending_set_pixels);
    DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(texture_uploader_.get());
    gl->BindTexture(GL_TEXTURE_2D, resource->gl_id);
    texture_uploader_->Upload(image,
                              image_rect,
                              source_rect,
                              dest_offset,
                              resource->format,
                              resource->size);
  }

  if (resource->pixels) {
    DCHECK(resource->allocated);
    DCHECK_EQ(RGBA_8888, resource->format);
    SkBitmap src_full;
    src_full.setConfig(
        SkBitmap::kARGB_8888_Config, image_rect.width(), image_rect.height());
    src_full.setPixels(const_cast<uint8_t*>(image));
    SkBitmap src_subset;
    SkIRect sk_source_rect = SkIRect::MakeXYWH(source_rect.x(),
                                               source_rect.y(),
                                               source_rect.width(),
                                               source_rect.height());
    sk_source_rect.offset(-image_rect.x(), -image_rect.y());
    src_full.extractSubset(&src_subset, sk_source_rect);

    ScopedWriteLockSoftware lock(this, id);
    SkCanvas* dest = lock.sk_canvas();
    dest->writePixels(src_subset, dest_offset.x(), dest_offset.y());
  }
}

size_t ResourceProvider::NumBlockingUploads() {
  if (!texture_uploader_)
    return 0;

  return texture_uploader_->NumBlockingUploads();
}

void ResourceProvider::MarkPendingUploadsAsNonBlocking() {
  if (!texture_uploader_)
    return;

  texture_uploader_->MarkPendingUploadsAsNonBlocking();
}

size_t ResourceProvider::EstimatedUploadsPerTick() {
  if (!texture_uploader_)
    return 1u;

  double textures_per_second = texture_uploader_->EstimatedTexturesPerSecond();
  size_t textures_per_tick = floor(
      kTextureUploadTickRate * textures_per_second);
  return textures_per_tick ? textures_per_tick : 1u;
}

void ResourceProvider::FlushUploads() {
  if (!texture_uploader_)
    return;

  texture_uploader_->Flush();
}

void ResourceProvider::ReleaseCachedData() {
  if (!texture_uploader_)
    return;

  texture_uploader_->ReleaseCachedQueries();
}

base::TimeTicks ResourceProvider::EstimatedUploadCompletionTime(
    size_t uploads_per_tick) {
  if (lost_output_surface_)
    return base::TimeTicks();

  // Software resource uploads happen on impl thread, so don't bother batching
  // them up and trying to wait for them to complete.
  if (!texture_uploader_) {
    return gfx::FrameTime::Now() + base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * kSoftwareUploadTickRate);
  }

  base::TimeDelta upload_one_texture_time =
      base::TimeDelta::FromMicroseconds(
          base::Time::kMicrosecondsPerSecond * kTextureUploadTickRate) /
      uploads_per_tick;

  size_t total_uploads = NumBlockingUploads() + uploads_per_tick;
  return gfx::FrameTime::Now() + upload_one_texture_time * total_uploads;
}

void ResourceProvider::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Flush();
}

void ResourceProvider::Finish() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Finish();
}

bool ResourceProvider::ShallowFlushIfSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  if (!gl)
    return false;

  gl->ShallowFlushCHROMIUM();
  return true;
}

ResourceProvider::Resource* ResourceProvider::GetResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  return &it->second;
}

const ResourceProvider::Resource* ResourceProvider::LockForRead(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write ||
         resource->set_pixels_completion_forced) <<
      "locked for write: " << resource->locked_for_write <<
      " pixels completion forced: " << resource->set_pixels_completion_forced;
  DCHECK_EQ(resource->exported_count, 0);
  // Uninitialized! Call SetPixels or LockForWrite first.
  DCHECK(resource->allocated);

  LazyCreate(resource);

  if (resource->external) {
    if (!resource->gl_id && resource->mailbox.IsTexture()) {
      GLES2Interface* gl = ContextGL();
      DCHECK(gl);
      if (resource->mailbox.sync_point()) {
        GLC(gl, gl->WaitSyncPointCHROMIUM(resource->mailbox.sync_point()));
        resource->mailbox.ResetSyncPoint();
      }
      resource->gl_id = texture_id_allocator_->NextId();
      GLC(gl, gl->BindTexture(resource->target, resource->gl_id));
      GLC(gl,
          gl->ConsumeTextureCHROMIUM(resource->target,
                                     resource->mailbox.data()));
    }
  }

  resource->lock_for_read_count++;
  if (resource->enable_read_lock_fences)
    resource->read_lock_fence = current_read_lock_fence_;

  return resource;
}

void ResourceProvider::UnlockForRead(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK_GT(resource->lock_for_read_count, 0);
  DCHECK_EQ(resource->exported_count, 0);
  resource->lock_for_read_count--;
}

const ResourceProvider::Resource* ResourceProvider::LockForWrite(
    ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->external);
  DCHECK(!resource->lost);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  resource->locked_for_write = true;
  return resource;
}

bool ResourceProvider::CanLockForWrite(ResourceId id) {
  Resource* resource = GetResource(id);
  return !resource->locked_for_write && !resource->lock_for_read_count &&
         !resource->exported_count && !resource->external && !resource->lost &&
         ReadLockFenceHasPassed(resource);
}

void ResourceProvider::UnlockForWrite(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->locked_for_write);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->external);
  resource->locked_for_write = false;
}

ResourceProvider::ScopedReadLockGL::ScopedReadLockGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      texture_id_(resource_provider->LockForRead(resource_id)->gl_id) {
  DCHECK(texture_id_);
}

ResourceProvider::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id,
    GLenum filter)
    : ScopedReadLockGL(resource_provider, resource_id),
      unit_(GL_TEXTURE0),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {
}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id,
    GLenum unit,
    GLenum filter)
    : ScopedReadLockGL(resource_provider, resource_id),
      unit_(unit),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {
}

ResourceProvider::ScopedSamplerGL::~ScopedSamplerGL() {
}

ResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      texture_id_(resource_provider->LockForWrite(resource_id)->gl_id) {
  DCHECK(texture_id_);
}

ResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL() {
  resource_provider_->UnlockForWrite(resource_id_);
}

void ResourceProvider::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap, const Resource* resource) {
  DCHECK(resource->pixels);
  DCHECK_EQ(RGBA_8888, resource->format);
  sk_bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                       resource->size.width(),
                       resource->size.height());
  sk_bitmap->setPixels(resource->pixels);
}

ResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id) {
  const Resource* resource = resource_provider->LockForRead(resource_id);
  wrap_mode_ = resource->wrap_mode;
  ResourceProvider::PopulateSkBitmapWithResource(&sk_bitmap_, resource);
}

ResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id) {
  ResourceProvider::PopulateSkBitmapWithResource(
      &sk_bitmap_, resource_provider->LockForWrite(resource_id));
  sk_canvas_.reset(new SkCanvas(sk_bitmap_));
}

ResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware() {
  resource_provider_->UnlockForWrite(resource_id_);
}

ResourceProvider::ResourceProvider(OutputSurface* output_surface,
                                   SharedBitmapManager* shared_bitmap_manager,
                                   int highp_threshold_min,
                                   bool use_rgba_4444_texture_format,
                                   size_t id_allocation_chunk_size)
    : output_surface_(output_surface),
      shared_bitmap_manager_(shared_bitmap_manager),
      lost_output_surface_(false),
      highp_threshold_min_(highp_threshold_min),
      next_id_(1),
      next_child_(1),
      default_resource_type_(InvalidType),
      use_texture_storage_ext_(false),
      use_texture_usage_hint_(false),
      use_compressed_texture_etc1_(false),
      max_texture_size_(0),
      best_texture_format_(RGBA_8888),
      use_rgba_4444_texture_format_(use_rgba_4444_texture_format),
      id_allocation_chunk_size_(id_allocation_chunk_size) {
  DCHECK(output_surface_->HasClient());
  DCHECK(id_allocation_chunk_size_);
}

void ResourceProvider::InitializeSoftware() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(Bitmap, default_resource_type_);

  CleanUpGLIfNeeded();

  default_resource_type_ = Bitmap;
  // Pick an arbitrary limit here similar to what hardware might.
  max_texture_size_ = 16 * 1024;
  best_texture_format_ = RGBA_8888;
}

bool ResourceProvider::InitializeGL() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!texture_uploader_);
  DCHECK_NE(GLTexture, default_resource_type_);
  DCHECK(!texture_id_allocator_);
  DCHECK(!buffer_id_allocator_);

  default_resource_type_ = GLTexture;

  const ContextProvider::Capabilities& caps =
      output_surface_->context_provider()->ContextCapabilities();

  bool use_bgra = caps.texture_format_bgra8888;
  use_texture_storage_ext_ = caps.texture_storage;
  use_texture_usage_hint_ = caps.texture_usage;
  use_compressed_texture_etc1_ = caps.texture_format_etc1;

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  texture_uploader_ = TextureUploader::Create(gl);
  max_texture_size_ = 0;  // Context expects cleared value.
  GLC(gl, gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_));
  best_texture_format_ = PlatformColor::BestTextureFormat(use_bgra);

  texture_id_allocator_.reset(
      new TextureIdAllocator(gl, id_allocation_chunk_size_));
  buffer_id_allocator_.reset(
      new BufferIdAllocator(gl, id_allocation_chunk_size_));

  return true;
}

void ResourceProvider::CleanUpGLIfNeeded() {
  GLES2Interface* gl = ContextGL();
  if (default_resource_type_ != GLTexture) {
    // We are not in GL mode, but double check before returning.
    DCHECK(!gl);
    DCHECK(!texture_uploader_);
    return;
  }

  DCHECK(gl);
  texture_uploader_.reset();
  texture_id_allocator_.reset();
  buffer_id_allocator_.reset();
  Finish();
}

int ResourceProvider::CreateChild(const ReturnCallback& return_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Child child_info;
  child_info.return_callback = return_callback;

  int child = next_child_++;
  children_[child] = child_info;
  return child;
}

void ResourceProvider::DestroyChild(int child_id) {
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  DestroyChildInternal(it, Normal);
}

void ResourceProvider::DestroyChildInternal(ChildMap::iterator it,
                                            DeleteStyle style) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Child& child = it->second;
  DCHECK(style == ForShutdown || !child.marked_for_deletion);

  ResourceIdArray resources_for_child;

  for (ResourceIdMap::iterator child_it = child.child_to_parent_map.begin();
       child_it != child.child_to_parent_map.end();
       ++child_it) {
    ResourceId id = child_it->second;
    resources_for_child.push_back(id);
  }

  // If the child is going away, don't consider any resources in use.
  child.in_use_resources.clear();
  child.marked_for_deletion = true;

  DeleteAndReturnUnusedResourcesToChild(it, style, resources_for_child);
}

const ResourceProvider::ResourceIdMap& ResourceProvider::GetChildToParentMap(
    int child) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ChildMap::const_iterator it = children_.find(child);
  DCHECK(it != children_.end());
  DCHECK(!it->second.marked_for_deletion);
  return it->second.child_to_parent_map;
}

void ResourceProvider::PrepareSendToParent(const ResourceIdArray& resources,
                                           TransferableResourceArray* list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  bool need_sync_point = false;
  for (ResourceIdArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    TransferableResource resource;
    TransferResource(gl, *it, &resource);
    if (!resource.sync_point && !resource.is_software)
      need_sync_point = true;
    ++resources_.find(*it)->second.exported_count;
    list->push_back(resource);
  }
  if (need_sync_point) {
    GLuint sync_point = gl->InsertSyncPointCHROMIUM();
    for (TransferableResourceArray::iterator it = list->begin();
         it != list->end();
         ++it) {
      if (!it->sync_point)
        it->sync_point = sync_point;
    }
  }
}

void ResourceProvider::ReceiveFromChild(
    int child, const TransferableResourceArray& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  Child& child_info = children_.find(child)->second;
  DCHECK(!child_info.marked_for_deletion);
  for (TransferableResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceIdMap::iterator resource_in_map_it =
        child_info.child_to_parent_map.find(it->id);
    if (resource_in_map_it != child_info.child_to_parent_map.end()) {
      resources_[resource_in_map_it->second].imported_count++;
      continue;
    }

    scoped_ptr<SharedBitmap> bitmap;
    uint8_t* pixels = NULL;
    if (it->is_software) {
      if (shared_bitmap_manager_)
        bitmap = shared_bitmap_manager_->GetSharedBitmapFromId(it->size,
                                                               it->mailbox);
      if (bitmap)
        pixels = bitmap->pixels();
    }

    if ((!it->is_software && !gl) || (it->is_software && !pixels)) {
      TRACE_EVENT0("cc", "ResourceProvider::ReceiveFromChild dropping invalid");
      ReturnedResourceArray to_return;
      to_return.push_back(it->ToReturnedResource());
      child_info.return_callback.Run(to_return);
      continue;
    }

    ResourceId local_id = next_id_++;
    Resource& resource = resources_[local_id];
    if (it->is_software) {
      resource = Resource(
          pixels, bitmap.release(), it->size, GL_LINEAR, GL_CLAMP_TO_EDGE);
    } else {
      GLuint texture_id;
      // NOTE: If the parent is a browser and the child a renderer, the parent
      // is not supposed to have its context wait, because that could induce
      // deadlocks and/or security issues. The caller is responsible for
      // waiting asynchronously, and resetting sync_point before calling this.
      // However if the parent is a renderer (e.g. browser tag), it may be ok
      // (and is simpler) to wait.
      if (it->sync_point)
        GLC(gl, gl->WaitSyncPointCHROMIUM(it->sync_point));
      texture_id = texture_id_allocator_->NextId();
      GLC(gl, gl->BindTexture(it->target, texture_id));
      GLC(gl, gl->ConsumeTextureCHROMIUM(it->target, it->mailbox.name));
      resource = Resource(texture_id,
                          it->size,
                          it->target,
                          it->filter,
                          0,
                          GL_CLAMP_TO_EDGE,
                          TextureUsageAny,
                          it->format);
      resource.mailbox.SetName(it->mailbox);
    }
    resource.child_id = child;
    // Don't allocate a texture for a child.
    resource.allocated = true;
    resource.imported_count = 1;
    child_info.parent_to_child_map[local_id] = it->id;
    child_info.child_to_parent_map[it->id] = local_id;
  }
}

void ResourceProvider::DeclareUsedResourcesFromChild(
    int child,
    const ResourceIdArray& resources_from_child) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ChildMap::iterator child_it = children_.find(child);
  DCHECK(child_it != children_.end());
  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);
  child_info.in_use_resources.clear();

  for (size_t i = 0; i < resources_from_child.size(); ++i) {
    ResourceIdMap::iterator it =
        child_info.child_to_parent_map.find(resources_from_child[i]);
    DCHECK(it != child_info.child_to_parent_map.end());

    ResourceId local_id = it->second;
    child_info.in_use_resources.insert(local_id);
  }

  ResourceIdArray unused;
  for (ResourceIdMap::iterator it = child_info.child_to_parent_map.begin();
       it != child_info.child_to_parent_map.end();
       ++it) {
    ResourceId local_id = it->second;
    bool resource_is_in_use = child_info.in_use_resources.count(local_id) > 0;
    if (!resource_is_in_use)
      unused.push_back(local_id);
  }
  DeleteAndReturnUnusedResourcesToChild(child_it, Normal, unused);
}

// static
bool ResourceProvider::CompareResourceMapIteratorsByChildId(
    const std::pair<ReturnedResource, ResourceMap::iterator>& a,
    const std::pair<ReturnedResource, ResourceMap::iterator>& b) {
  const ResourceMap::iterator& a_it = a.second;
  const ResourceMap::iterator& b_it = b.second;
  const Resource& a_resource = a_it->second;
  const Resource& b_resource = b_it->second;
  return a_resource.child_id < b_resource.child_id;
}

void ResourceProvider::ReceiveReturnsFromParent(
    const ReturnedResourceArray& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();

  int child_id = 0;
  ResourceIdArray resources_for_child;

  std::vector<std::pair<ReturnedResource, ResourceMap::iterator> >
      sorted_resources;

  for (ReturnedResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceId local_id = it->id;
    ResourceMap::iterator map_iterator = resources_.find(local_id);

    // Resource was already lost (e.g. it belonged to a child that was
    // destroyed).
    if (map_iterator == resources_.end())
      continue;

    sorted_resources.push_back(
        std::pair<ReturnedResource, ResourceMap::iterator>(*it, map_iterator));
  }

  std::sort(sorted_resources.begin(),
            sorted_resources.end(),
            CompareResourceMapIteratorsByChildId);

  ChildMap::iterator child_it = children_.end();
  for (size_t i = 0; i < sorted_resources.size(); ++i) {
    ReturnedResource& returned = sorted_resources[i].first;
    ResourceMap::iterator& map_iterator = sorted_resources[i].second;
    ResourceId local_id = map_iterator->first;
    Resource* resource = &map_iterator->second;

    CHECK_GE(resource->exported_count, returned.count);
    resource->exported_count -= returned.count;
    resource->lost |= returned.lost;
    if (resource->exported_count)
      continue;

    if (resource->gl_id) {
      if (returned.sync_point)
        GLC(gl, gl->WaitSyncPointCHROMIUM(returned.sync_point));
    } else if (!resource->shared_bitmap) {
      resource->mailbox =
          TextureMailbox(resource->mailbox.name(), returned.sync_point);
    }

    if (!resource->marked_for_deletion)
      continue;

    if (!resource->child_id) {
      // The resource belongs to this ResourceProvider, so it can be destroyed.
      DeleteResourceInternal(map_iterator, Normal);
      continue;
    }

    // Delete the resource and return it to the child it came from one.
    if (resource->child_id != child_id) {
      if (child_id) {
        DCHECK_NE(resources_for_child.size(), 0u);
        DCHECK(child_it != children_.end());
        DeleteAndReturnUnusedResourcesToChild(
            child_it, Normal, resources_for_child);
        resources_for_child.clear();
      }

      child_it = children_.find(resource->child_id);
      DCHECK(child_it != children_.end());
      child_id = resource->child_id;
    }
    resources_for_child.push_back(local_id);
  }

  if (child_id) {
    DCHECK_NE(resources_for_child.size(), 0u);
    DCHECK(child_it != children_.end());
    DeleteAndReturnUnusedResourcesToChild(
        child_it, Normal, resources_for_child);
  }
}

void ResourceProvider::TransferResource(GLES2Interface* gl,
                                        ResourceId id,
                                        TransferableResource* resource) {
  Resource* source = GetResource(id);
  DCHECK(!source->locked_for_write);
  DCHECK(!source->lock_for_read_count);
  DCHECK(!source->external || (source->external && source->mailbox.IsValid()));
  DCHECK(source->allocated);
  DCHECK_EQ(source->wrap_mode, GL_CLAMP_TO_EDGE);
  resource->id = id;
  resource->format = source->format;
  resource->target = source->target;
  resource->filter = source->filter;
  resource->size = source->size;

  if (source->shared_bitmap) {
    resource->mailbox = source->shared_bitmap->id();
    resource->is_software = true;
  } else if (!source->mailbox.IsValid()) {
    // This is a resource allocated by the compositor, we need to produce it.
    // Don't set a sync point, the caller will do it.
    DCHECK(source->gl_id);
    GLC(gl, gl->BindTexture(resource->target, source->gl_id));
    GLC(gl, gl->GenMailboxCHROMIUM(resource->mailbox.name));
    GLC(gl,
        gl->ProduceTextureCHROMIUM(resource->target, resource->mailbox.name));
    source->mailbox.SetName(resource->mailbox);
  } else {
    DCHECK(source->mailbox.IsTexture());
    // This is either an external resource, or a compositor resource that we
    // already exported. Make sure to forward the sync point that we were given.
    resource->mailbox = source->mailbox.name();
    resource->sync_point = source->mailbox.sync_point();
    source->mailbox.ResetSyncPoint();
  }
}

void ResourceProvider::DeleteAndReturnUnusedResourcesToChild(
    ChildMap::iterator child_it,
    DeleteStyle style,
    const ResourceIdArray& unused) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(child_it != children_.end());
  Child* child_info = &child_it->second;

  if (unused.empty() && !child_info->marked_for_deletion)
    return;

  ReturnedResourceArray to_return;

  GLES2Interface* gl = ContextGL();
  bool need_sync_point = false;
  for (size_t i = 0; i < unused.size(); ++i) {
    ResourceId local_id = unused[i];

    ResourceMap::iterator it = resources_.find(local_id);
    CHECK(it != resources_.end());
    Resource& resource = it->second;

    DCHECK(!resource.locked_for_write);
    DCHECK(!resource.lock_for_read_count);
    DCHECK_EQ(0u, child_info->in_use_resources.count(local_id));
    DCHECK(child_info->parent_to_child_map.count(local_id));

    ResourceId child_id = child_info->parent_to_child_map[local_id];
    DCHECK(child_info->child_to_parent_map.count(child_id));

    bool is_lost =
        resource.lost || (!resource.shared_bitmap && lost_output_surface_);
    if (resource.exported_count > 0) {
      if (style != ForShutdown) {
        // Defer this until we receive the resource back from the parent.
        resource.marked_for_deletion = true;
        continue;
      }

      // We still have an exported_count, so we'll have to lose it.
      is_lost = true;
    }

    if (gl && resource.filter != resource.original_filter) {
      DCHECK(resource.target);
      DCHECK(resource.gl_id);

      GLC(gl, gl->BindTexture(resource.target, resource.gl_id));
      GLC(gl,
          gl->TexParameteri(resource.target,
                            GL_TEXTURE_MIN_FILTER,
                            resource.original_filter));
      GLC(gl,
          gl->TexParameteri(resource.target,
                            GL_TEXTURE_MAG_FILTER,
                            resource.original_filter));
    }

    ReturnedResource returned;
    returned.id = child_id;
    returned.sync_point = resource.mailbox.sync_point();
    if (!returned.sync_point && !resource.shared_bitmap)
      need_sync_point = true;
    returned.count = resource.imported_count;
    returned.lost = is_lost;
    to_return.push_back(returned);

    child_info->parent_to_child_map.erase(local_id);
    child_info->child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
    DeleteResourceInternal(it, style);
  }
  if (need_sync_point) {
    DCHECK(gl);
    GLuint sync_point = gl->InsertSyncPointCHROMIUM();
    for (size_t i = 0; i < to_return.size(); ++i) {
      if (!to_return[i].sync_point)
        to_return[i].sync_point = sync_point;
    }
  }

  if (!to_return.empty())
    child_info->return_callback.Run(to_return);

  if (child_info->marked_for_deletion &&
      child_info->parent_to_child_map.empty()) {
    DCHECK(child_info->child_to_parent_map.empty());
    children_.erase(child_it);
  }
}

void ResourceProvider::AcquirePixelBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);
  DCHECK_NE(ETC1, resource->format);

  if (resource->type == GLTexture) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    if (!resource->gl_pixel_buffer_id)
      resource->gl_pixel_buffer_id = buffer_id_allocator_->NextId();
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                   resource->gl_pixel_buffer_id);
    unsigned bytes_per_pixel = BitsPerPixel(resource->format) / 8;
    gl->BufferData(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                   resource->size.height() *
                       RoundUp(bytes_per_pixel * resource->size.width(), 4u),
                   NULL,
                   GL_DYNAMIC_DRAW);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels) {
    if (resource->pixel_buffer)
      return;

    resource->pixel_buffer = new uint8_t[4 * resource->size.GetArea()];
  }
}

void ResourceProvider::ReleasePixelBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);

  // The pixel buffer can be released while there is a pending "set pixels"
  // if completion has been forced. Any shared memory associated with this
  // pixel buffer will not be freed until the waitAsyncTexImage2DCHROMIUM
  // command has been processed on the service side. It is also safe to
  // reuse any query id associated with this resource before they complete
  // as each new query has a unique submit count.
  if (resource->pending_set_pixels) {
    DCHECK(resource->set_pixels_completion_forced);
    resource->pending_set_pixels = false;
    UnlockForWrite(id);
  }

  if (resource->type == GLTexture) {
    if (!resource->gl_pixel_buffer_id)
      return;
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                   resource->gl_pixel_buffer_id);
    gl->BufferData(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0, NULL, GL_DYNAMIC_DRAW);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels) {
    if (!resource->pixel_buffer)
      return;
    delete[] resource->pixel_buffer;
    resource->pixel_buffer = NULL;
  }
}

uint8_t* ResourceProvider::MapPixelBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);

  if (resource->type == GLTexture) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(resource->gl_pixel_buffer_id);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                   resource->gl_pixel_buffer_id);
    uint8_t* image = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, GL_WRITE_ONLY));
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
    // Buffer is required to be 4-byte aligned.
    CHECK(!(reinterpret_cast<intptr_t>(image) & 3));
    return image;
  }

  if (resource->pixels)
    return resource->pixel_buffer;

  return NULL;
}

void ResourceProvider::UnmapPixelBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);

  if (resource->type == GLTexture) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(resource->gl_pixel_buffer_id);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                   resource->gl_pixel_buffer_id);
    gl->UnmapBufferCHROMIUM(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }
}

GLenum ResourceProvider::BindForSampling(
    ResourceProvider::ResourceId resource_id,
    GLenum unit,
    GLenum filter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  ResourceMap::iterator it = resources_.find(resource_id);
  DCHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);
  DCHECK(!resource->locked_for_write || resource->set_pixels_completion_forced);

  ScopedSetActiveTexture scoped_active_tex(gl, unit);
  GLenum target = resource->target;
  GLC(gl, gl->BindTexture(target, resource->gl_id));
  if (filter != resource->filter) {
    GLC(gl, gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter));
    GLC(gl, gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter));
    resource->filter = filter;
  }

  if (resource->image_id && resource->dirty_image) {
    // Release image currently bound to texture.
    if (resource->bound_image_id)
      gl->ReleaseTexImage2DCHROMIUM(target, resource->bound_image_id);
    gl->BindTexImage2DCHROMIUM(target, resource->image_id);
    resource->bound_image_id = resource->image_id;
    resource->dirty_image = false;
  }

  return target;
}

void ResourceProvider::BeginSetPixels(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->pending_set_pixels);

  LazyCreate(resource);
  DCHECK(resource->gl_id || resource->allocated);
  DCHECK(ReadLockFenceHasPassed(resource));
  DCHECK(!resource->image_id);

  bool allocate = !resource->allocated;
  resource->allocated = true;
  LockForWrite(id);

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(resource->gl_pixel_buffer_id);
    DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
    gl->BindTexture(GL_TEXTURE_2D, resource->gl_id);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                   resource->gl_pixel_buffer_id);
    if (!resource->gl_upload_query_id)
      gl->GenQueriesEXT(1, &resource->gl_upload_query_id);
    gl->BeginQueryEXT(GL_ASYNC_PIXEL_UNPACK_COMPLETED_CHROMIUM,
                      resource->gl_upload_query_id);
    if (allocate) {
      gl->AsyncTexImage2DCHROMIUM(GL_TEXTURE_2D,
                                  0, /* level */
                                  GLInternalFormat(resource->format),
                                  resource->size.width(),
                                  resource->size.height(),
                                  0, /* border */
                                  GLDataFormat(resource->format),
                                  GLDataType(resource->format),
                                  NULL);
    } else {
      gl->AsyncTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                     0, /* level */
                                     0, /* x */
                                     0, /* y */
                                     resource->size.width(),
                                     resource->size.height(),
                                     GLDataFormat(resource->format),
                                     GLDataType(resource->format),
                                     NULL);
    }
    gl->EndQueryEXT(GL_ASYNC_PIXEL_UNPACK_COMPLETED_CHROMIUM);
    gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels) {
    DCHECK(!resource->mailbox.IsValid());
    DCHECK(resource->pixel_buffer);
    DCHECK_EQ(RGBA_8888, resource->format);

    std::swap(resource->pixels, resource->pixel_buffer);
    delete[] resource->pixel_buffer;
    resource->pixel_buffer = NULL;
  }

  resource->pending_set_pixels = true;
  resource->set_pixels_completion_forced = false;
}

void ResourceProvider::ForceSetPixelsToComplete(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);
  DCHECK(!resource->set_pixels_completion_forced);

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    GLC(gl, gl->BindTexture(GL_TEXTURE_2D, resource->gl_id));
    GLC(gl, gl->WaitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D));
    GLC(gl, gl->BindTexture(GL_TEXTURE_2D, 0));
  }

  resource->set_pixels_completion_forced = true;
}

bool ResourceProvider::DidSetPixelsComplete(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(resource->gl_upload_query_id);
    GLuint complete = 1;
    gl->GetQueryObjectuivEXT(
        resource->gl_upload_query_id, GL_QUERY_RESULT_AVAILABLE_EXT, &complete);
    if (!complete)
      return false;
  }

  resource->pending_set_pixels = false;
  UnlockForWrite(id);

  return true;
}

void ResourceProvider::CreateForTesting(ResourceId id) {
  LazyCreate(GetResource(id));
}

GLenum ResourceProvider::TargetForTesting(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->target;
}

void ResourceProvider::LazyCreate(Resource* resource) {
  if (resource->type != GLTexture || resource->gl_id != 0)
    return;

  // Early out for resources that don't require texture creation.
  if (resource->texture_pool == 0)
    return;

  resource->gl_id = texture_id_allocator_->NextId();

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  // Create and set texture properties. Allocation is delayed until needed.
  GLC(gl, gl->BindTexture(resource->target, resource->gl_id));
  GLC(gl,
      gl->TexParameteri(resource->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GLC(gl,
      gl->TexParameteri(resource->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GLC(gl,
      gl->TexParameteri(
          resource->target, GL_TEXTURE_WRAP_S, resource->wrap_mode));
  GLC(gl,
      gl->TexParameteri(
          resource->target, GL_TEXTURE_WRAP_T, resource->wrap_mode));
  GLC(gl,
      gl->TexParameteri(
          resource->target, GL_TEXTURE_POOL_CHROMIUM, resource->texture_pool));
  if (use_texture_usage_hint_ && resource->hint == TextureUsageFramebuffer) {
    GLC(gl,
        gl->TexParameteri(resource->target,
                          GL_TEXTURE_USAGE_ANGLE,
                          GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
  }
}

void ResourceProvider::AllocateForTesting(ResourceId id) {
  LazyAllocate(GetResource(id));
}

void ResourceProvider::LazyAllocate(Resource* resource) {
  DCHECK(resource);
  LazyCreate(resource);

  DCHECK(resource->gl_id || resource->allocated);
  if (resource->allocated || !resource->gl_id)
    return;
  resource->allocated = true;
  GLES2Interface* gl = ContextGL();
  gfx::Size& size = resource->size;
  DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
  ResourceFormat format = resource->format;
  GLC(gl, gl->BindTexture(GL_TEXTURE_2D, resource->gl_id));
  if (use_texture_storage_ext_ && IsFormatSupportedForStorage(format) &&
      resource->hint != TextureUsageFramebuffer) {
    GLenum storage_format = TextureToStorageFormat(format);
    GLC(gl,
        gl->TexStorage2DEXT(
            GL_TEXTURE_2D, 1, storage_format, size.width(), size.height()));
  } else {
    // ETC1 does not support preallocation.
    if (format != ETC1) {
      GLC(gl,
          gl->TexImage2D(GL_TEXTURE_2D,
                         0,
                         GLInternalFormat(format),
                         size.width(),
                         size.height(),
                         0,
                         GLDataFormat(format),
                         GLDataType(format),
                         NULL));
    }
  }
}

void ResourceProvider::EnableReadLockFences(ResourceProvider::ResourceId id,
                                            bool enable) {
  Resource* resource = GetResource(id);
  resource->enable_read_lock_fences = enable;
}

void ResourceProvider::AcquireImage(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);

  if (resource->type != GLTexture)
    return;

  if (resource->image_id)
    return;

  resource->allocated = true;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  resource->image_id =
      gl->CreateImageCHROMIUM(resource->size.width(),
                              resource->size.height(),
                              TextureToStorageFormat(resource->format));
  DCHECK(resource->image_id);
}

void ResourceProvider::ReleaseImage(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);

  if (!resource->image_id)
    return;

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->DestroyImageCHROMIUM(resource->image_id);
  resource->image_id = 0;
  resource->bound_image_id = 0;
  resource->dirty_image = false;
  resource->allocated = false;
}

uint8_t* ResourceProvider::MapImage(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(ReadLockFenceHasPassed(resource));
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);

  if (resource->image_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    return static_cast<uint8_t*>(
        gl->MapImageCHROMIUM(resource->image_id, GL_READ_WRITE));
  }

  if (resource->pixels)
    return resource->pixels;

  return NULL;
}

void ResourceProvider::UnmapImage(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);

  if (resource->image_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->UnmapImageCHROMIUM(resource->image_id);
    resource->dirty_image = true;
  }
}

int ResourceProvider::GetImageStride(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);

  int stride = 0;

  if (resource->image_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->GetImageParameterivCHROMIUM(
        resource->image_id, GL_IMAGE_ROWBYTES_CHROMIUM, &stride);
  }

  return stride;
}

base::SharedMemory* ResourceProvider::GetSharedMemory(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->external);
  DCHECK_EQ(resource->exported_count, 0);

  if (!resource->shared_bitmap)
    return NULL;
  return resource->shared_bitmap->memory();
}

GLint ResourceProvider::GetActiveTextureUnit(GLES2Interface* gl) {
  GLint active_unit = 0;
  gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
  return active_unit;
}

GLES2Interface* ResourceProvider::ContextGL() const {
  ContextProvider* context_provider = output_surface_->context_provider();
  return context_provider ? context_provider->ContextGL() : NULL;
}

}  // namespace cc
