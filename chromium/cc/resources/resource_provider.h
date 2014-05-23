// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/resources/release_callback.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/return_callback.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/resources/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/size.h"

namespace gpu {
namespace gles {
class GLES2Interface;
}
}

namespace gfx {
class Rect;
class Vector2d;
}

namespace cc {
class IdAllocator;
class SharedBitmap;
class SharedBitmapManager;
class TextureUploader;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider {
 public:
  typedef unsigned ResourceId;
  typedef std::vector<ResourceId> ResourceIdArray;
  typedef std::set<ResourceId> ResourceIdSet;
  typedef base::hash_map<ResourceId, ResourceId> ResourceIdMap;
  enum TextureUsageHint {
    TextureUsageAny,
    TextureUsageFramebuffer,
  };
  enum ResourceType {
    InvalidType = 0,
    GLTexture = 1,
    Bitmap,
  };

  static scoped_ptr<ResourceProvider> Create(
      OutputSurface* output_surface,
      SharedBitmapManager* shared_bitmap_manager,
      int highp_threshold_min,
      bool use_rgba_4444_texture_format,
      size_t id_allocation_chunk_size);
  virtual ~ResourceProvider();

  void InitializeSoftware();
  bool InitializeGL();

  void DidLoseOutputSurface() { lost_output_surface_ = true; }

  int max_texture_size() const { return max_texture_size_; }
  ResourceFormat memory_efficient_texture_format() const {
    return use_rgba_4444_texture_format_ ? RGBA_4444 : best_texture_format_;
  }
  ResourceFormat best_texture_format() const { return best_texture_format_; }
  size_t num_resources() const { return resources_.size(); }

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);

  bool IsLost(ResourceId id);

  // Producer interface.

  ResourceType default_resource_type() const { return default_resource_type_; }
  ResourceType GetResourceType(ResourceId id);

  // Creates a resource of the default resource type.
  ResourceId CreateResource(gfx::Size size,
                            GLint wrap_mode,
                            TextureUsageHint hint,
                            ResourceFormat format);

  // Creates a resource which is tagged as being managed for GPU memory
  // accounting purposes.
  ResourceId CreateManagedResource(gfx::Size size,
                                   GLenum target,
                                   GLint wrap_mode,
                                   TextureUsageHint hint,
                                   ResourceFormat format);

  // You can also explicitly create a specific resource type.
  ResourceId CreateGLTexture(gfx::Size size,
                             GLenum target,
                             GLenum texture_pool,
                             GLint wrap_mode,
                             TextureUsageHint hint,
                             ResourceFormat format);

  ResourceId CreateBitmap(gfx::Size size, GLint wrap_mode);
  // Wraps an external texture into a GL resource.
  ResourceId CreateResourceFromExternalTexture(
      unsigned texture_target,
      unsigned texture_id);

  // Wraps an external texture mailbox into a GL resource.
  ResourceId CreateResourceFromTextureMailbox(
      const TextureMailbox& mailbox,
      scoped_ptr<SingleReleaseCallback> release_callback);

  void DeleteResource(ResourceId id);

  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void SetPixels(ResourceId id,
                 const uint8_t* image,
                 gfx::Rect image_rect,
                 gfx::Rect source_rect,
                 gfx::Vector2d dest_offset);

  // Check upload status.
  size_t NumBlockingUploads();
  void MarkPendingUploadsAsNonBlocking();
  size_t EstimatedUploadsPerTick();
  void FlushUploads();
  void ReleaseCachedData();
  base::TimeTicks EstimatedUploadCompletionTime(size_t uploads_per_tick);

  // Flush all context operations, kicking uploads and ensuring ordering with
  // respect to other contexts.
  void Flush();

  // Finish all context operations, causing any pending callbacks to be
  // scheduled.
  void Finish();

  // Only flush the command buffer if supported.
  // Returns true if the shallow flush occurred, false otherwise.
  bool ShallowFlushIfSupported();

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(const ReturnCallback& return_callback);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Gets the child->parent resource ID map.
  const ResourceIdMap& GetChildToParentMap(int child) const;

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(const ResourceIdArray& resources,
                           TransferableResourceArray* transferable_resources);

  // Receives resources from a child, moving them from mailboxes. Resource IDs
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // This adds the resources to the working set in the ResourceProvider without
  // declaring which resources are in use. Use DeclareUsedResourcesFromChild
  // after calling this method to do that. All calls to ReceiveFromChild should
  // be followed by a DeclareUsedResourcesFromChild.
  // NOTE: if the sync_point is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child, const TransferableResourceArray& transferable_resources);

  // Once a set of resources have been received, they may or may not be used.
  // This declares what set of resources are currently in use from the child,
  // releasing any other resources back to the child.
  void DeclareUsedResourcesFromChild(
      int child,
      const ResourceIdArray& resources_from_child);

  // Receives resources from the parent, moving them from mailboxes. Resource
  // IDs passed are in the child namespace.
  // NOTE: if the sync_point is set on any TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      const ReturnedResourceArray& transferable_resources);

  // The following lock classes are part of the ResourceProvider API and are
  // needed to read and write the resource contents. The user must ensure
  // that they only use GL locks on GL resources, etc, and this is enforced
  // by assertions.
  class CC_EXPORT ScopedReadLockGL {
   public:
    ScopedReadLockGL(ResourceProvider* resource_provider,
                     ResourceProvider::ResourceId resource_id);
    virtual ~ScopedReadLockGL();

    unsigned texture_id() const { return texture_id_; }

   protected:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;

   private:
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
  };

  class CC_EXPORT ScopedSamplerGL : public ScopedReadLockGL {
   public:
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceProvider::ResourceId resource_id,
                    GLenum filter);
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceProvider::ResourceId resource_id,
                    GLenum unit,
                    GLenum filter);
    virtual ~ScopedSamplerGL();

    GLenum target() const { return target_; }

   private:
    GLenum unit_;
    GLenum target_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
  };

  class CC_EXPORT ScopedWriteLockGL {
   public:
    ScopedWriteLockGL(ResourceProvider* resource_provider,
                      ResourceProvider::ResourceId resource_id);
    ~ScopedWriteLockGL();

    unsigned texture_id() const { return texture_id_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  class CC_EXPORT ScopedReadLockSoftware {
   public:
    ScopedReadLockSoftware(ResourceProvider* resource_provider,
                           ResourceProvider::ResourceId resource_id);
    ~ScopedReadLockSoftware();

    const SkBitmap* sk_bitmap() const { return &sk_bitmap_; }
    GLint wrap_mode() const { return wrap_mode_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    GLint wrap_mode_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(ResourceProvider* resource_provider,
                            ResourceProvider::ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkCanvas* sk_canvas() { return sk_canvas_.get(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    scoped_ptr<SkCanvas> sk_canvas_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

  class Fence : public base::RefCounted<Fence> {
   public:
    Fence() {}
    virtual bool HasPassed() = 0;

   protected:
    friend class base::RefCounted<Fence>;
    virtual ~Fence() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Fence);
  };

  // Acquire pixel buffer for resource. The pixel buffer can be used to
  // set resource pixels without performing unnecessary copying.
  void AcquirePixelBuffer(ResourceId id);
  void ReleasePixelBuffer(ResourceId id);

  // Map/unmap the acquired pixel buffer.
  uint8_t* MapPixelBuffer(ResourceId id);
  void UnmapPixelBuffer(ResourceId id);

  // Asynchronously update pixels from acquired pixel buffer.
  void BeginSetPixels(ResourceId id);
  void ForceSetPixelsToComplete(ResourceId id);
  bool DidSetPixelsComplete(ResourceId id);

  // Acquire and release an image. The image allows direct
  // manipulation of texture memory.
  void AcquireImage(ResourceId id);
  void ReleaseImage(ResourceId id);

  // Maps the acquired image so that its pixels could be modified.
  // Unmap is called when all pixels are set.
  uint8_t* MapImage(ResourceId id);
  void UnmapImage(ResourceId id);

  // Returns the stride for the image.
  int GetImageStride(ResourceId id);

  base::SharedMemory* GetSharedMemory(ResourceId id);

  // For tests only! This prevents detecting uninitialized reads.
  // Use SetPixels or LockForWrite to allocate implicitly.
  void AllocateForTesting(ResourceId id);

  // For tests only!
  void CreateForTesting(ResourceId id);

  GLenum TargetForTesting(ResourceId id);

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(scoped_refptr<Fence> fence) {
    current_read_lock_fence_ = fence;
  }
  Fence* GetReadLockFence() { return current_read_lock_fence_.get(); }

  // Enable read lock fences for a specific resource.
  void EnableReadLockFences(ResourceProvider::ResourceId id, bool enable);

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(ResourceId id);

  static GLint GetActiveTextureUnit(gpu::gles2::GLES2Interface* gl);

 private:
  struct Resource {
    Resource();
    ~Resource();
    Resource(unsigned texture_id,
             gfx::Size size,
             GLenum target,
             GLenum filter,
             GLenum texture_pool,
             GLint wrap_mode,
             TextureUsageHint hint,
             ResourceFormat format);
    Resource(uint8_t* pixels,
             SharedBitmap* bitmap,
             gfx::Size size,
             GLenum filter,
             GLint wrap_mode);

    int child_id;
    unsigned gl_id;
    // Pixel buffer used for set pixels without unnecessary copying.
    unsigned gl_pixel_buffer_id;
    // Query used to determine when asynchronous set pixels complete.
    unsigned gl_upload_query_id;
    TextureMailbox mailbox;
    ReleaseCallback release_callback;
    uint8_t* pixels;
    uint8_t* pixel_buffer;
    int lock_for_read_count;
    int imported_count;
    int exported_count;
    bool locked_for_write;
    bool external;
    bool marked_for_deletion;
    bool pending_set_pixels;
    bool set_pixels_completion_forced;
    bool allocated;
    bool enable_read_lock_fences;
    scoped_refptr<Fence> read_lock_fence;
    gfx::Size size;
    GLenum target;
    // TODO(skyostil): Use a separate sampler object for filter state.
    GLenum original_filter;
    GLenum filter;
    unsigned image_id;
    unsigned bound_image_id;
    bool dirty_image;
    GLenum texture_pool;
    GLint wrap_mode;
    bool lost;
    TextureUsageHint hint;
    ResourceType type;
    ResourceFormat format;
    SharedBitmap* shared_bitmap;
  };
  typedef base::hash_map<ResourceId, Resource> ResourceMap;

  static bool CompareResourceMapIteratorsByChildId(
      const std::pair<ReturnedResource, ResourceMap::iterator>& a,
      const std::pair<ReturnedResource, ResourceMap::iterator>& b);

  struct Child {
    Child();
    ~Child();

    ResourceIdMap child_to_parent_map;
    ResourceIdMap parent_to_child_map;
    ReturnCallback return_callback;
    ResourceIdSet in_use_resources;
    bool marked_for_deletion;
  };
  typedef base::hash_map<int, Child> ChildMap;

  bool ReadLockFenceHasPassed(Resource* resource) {
    return !resource->read_lock_fence.get() ||
           resource->read_lock_fence->HasPassed();
  }

  ResourceProvider(OutputSurface* output_surface,
                   SharedBitmapManager* shared_bitmap_manager,
                   int highp_threshold_min,
                   bool use_rgba_4444_texture_format,
                   size_t id_allocation_chunk_size);

  void CleanUpGLIfNeeded();

  Resource* GetResource(ResourceId id);
  const Resource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id);
  const Resource* LockForWrite(ResourceId id);
  void UnlockForWrite(ResourceId id);
  static void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                           const Resource* resource);

  void TransferResource(gpu::gles2::GLES2Interface* gl,
                        ResourceId id,
                        TransferableResource* resource);
  enum DeleteStyle {
    Normal,
    ForShutdown,
  };
  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);
  void DeleteAndReturnUnusedResourcesToChild(ChildMap::iterator child_it,
                                             DeleteStyle style,
                                             const ResourceIdArray& unused);
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);
  void LazyCreate(Resource* resource);
  void LazyAllocate(Resource* resource);

  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  GLenum BindForSampling(ResourceProvider::ResourceId resource_id,
                         GLenum unit,
                         GLenum filter);

  // Returns NULL if the output_surface_ does not have a ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;

  OutputSurface* output_surface_;
  SharedBitmapManager* shared_bitmap_manager_;
  bool lost_output_surface_;
  int highp_threshold_min_;
  ResourceId next_id_;
  ResourceMap resources_;
  int next_child_;
  ChildMap children_;

  ResourceType default_resource_type_;
  bool use_texture_storage_ext_;
  bool use_texture_usage_hint_;
  bool use_compressed_texture_etc1_;
  scoped_ptr<TextureUploader> texture_uploader_;
  int max_texture_size_;
  ResourceFormat best_texture_format_;

  base::ThreadChecker thread_checker_;

  scoped_refptr<Fence> current_read_lock_fence_;
  bool use_rgba_4444_texture_format_;

  const size_t id_allocation_chunk_size_;
  scoped_ptr<IdAllocator> texture_id_allocator_;
  scoped_ptr<IdAllocator> buffer_id_allocator_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};


// TODO(epenner): Move these format conversions to resource_format.h
// once that builds on mac (npapi.h currently #includes OpenGL.h).
inline unsigned BitsPerPixel(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_bits_per_pixel[RESOURCE_FORMAT_MAX + 1] = {
    32,  // RGBA_8888
    16,  // RGBA_4444
    32,  // BGRA_8888
    8,   // LUMINANCE_8
    16,  // RGB_565,
    4    // ETC1
  };
  return format_bits_per_pixel[format];
}

inline GLenum GLDataType(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_gl_data_type[RESOURCE_FORMAT_MAX + 1] = {
    GL_UNSIGNED_BYTE,           // RGBA_8888
    GL_UNSIGNED_SHORT_4_4_4_4,  // RGBA_4444
    GL_UNSIGNED_BYTE,           // BGRA_8888
    GL_UNSIGNED_BYTE,           // LUMINANCE_8
    GL_UNSIGNED_SHORT_5_6_5,    // RGB_565,
    GL_UNSIGNED_BYTE            // ETC1
  };
  return format_gl_data_type[format];
}

inline GLenum GLDataFormat(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_gl_data_format[RESOURCE_FORMAT_MAX + 1] = {
    GL_RGBA,           // RGBA_8888
    GL_RGBA,           // RGBA_4444
    GL_BGRA_EXT,       // BGRA_8888
    GL_LUMINANCE,      // LUMINANCE_8
    GL_RGB,            // RGB_565
    GL_ETC1_RGB8_OES   // ETC1
  };
  return format_gl_data_format[format];
}

inline GLenum GLInternalFormat(ResourceFormat format) {
  return GLDataFormat(format);
}

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_
