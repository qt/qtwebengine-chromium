// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/gl_renderer.h"

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/context_provider.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/geometry_binding.h"
#include "cc/output/gl_frame_data.h"
#include "cc/output/output_surface.h"
#include "cc/output/render_surface_filters.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/layer_quad.h"
#include "cc/resources/scoped_resource.h"
#include "cc/resources/sync_point_helper.h"
#include "cc/resources/texture_mailbox_deleter.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/proxy.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "third_party/skia/include/gpu/SkGrTexturePixelRef.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"

using WebKit::WebGraphicsContext3D;

namespace cc {

namespace {

// TODO(epenner): This should probably be moved to output surface.
//
// This implements a simple fence based on client side swaps.
// This is to isolate the ResourceProvider from 'frames' which
// it shouldn't need to care about, while still allowing us to
// enforce good texture recycling behavior strictly throughout
// the compositor (don't recycle a texture while it's in use).
class SimpleSwapFence : public ResourceProvider::Fence {
 public:
  SimpleSwapFence() : has_passed_(false) {}
  virtual bool HasPassed() OVERRIDE { return has_passed_; }
  void SetHasPassed() { has_passed_ = true; }
 private:
  virtual ~SimpleSwapFence() {}
  bool has_passed_;
};

bool NeedsIOSurfaceReadbackWorkaround() {
#if defined(OS_MACOSX)
  // This isn't strictly required in DumpRenderTree-mode when Mesa is used,
  // but it doesn't seem to hurt.
  return true;
#else
  return false;
#endif
}

Float4 UVTransform(const TextureDrawQuad* quad) {
  gfx::PointF uv0 = quad->uv_top_left;
  gfx::PointF uv1 = quad->uv_bottom_right;
  Float4 xform = { { uv0.x(), uv0.y(), uv1.x() - uv0.x(), uv1.y() - uv0.y() } };
  if (quad->flipped) {
    xform.data[1] = 1.0f - xform.data[1];
    xform.data[3] = -xform.data[3];
  }
  return xform;
}

Float4 PremultipliedColor(SkColor color) {
  const float factor = 1.0f / 255.0f;
  const float alpha = SkColorGetA(color) * factor;

  Float4 result = { {
      SkColorGetR(color) * factor * alpha,
      SkColorGetG(color) * factor * alpha,
      SkColorGetB(color) * factor * alpha,
      alpha
  } };
  return result;
}

// Smallest unit that impact anti-aliasing output. We use this to
// determine when anti-aliasing is unnecessary.
const float kAntiAliasingEpsilon = 1.0f / 1024.0f;

}  // anonymous namespace

struct GLRenderer::PendingAsyncReadPixels {
  PendingAsyncReadPixels() : buffer(0) {}

  scoped_ptr<CopyOutputRequest> copy_request;
  base::CancelableClosure finished_read_pixels_callback;
  unsigned buffer;

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingAsyncReadPixels);
};

scoped_ptr<GLRenderer> GLRenderer::Create(
    RendererClient* client,
    const LayerTreeSettings* settings,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider,
    TextureMailboxDeleter* texture_mailbox_deleter,
    int highp_threshold_min,
    bool use_skia_gpu_backend) {
  scoped_ptr<GLRenderer> renderer(new GLRenderer(client,
                                                 settings,
                                                 output_surface,
                                                 resource_provider,
                                                 texture_mailbox_deleter,
                                                 highp_threshold_min));
  if (!renderer->Initialize())
    return scoped_ptr<GLRenderer>();
  if (use_skia_gpu_backend) {
    renderer->InitializeGrContext();
    DCHECK(renderer->CanUseSkiaGPUBackend())
        << "Requested Skia GPU backend, but can't use it.";
  }

  return renderer.Pass();
}

GLRenderer::GLRenderer(RendererClient* client,
                       const LayerTreeSettings* settings,
                       OutputSurface* output_surface,
                       ResourceProvider* resource_provider,
                       TextureMailboxDeleter* texture_mailbox_deleter,
                       int highp_threshold_min)
    : DirectRenderer(client, settings, output_surface, resource_provider),
      offscreen_framebuffer_id_(0),
      shared_geometry_quad_(gfx::RectF(-0.5f, -0.5f, 1.0f, 1.0f)),
      context_(output_surface->context_provider()->Context3d()),
      texture_mailbox_deleter_(texture_mailbox_deleter),
      is_backbuffer_discarded_(false),
      discard_backbuffer_when_not_visible_(false),
      is_using_bind_uniform_(false),
      visible_(true),
      is_scissor_enabled_(false),
      stencil_shadow_(false),
      blend_shadow_(false),
      highp_threshold_min_(highp_threshold_min),
      highp_threshold_cache_(0),
      on_demand_tile_raster_resource_id_(0) {
  DCHECK(context_);
}

bool GLRenderer::Initialize() {
  if (!context_->makeContextCurrent())
    return false;

  ContextProvider::Capabilities context_caps =
    output_surface_->context_provider()->ContextCapabilities();

  capabilities_.using_partial_swap =
      settings_->partial_swap_enabled && context_caps.post_sub_buffer;

  capabilities_.using_set_visibility = context_caps.set_visibility;

  DCHECK(!context_caps.iosurface || context_caps.texture_rectangle);

  capabilities_.using_egl_image = context_caps.egl_image_external;

  capabilities_.max_texture_size = resource_provider_->max_texture_size();
  capabilities_.best_texture_format = resource_provider_->best_texture_format();

  // The updater can access textures while the GLRenderer is using them.
  capabilities_.allow_partial_texture_updates = true;

  // Check for texture fast paths. Currently we always use MO8 textures,
  // so we only need to avoid POT textures if we have an NPOT fast-path.
  capabilities_.avoid_pow2_textures = context_caps.fast_npot_mo8_textures;

  capabilities_.using_offscreen_context3d = true;

  capabilities_.using_map_image =
      settings_->use_map_image && context_caps.map_image;

  capabilities_.using_discard_framebuffer =
      context_caps.discard_framebuffer;

  is_using_bind_uniform_ = context_caps.bind_uniform_location;

  if (!InitializeSharedObjects())
    return false;

  // Make sure the viewport and context gets initialized, even if it is to zero.
  ViewportChanged();
  return true;
}

void GLRenderer::InitializeGrContext() {
  skia::RefPtr<GrGLInterface> interface = skia::AdoptRef(
      context_->createGrGLInterface());
  if (!interface)
    return;

  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend,
      reinterpret_cast<GrBackendContext>(interface.get())));
  ReinitializeGrCanvas();
}

GLRenderer::~GLRenderer() {
  while (!pending_async_read_pixels_.empty()) {
    PendingAsyncReadPixels* pending_read = pending_async_read_pixels_.back();
    pending_read->finished_read_pixels_callback.Cancel();
    pending_async_read_pixels_.pop_back();
  }

  CleanupSharedObjects();
}

const RendererCapabilities& GLRenderer::Capabilities() const {
  return capabilities_;
}

WebGraphicsContext3D* GLRenderer::Context() { return context_; }

void GLRenderer::DebugGLCall(WebGraphicsContext3D* context,
                             const char* command,
                             const char* file,
                             int line) {
  unsigned error = context->getError();
  if (error != GL_NO_ERROR)
    LOG(ERROR) << "GL command failed: File: " << file << "\n\tLine " << line
               << "\n\tcommand: " << command << ", error "
               << static_cast<int>(error) << "\n";
}

void GLRenderer::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;

  EnforceMemoryPolicy();

  // TODO(jamesr): Replace setVisibilityCHROMIUM() with an extension to
  // explicitly manage front/backbuffers
  // crbug.com/116049
  if (capabilities_.using_set_visibility)
    context_->setVisibilityCHROMIUM(visible);
}

void GLRenderer::SendManagedMemoryStats(size_t bytes_visible,
                                        size_t bytes_visible_and_nearby,
                                        size_t bytes_allocated) {
  WebKit::WebGraphicsManagedMemoryStats stats;
  stats.bytesVisible = bytes_visible;
  stats.bytesVisibleAndNearby = bytes_visible_and_nearby;
  stats.bytesAllocated = bytes_allocated;
  stats.backbufferRequested = !is_backbuffer_discarded_;
  context_->sendManagedMemoryStatsCHROMIUM(&stats);
}

void GLRenderer::ReleaseRenderPassTextures() { render_pass_textures_.clear(); }

void GLRenderer::ViewportChanged() {
  ReinitializeGrCanvas();
}

void GLRenderer::DiscardPixels(bool has_external_stencil_test,
                               bool draw_rect_covers_full_surface) {
  if (has_external_stencil_test || !draw_rect_covers_full_surface ||
      !capabilities_.using_discard_framebuffer)
    return;
  bool using_default_framebuffer =
      !current_framebuffer_lock_ &&
      output_surface_->capabilities().uses_default_gl_framebuffer;
  GLenum attachments[] = {static_cast<GLenum>(
      using_default_framebuffer ? GL_COLOR_EXT : GL_COLOR_ATTACHMENT0_EXT)};
  context_->discardFramebufferEXT(
      GL_FRAMEBUFFER, arraysize(attachments), attachments);
}

void GLRenderer::ClearFramebuffer(DrawingFrame* frame,
                                  bool has_external_stencil_test) {
  // It's unsafe to clear when we have a stencil test because glClear ignores
  // stencil.
  if (has_external_stencil_test) {
    DCHECK(!frame->current_render_pass->has_transparent_background);
    return;
  }

  // On DEBUG builds, opaque render passes are cleared to blue to easily see
  // regions that were not drawn on the screen.
  if (frame->current_render_pass->has_transparent_background)
    GLC(context_, context_->clearColor(0, 0, 0, 0));
  else
    GLC(context_, context_->clearColor(0, 0, 1, 1));

  bool always_clear = false;
#ifndef NDEBUG
  always_clear = true;
#endif
  if (always_clear || frame->current_render_pass->has_transparent_background) {
    GLbitfield clear_bits = GL_COLOR_BUFFER_BIT;
    // Only the Skia GPU backend uses the stencil buffer.  No need to clear it
    // otherwise.
    if (always_clear || CanUseSkiaGPUBackend()) {
      GLC(context_, context_->clearStencil(0));
      clear_bits |= GL_STENCIL_BUFFER_BIT;
    }
    context_->clear(clear_bits);
  }
}

void GLRenderer::BeginDrawingFrame(DrawingFrame* frame) {
  if (client_->DeviceViewport().IsEmpty())
    return;

  TRACE_EVENT0("cc", "GLRenderer::DrawLayers");

  MakeContextCurrent();

  ReinitializeGLState();
}

void GLRenderer::DoNoOp() {
  GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, 0));
  GLC(context_, context_->flush());
}

void GLRenderer::DoDrawQuad(DrawingFrame* frame, const DrawQuad* quad) {
  DCHECK(quad->rect.Contains(quad->visible_rect));
  if (quad->material != DrawQuad::TEXTURE_CONTENT) {
    FlushTextureQuadCache();
  }

  switch (quad->material) {
    case DrawQuad::INVALID:
      NOTREACHED();
      break;
    case DrawQuad::CHECKERBOARD:
      DrawCheckerboardQuad(frame, CheckerboardDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(frame, DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::IO_SURFACE_CONTENT:
      DrawIOSurfaceQuad(frame, IOSurfaceDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(frame, PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(frame, RenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(frame, SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawStreamVideoQuad(frame, StreamVideoDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TEXTURE_CONTENT:
      EnqueueTextureQuad(frame, TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TILED_CONTENT:
      DrawTileQuad(frame, TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::YUV_VIDEO_CONTENT:
      DrawYUVVideoQuad(frame, YUVVideoDrawQuad::MaterialCast(quad));
      break;
  }
}

void GLRenderer::DrawCheckerboardQuad(const DrawingFrame* frame,
                                      const CheckerboardDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  const TileCheckerboardProgram* program = GetTileCheckerboardProgram();
  DCHECK(program && (program->initialized() || IsContextLost()));
  SetUseProgram(program->program());

  SkColor color = quad->color;
  GLC(Context(),
      Context()->uniform4f(program->fragment_shader().color_location(),
                           SkColorGetR(color) * (1.0f / 255.0f),
                           SkColorGetG(color) * (1.0f / 255.0f),
                           SkColorGetB(color) * (1.0f / 255.0f),
                           1));

  const int checkerboard_width = 16;
  float frequency = 1.0f / checkerboard_width;

  gfx::Rect tile_rect = quad->rect;
  float tex_offset_x = tile_rect.x() % checkerboard_width;
  float tex_offset_y = tile_rect.y() % checkerboard_width;
  float tex_scale_x = tile_rect.width();
  float tex_scale_y = tile_rect.height();
  GLC(Context(),
      Context()->uniform4f(program->fragment_shader().tex_transform_location(),
                           tex_offset_x,
                           tex_offset_y,
                           tex_scale_x,
                           tex_scale_y));

  GLC(Context(),
      Context()->uniform1f(program->fragment_shader().frequency_location(),
                           frequency));

  SetShaderOpacity(quad->opacity(),
                   program->fragment_shader().alpha_location());
  DrawQuadGeometry(frame,
                   quad->quadTransform(),
                   quad->rect,
                   program->vertex_shader().matrix_location());
}

void GLRenderer::DrawDebugBorderQuad(const DrawingFrame* frame,
                                     const DebugBorderDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  static float gl_matrix[16];
  const DebugBorderProgram* program = GetDebugBorderProgram();
  DCHECK(program && (program->initialized() || IsContextLost()));
  SetUseProgram(program->program());

  // Use the full quad_rect for debug quads to not move the edges based on
  // partial swaps.
  gfx::Rect layer_rect = quad->rect;
  gfx::Transform render_matrix = quad->quadTransform();
  render_matrix.Translate(0.5f * layer_rect.width() + layer_rect.x(),
                          0.5f * layer_rect.height() + layer_rect.y());
  render_matrix.Scale(layer_rect.width(), layer_rect.height());
  GLRenderer::ToGLMatrix(&gl_matrix[0],
                         frame->projection_matrix * render_matrix);
  GLC(Context(),
      Context()->uniformMatrix4fv(
          program->vertex_shader().matrix_location(), 1, false, &gl_matrix[0]));

  SkColor color = quad->color;
  float alpha = SkColorGetA(color) * (1.0f / 255.0f);

  GLC(Context(),
      Context()->uniform4f(program->fragment_shader().color_location(),
                           (SkColorGetR(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetG(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetB(color) * (1.0f / 255.0f)) * alpha,
                           alpha));

  GLC(Context(), Context()->lineWidth(quad->width));

  // The indices for the line are stored in the same array as the triangle
  // indices.
  GLC(Context(),
      Context()->drawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, 0));
}

static inline SkBitmap ApplyFilters(GLRenderer* renderer,
                                    ContextProvider* offscreen_contexts,
                                    const FilterOperations& filters,
                                    ScopedResource* source_texture_resource) {
  if (filters.IsEmpty())
    return SkBitmap();

  if (!offscreen_contexts || !offscreen_contexts->GrContext())
    return SkBitmap();

  ResourceProvider::ScopedWriteLockGL lock(renderer->resource_provider(),
                                           source_texture_resource->id());

  // Flush the compositor context to ensure that textures there are available
  // in the shared context.  Do this after locking/creating the compositor
  // texture.
  renderer->resource_provider()->Flush();

  // Make sure skia uses the correct GL context.
  offscreen_contexts->Context3d()->makeContextCurrent();

  SkBitmap source =
      RenderSurfaceFilters::Apply(filters,
                                  lock.texture_id(),
                                  source_texture_resource->size(),
                                  offscreen_contexts->GrContext());

  // Flush skia context so that all the rendered stuff appears on the
  // texture.
  offscreen_contexts->GrContext()->flush();

  // Flush the GL context so rendering results from this context are
  // visible in the compositor's context.
  offscreen_contexts->Context3d()->flush();

  // Use the compositor's GL context again.
  renderer->Context()->makeContextCurrent();
  return source;
}

static SkBitmap ApplyImageFilter(GLRenderer* renderer,
                                 ContextProvider* offscreen_contexts,
                                 gfx::Point origin,
                                 SkImageFilter* filter,
                                 ScopedResource* source_texture_resource) {
  if (!filter)
    return SkBitmap();

  if (!offscreen_contexts || !offscreen_contexts->GrContext())
    return SkBitmap();

  ResourceProvider::ScopedWriteLockGL lock(renderer->resource_provider(),
                                           source_texture_resource->id());

  // Flush the compositor context to ensure that textures there are available
  // in the shared context.  Do this after locking/creating the compositor
  // texture.
  renderer->resource_provider()->Flush();

  // Make sure skia uses the correct GL context.
  offscreen_contexts->Context3d()->makeContextCurrent();

  // Wrap the source texture in a Ganesh platform texture.
  GrBackendTextureDesc backend_texture_description;
  backend_texture_description.fWidth = source_texture_resource->size().width();
  backend_texture_description.fHeight =
      source_texture_resource->size().height();
  backend_texture_description.fConfig = kSkia8888_GrPixelConfig;
  backend_texture_description.fTextureHandle = lock.texture_id();
  backend_texture_description.fOrigin = kBottomLeft_GrSurfaceOrigin;
  skia::RefPtr<GrTexture> texture =
      skia::AdoptRef(offscreen_contexts->GrContext()->wrapBackendTexture(
          backend_texture_description));

  // Place the platform texture inside an SkBitmap.
  SkBitmap source;
  source.setConfig(SkBitmap::kARGB_8888_Config,
                   source_texture_resource->size().width(),
                   source_texture_resource->size().height());
  skia::RefPtr<SkGrPixelRef> pixel_ref =
      skia::AdoptRef(new SkGrPixelRef(texture.get()));
  source.setPixelRef(pixel_ref.get());

  // Create a scratch texture for backing store.
  GrTextureDesc desc;
  desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
  desc.fSampleCnt = 0;
  desc.fWidth = source.width();
  desc.fHeight = source.height();
  desc.fConfig = kSkia8888_GrPixelConfig;
  desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
  GrAutoScratchTexture scratch_texture(
      offscreen_contexts->GrContext(), desc, GrContext::kExact_ScratchTexMatch);
  skia::RefPtr<GrTexture> backing_store =
      skia::AdoptRef(scratch_texture.detach());

  // Create a device and canvas using that backing store.
  SkGpuDevice device(offscreen_contexts->GrContext(), backing_store.get());
  SkCanvas canvas(&device);

  // Draw the source bitmap through the filter to the canvas.
  SkPaint paint;
  paint.setImageFilter(filter);
  canvas.clear(SK_ColorTRANSPARENT);

  // TODO(senorblanco): in addition to the origin translation here, the canvas
  // should also be scaled to accomodate device pixel ratio and pinch zoom. See
  // crbug.com/281516 and crbug.com/281518.
  canvas.translate(SkIntToScalar(-origin.x()), SkIntToScalar(-origin.y()));
  canvas.drawSprite(source, 0, 0, &paint);

  // Flush skia context so that all the rendered stuff appears on the
  // texture.
  offscreen_contexts->GrContext()->flush();

  // Flush the GL context so rendering results from this context are
  // visible in the compositor's context.
  offscreen_contexts->Context3d()->flush();

  // Use the compositor's GL context again.
  renderer->Context()->makeContextCurrent();

  return device.accessBitmap(false);
}

scoped_ptr<ScopedResource> GLRenderer::DrawBackgroundFilters(
    DrawingFrame* frame,
    const RenderPassDrawQuad* quad,
    const gfx::Transform& contents_device_transform,
    const gfx::Transform& contents_device_transform_inverse) {
  // This method draws a background filter, which applies a filter to any pixels
  // behind the quad and seen through its background.  The algorithm works as
  // follows:
  // 1. Compute a bounding box around the pixels that will be visible through
  // the quad.
  // 2. Read the pixels in the bounding box into a buffer R.
  // 3. Apply the background filter to R, so that it is applied in the pixels'
  // coordinate space.
  // 4. Apply the quad's inverse transform to map the pixels in R into the
  // quad's content space. This implicitly clips R by the content bounds of the
  // quad since the destination texture has bounds matching the quad's content.
  // 5. Draw the background texture for the contents using the same transform as
  // used to draw the contents itself. This is done without blending to replace
  // the current background pixels with the new filtered background.
  // 6. Draw the contents of the quad over drop of the new background with
  // blending, as per usual. The filtered background pixels will show through
  // any non-opaque pixels in this draws.
  //
  // Pixel copies in this algorithm occur at steps 2, 3, 4, and 5.

  // TODO(danakj): When this algorithm changes, update
  // LayerTreeHost::PrioritizeTextures() accordingly.

  FilterOperations filters =
      RenderSurfaceFilters::Optimize(quad->background_filters);
  DCHECK(!filters.IsEmpty());

  // TODO(danakj): We only allow background filters on an opaque render surface
  // because other surfaces may contain translucent pixels, and the contents
  // behind those translucent pixels wouldn't have the filter applied.
  if (frame->current_render_pass->has_transparent_background)
    return scoped_ptr<ScopedResource>();
  DCHECK(!frame->current_texture);

  // TODO(danakj): Do a single readback for both the surface and replica and
  // cache the filtered results (once filter textures are not reused).
  gfx::Rect window_rect = gfx::ToEnclosingRect(MathUtil::MapClippedRect(
      contents_device_transform, SharedGeometryQuad().BoundingBox()));

  int top, right, bottom, left;
  filters.GetOutsets(&top, &right, &bottom, &left);
  window_rect.Inset(-left, -top, -right, -bottom);

  window_rect.Intersect(
      MoveFromDrawToWindowSpace(frame->current_render_pass->output_rect));

  scoped_ptr<ScopedResource> device_background_texture =
      ScopedResource::create(resource_provider_);
  if (!device_background_texture->Allocate(window_rect.size(),
                                           ResourceProvider::TextureUsageAny,
                                           RGBA_8888)) {
    return scoped_ptr<ScopedResource>();
  } else {
    ResourceProvider::ScopedWriteLockGL lock(resource_provider_,
                                             device_background_texture->id());
    GetFramebufferTexture(lock.texture_id(),
                          device_background_texture->format(),
                          window_rect);
  }

  SkBitmap filtered_device_background =
      ApplyFilters(this,
                   frame->offscreen_context_provider,
                   filters,
                   device_background_texture.get());
  if (!filtered_device_background.getTexture())
    return scoped_ptr<ScopedResource>();

  GrTexture* texture =
      reinterpret_cast<GrTexture*>(filtered_device_background.getTexture());
  int filtered_device_background_texture_id = texture->getTextureHandle();

  scoped_ptr<ScopedResource> background_texture =
      ScopedResource::create(resource_provider_);
  if (!background_texture->Allocate(quad->rect.size(),
                                    ResourceProvider::TextureUsageFramebuffer,
                                    RGBA_8888))
    return scoped_ptr<ScopedResource>();

  const RenderPass* target_render_pass = frame->current_render_pass;
  bool using_background_texture =
      UseScopedTexture(frame, background_texture.get(), quad->rect);

  if (using_background_texture) {
    // Copy the readback pixels from device to the background texture for the
    // surface.
    gfx::Transform device_to_framebuffer_transform;
    device_to_framebuffer_transform.Translate(
        quad->rect.width() * 0.5f + quad->rect.x(),
        quad->rect.height() * 0.5f + quad->rect.y());
    device_to_framebuffer_transform.Scale(quad->rect.width(),
                                          quad->rect.height());
    device_to_framebuffer_transform.PreconcatTransform(
        contents_device_transform_inverse);

#ifndef NDEBUG
    GLC(Context(), Context()->clearColor(0, 0, 1, 1));
    Context()->clear(GL_COLOR_BUFFER_BIT);
#endif

    // The filtered_deveice_background_texture is oriented the same as the frame
    // buffer. The transform we are copying with has a vertical flip, as well as
    // the |device_to_framebuffer_transform|, which cancel each other out. So do
    // not flip the contents in the shader to maintain orientation.
    bool flip_vertically = false;

    CopyTextureToFramebuffer(frame,
                             filtered_device_background_texture_id,
                             window_rect,
                             device_to_framebuffer_transform,
                             flip_vertically);
  }

  UseRenderPass(frame, target_render_pass);

  if (!using_background_texture)
    return scoped_ptr<ScopedResource>();
  return background_texture.Pass();
}

void GLRenderer::DrawRenderPassQuad(DrawingFrame* frame,
                                    const RenderPassDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  CachedResource* contents_texture =
      render_pass_textures_.get(quad->render_pass_id);
  if (!contents_texture || !contents_texture->id())
    return;

  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, quad->quadTransform(), quad->rect);
  gfx::Transform contents_device_transform =
      frame->window_matrix * frame->projection_matrix * quad_rect_matrix;
  contents_device_transform.FlattenTo2d();

  // Can only draw surface if device matrix is invertible.
  gfx::Transform contents_device_transform_inverse(
      gfx::Transform::kSkipInitialization);
  if (!contents_device_transform.GetInverse(&contents_device_transform_inverse))
    return;

  scoped_ptr<ScopedResource> background_texture;
  if (!quad->background_filters.IsEmpty()) {
    // The pixels from the filtered background should completely replace the
    // current pixel values.
    bool disable_blending = blend_enabled();
    if (disable_blending)
      SetBlendEnabled(false);

    background_texture = DrawBackgroundFilters(
        frame,
        quad,
        contents_device_transform,
        contents_device_transform_inverse);

    if (disable_blending)
      SetBlendEnabled(true);
  }

  // TODO(senorblanco): Cache this value so that we don't have to do it for both
  // the surface and its replica.  Apply filters to the contents texture.
  SkBitmap filter_bitmap;
  SkScalar color_matrix[20];
  bool use_color_matrix = false;
  if (quad->filter) {
    skia::RefPtr<SkColorFilter> cf;

    {
      SkColorFilter* colorfilter_rawptr = NULL;
      quad->filter->asColorFilter(&colorfilter_rawptr);
      cf = skia::AdoptRef(colorfilter_rawptr);
    }

    if (cf && cf->asColorMatrix(color_matrix) && !quad->filter->getInput(0)) {
      // We have a single color matrix as a filter; apply it locally
      // in the compositor.
      use_color_matrix = true;
    } else {
      filter_bitmap = ApplyImageFilter(this,
                                       frame->offscreen_context_provider,
                                       quad->rect.origin(),
                                       quad->filter.get(),
                                       contents_texture);
    }
  } else if (!quad->filters.IsEmpty()) {
    FilterOperations optimized_filters =
        RenderSurfaceFilters::Optimize(quad->filters);

    if ((optimized_filters.size() == 1) &&
        (optimized_filters.at(0).type() == FilterOperation::COLOR_MATRIX)) {
      memcpy(
          color_matrix, optimized_filters.at(0).matrix(), sizeof(color_matrix));
      use_color_matrix = true;
    } else {
      filter_bitmap = ApplyFilters(this,
                                   frame->offscreen_context_provider,
                                   optimized_filters,
                                   contents_texture);
    }
  }

  // Draw the background texture if there is one.
  if (background_texture) {
    DCHECK(background_texture->size() == quad->rect.size());
    ResourceProvider::ScopedReadLockGL lock(resource_provider_,
                                            background_texture->id());

    // The background_texture is oriented the same as the frame buffer. The
    // transform we are copying with has a vertical flip, so flip the contents
    // in the shader to maintain orientation
    bool flip_vertically = true;

    CopyTextureToFramebuffer(frame,
                             lock.texture_id(),
                             quad->rect,
                             quad->quadTransform(),
                             flip_vertically);
  }

  bool clipped = false;
  gfx::QuadF device_quad = MathUtil::MapQuad(
      contents_device_transform, SharedGeometryQuad(), &clipped);
  LayerQuad device_layer_bounds(gfx::QuadF(device_quad.BoundingBox()));
  LayerQuad device_layer_edges(device_quad);

  // Use anti-aliasing programs only when necessary.
  bool use_aa = !clipped &&
      (!device_quad.IsRectilinear() ||
       !gfx::IsNearestRectWithinDistance(device_quad.BoundingBox(),
                                         kAntiAliasingEpsilon));
  if (use_aa) {
    device_layer_bounds.InflateAntiAliasingDistance();
    device_layer_edges.InflateAntiAliasingDistance();
  }

  scoped_ptr<ResourceProvider::ScopedReadLockGL> mask_resource_lock;
  unsigned mask_texture_id = 0;
  if (quad->mask_resource_id) {
    mask_resource_lock.reset(new ResourceProvider::ScopedReadLockGL(
        resource_provider_, quad->mask_resource_id));
    mask_texture_id = mask_resource_lock->texture_id();
  }

  // TODO(danakj): use the background_texture and blend the background in with
  // this draw instead of having a separate copy of the background texture.

  scoped_ptr<ResourceProvider::ScopedReadLockGL> contents_resource_lock;
  if (filter_bitmap.getTexture()) {
    GrTexture* texture =
        reinterpret_cast<GrTexture*>(filter_bitmap.getTexture());
    DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(Context()));
    Context()->bindTexture(GL_TEXTURE_2D, texture->getTextureHandle());
  } else {
    contents_resource_lock = make_scoped_ptr(
        new ResourceProvider::ScopedSamplerGL(resource_provider_,
                                              contents_texture->id(),
                                              GL_TEXTURE_2D,
                                              GL_LINEAR));
  }

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_, &highp_threshold_cache_, highp_threshold_min_,
      quad->shared_quad_state->visible_content_rect.bottom_right());

  int shader_quad_location = -1;
  int shader_edge_location = -1;
  int shader_viewport_location = -1;
  int shader_mask_sampler_location = -1;
  int shader_mask_tex_coord_scale_location = -1;
  int shader_mask_tex_coord_offset_location = -1;
  int shader_matrix_location = -1;
  int shader_alpha_location = -1;
  int shader_color_matrix_location = -1;
  int shader_color_offset_location = -1;
  int shader_tex_transform_location = -1;

  if (use_aa && mask_texture_id && !use_color_matrix) {
    const RenderPassMaskProgramAA* program =
        GetRenderPassMaskProgramAA(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_quad_location = program->vertex_shader().quad_location();
    shader_edge_location = program->vertex_shader().edge_location();
    shader_viewport_location = program->vertex_shader().viewport_location();
    shader_mask_sampler_location =
        program->fragment_shader().mask_sampler_location();
    shader_mask_tex_coord_scale_location =
        program->fragment_shader().mask_tex_coord_scale_location();
    shader_mask_tex_coord_offset_location =
        program->fragment_shader().mask_tex_coord_offset_location();
    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
  } else if (!use_aa && mask_texture_id && !use_color_matrix) {
    const RenderPassMaskProgram* program =
        GetRenderPassMaskProgram(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_mask_sampler_location =
        program->fragment_shader().mask_sampler_location();
    shader_mask_tex_coord_scale_location =
        program->fragment_shader().mask_tex_coord_scale_location();
    shader_mask_tex_coord_offset_location =
        program->fragment_shader().mask_tex_coord_offset_location();
    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
  } else if (use_aa && !mask_texture_id && !use_color_matrix) {
    const RenderPassProgramAA* program =
        GetRenderPassProgramAA(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_quad_location = program->vertex_shader().quad_location();
    shader_edge_location = program->vertex_shader().edge_location();
    shader_viewport_location = program->vertex_shader().viewport_location();
    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
  } else if (use_aa && mask_texture_id && use_color_matrix) {
    const RenderPassMaskColorMatrixProgramAA* program =
        GetRenderPassMaskColorMatrixProgramAA(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_quad_location = program->vertex_shader().quad_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
    shader_edge_location = program->vertex_shader().edge_location();
    shader_viewport_location = program->vertex_shader().viewport_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_mask_sampler_location =
        program->fragment_shader().mask_sampler_location();
    shader_mask_tex_coord_scale_location =
        program->fragment_shader().mask_tex_coord_scale_location();
    shader_mask_tex_coord_offset_location =
        program->fragment_shader().mask_tex_coord_offset_location();
    shader_color_matrix_location =
        program->fragment_shader().color_matrix_location();
    shader_color_offset_location =
        program->fragment_shader().color_offset_location();
  } else if (use_aa && !mask_texture_id && use_color_matrix) {
    const RenderPassColorMatrixProgramAA* program =
        GetRenderPassColorMatrixProgramAA(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_quad_location = program->vertex_shader().quad_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
    shader_edge_location = program->vertex_shader().edge_location();
    shader_viewport_location = program->vertex_shader().viewport_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_color_matrix_location =
        program->fragment_shader().color_matrix_location();
    shader_color_offset_location =
        program->fragment_shader().color_offset_location();
  } else if (!use_aa && mask_texture_id && use_color_matrix) {
    const RenderPassMaskColorMatrixProgram* program =
        GetRenderPassMaskColorMatrixProgram(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
    shader_mask_sampler_location =
        program->fragment_shader().mask_sampler_location();
    shader_mask_tex_coord_scale_location =
        program->fragment_shader().mask_tex_coord_scale_location();
    shader_mask_tex_coord_offset_location =
        program->fragment_shader().mask_tex_coord_offset_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_color_matrix_location =
        program->fragment_shader().color_matrix_location();
    shader_color_offset_location =
        program->fragment_shader().color_offset_location();
  } else if (!use_aa && !mask_texture_id && use_color_matrix) {
    const RenderPassColorMatrixProgram* program =
        GetRenderPassColorMatrixProgram(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_color_matrix_location =
        program->fragment_shader().color_matrix_location();
    shader_color_offset_location =
        program->fragment_shader().color_offset_location();
  } else {
    const RenderPassProgram* program =
        GetRenderPassProgram(tex_coord_precision);
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
  }
  float tex_scale_x =
      quad->rect.width() / static_cast<float>(contents_texture->size().width());
  float tex_scale_y = quad->rect.height() /
                      static_cast<float>(contents_texture->size().height());
  DCHECK_LE(tex_scale_x, 1.0f);
  DCHECK_LE(tex_scale_y, 1.0f);

  DCHECK(shader_tex_transform_location != -1 || IsContextLost());
  // Flip the content vertically in the shader, as the RenderPass input
  // texture is already oriented the same way as the framebuffer, but the
  // projection transform does a flip.
  GLC(Context(), Context()->uniform4f(shader_tex_transform_location,
                                      0.0f,
                                      tex_scale_y,
                                      tex_scale_x,
                                      -tex_scale_y));

  scoped_ptr<ResourceProvider::ScopedReadLockGL> shader_mask_sampler_lock;
  if (shader_mask_sampler_location != -1) {
    DCHECK_NE(shader_mask_tex_coord_scale_location, 1);
    DCHECK_NE(shader_mask_tex_coord_offset_location, 1);
    GLC(Context(), Context()->uniform1i(shader_mask_sampler_location, 1));

    float mask_tex_scale_x = quad->mask_uv_rect.width() / tex_scale_x;
    float mask_tex_scale_y = quad->mask_uv_rect.height() / tex_scale_y;

    // Mask textures are oriented vertically flipped relative to the framebuffer
    // and the RenderPass contents texture, so we flip the tex coords from the
    // RenderPass texture to find the mask texture coords.
    GLC(Context(),
        Context()->uniform2f(shader_mask_tex_coord_offset_location,
                             quad->mask_uv_rect.x(),
                             quad->mask_uv_rect.y() + mask_tex_scale_y));
    GLC(Context(),
        Context()->uniform2f(shader_mask_tex_coord_scale_location,
                             mask_tex_scale_x,
                             -mask_tex_scale_y));
    shader_mask_sampler_lock = make_scoped_ptr(
        new ResourceProvider::ScopedSamplerGL(resource_provider_,
                                              quad->mask_resource_id,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE1,
                                              GL_LINEAR));
  }

  if (shader_edge_location != -1) {
    float edge[24];
    device_layer_edges.ToFloatArray(edge);
    device_layer_bounds.ToFloatArray(&edge[12]);
    GLC(Context(), Context()->uniform3fv(shader_edge_location, 8, edge));
  }

  if (shader_viewport_location != -1) {
    float viewport[4] = {
      static_cast<float>(viewport_.x()),
      static_cast<float>(viewport_.y()),
      static_cast<float>(viewport_.width()),
      static_cast<float>(viewport_.height()),
    };
    GLC(Context(),
        Context()->uniform4fv(shader_viewport_location, 1, viewport));
  }

  if (shader_color_matrix_location != -1) {
    float matrix[16];
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j)
        matrix[i * 4 + j] = SkScalarToFloat(color_matrix[j * 5 + i]);
    }
    GLC(Context(),
        Context()->uniformMatrix4fv(
            shader_color_matrix_location, 1, false, matrix));
  }
  static const float kScale = 1.0f / 255.0f;
  if (shader_color_offset_location != -1) {
    float offset[4];
    for (int i = 0; i < 4; ++i)
      offset[i] = SkScalarToFloat(color_matrix[i * 5 + 4]) * kScale;

    GLC(Context(),
        Context()->uniform4fv(shader_color_offset_location, 1, offset));
  }

  // Map device space quad to surface space. contents_device_transform has no 3d
  // component since it was flattened, so we don't need to project.
  gfx::QuadF surface_quad = MathUtil::MapQuad(contents_device_transform_inverse,
                                              device_layer_edges.ToQuadF(),
                                              &clipped);

  SetShaderOpacity(quad->opacity(), shader_alpha_location);
  SetShaderQuadF(surface_quad, shader_quad_location);
  DrawQuadGeometry(
      frame, quad->quadTransform(), quad->rect, shader_matrix_location);

  // Flush the compositor context before the filter bitmap goes out of
  // scope, so the draw gets processed before the filter texture gets deleted.
  if (filter_bitmap.getTexture())
    context_->flush();
}

struct SolidColorProgramUniforms {
  unsigned program;
  unsigned matrix_location;
  unsigned viewport_location;
  unsigned quad_location;
  unsigned edge_location;
  unsigned color_location;
};

template<class T>
static void SolidColorUniformLocation(T program,
                                      SolidColorProgramUniforms* uniforms) {
  uniforms->program = program->program();
  uniforms->matrix_location = program->vertex_shader().matrix_location();
  uniforms->viewport_location = program->vertex_shader().viewport_location();
  uniforms->quad_location = program->vertex_shader().quad_location();
  uniforms->edge_location = program->vertex_shader().edge_location();
  uniforms->color_location = program->fragment_shader().color_location();
}

// static
bool GLRenderer::SetupQuadForAntialiasing(
    const gfx::Transform& device_transform,
    const DrawQuad* quad,
    gfx::QuadF* local_quad,
    float edge[24]) {
  gfx::Rect tile_rect = quad->visible_rect;

  bool clipped = false;
  gfx::QuadF device_layer_quad = MathUtil::MapQuad(
      device_transform, gfx::QuadF(quad->visibleContentRect()), &clipped);

  bool is_axis_aligned_in_target = device_layer_quad.IsRectilinear();
  bool is_nearest_rect_within_epsilon = is_axis_aligned_in_target &&
      gfx::IsNearestRectWithinDistance(device_layer_quad.BoundingBox(),
                                       kAntiAliasingEpsilon);
  // AAing clipped quads is not supported by the code yet.
  bool use_aa = !clipped && !is_nearest_rect_within_epsilon && quad->IsEdge();
  if (!use_aa)
    return false;

  LayerQuad device_layer_bounds(gfx::QuadF(device_layer_quad.BoundingBox()));
  device_layer_bounds.InflateAntiAliasingDistance();

  LayerQuad device_layer_edges(device_layer_quad);
  device_layer_edges.InflateAntiAliasingDistance();

  device_layer_edges.ToFloatArray(edge);
  device_layer_bounds.ToFloatArray(&edge[12]);

  gfx::PointF bottom_right = tile_rect.bottom_right();
  gfx::PointF bottom_left = tile_rect.bottom_left();
  gfx::PointF top_left = tile_rect.origin();
  gfx::PointF top_right = tile_rect.top_right();

  // Map points to device space.
  bottom_right = MathUtil::MapPoint(device_transform, bottom_right, &clipped);
  DCHECK(!clipped);
  bottom_left = MathUtil::MapPoint(device_transform, bottom_left, &clipped);
  DCHECK(!clipped);
  top_left = MathUtil::MapPoint(device_transform, top_left, &clipped);
  DCHECK(!clipped);
  top_right = MathUtil::MapPoint(device_transform, top_right, &clipped);
  DCHECK(!clipped);

  LayerQuad::Edge bottom_edge(bottom_right, bottom_left);
  LayerQuad::Edge left_edge(bottom_left, top_left);
  LayerQuad::Edge top_edge(top_left, top_right);
  LayerQuad::Edge right_edge(top_right, bottom_right);

  // Only apply anti-aliasing to edges not clipped by culling or scissoring.
  if (quad->IsTopEdge() && tile_rect.y() == quad->rect.y())
    top_edge = device_layer_edges.top();
  if (quad->IsLeftEdge() && tile_rect.x() == quad->rect.x())
    left_edge = device_layer_edges.left();
  if (quad->IsRightEdge() && tile_rect.right() == quad->rect.right())
    right_edge = device_layer_edges.right();
  if (quad->IsBottomEdge() && tile_rect.bottom() == quad->rect.bottom())
    bottom_edge = device_layer_edges.bottom();

  float sign = gfx::QuadF(tile_rect).IsCounterClockwise() ? -1 : 1;
  bottom_edge.scale(sign);
  left_edge.scale(sign);
  top_edge.scale(sign);
  right_edge.scale(sign);

  // Create device space quad.
  LayerQuad device_quad(left_edge, top_edge, right_edge, bottom_edge);

  // Map device space quad to local space. device_transform has no 3d
  // component since it was flattened, so we don't need to project.  We should
  // have already checked that the transform was uninvertible above.
  gfx::Transform inverse_device_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert = device_transform.GetInverse(&inverse_device_transform);
  DCHECK(did_invert);
  *local_quad = MathUtil::MapQuad(
      inverse_device_transform, device_quad.ToQuadF(), &clipped);
  // We should not DCHECK(!clipped) here, because anti-aliasing inflation may
  // cause device_quad to become clipped. To our knowledge this scenario does
  // not need to be handled differently than the unclipped case.

  return true;
}

void GLRenderer::DrawSolidColorQuad(const DrawingFrame* frame,
                                    const SolidColorDrawQuad* quad) {
  gfx::Rect tile_rect = quad->visible_rect;

  SkColor color = quad->color;
  float opacity = quad->opacity();
  float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;

  // Early out if alpha is small enough that quad doesn't contribute to output.
  if (alpha < std::numeric_limits<float>::epsilon() &&
      quad->ShouldDrawWithBlending())
    return;

  gfx::Transform device_transform =
      frame->window_matrix * frame->projection_matrix * quad->quadTransform();
  device_transform.FlattenTo2d();
  if (!device_transform.IsInvertible())
    return;

  gfx::QuadF local_quad = gfx::QuadF(gfx::RectF(tile_rect));
  float edge[24];
  bool use_aa =
      settings_->allow_antialiasing && !quad->force_anti_aliasing_off &&
      SetupQuadForAntialiasing(device_transform, quad, &local_quad, edge);

  SolidColorProgramUniforms uniforms;
  if (use_aa)
    SolidColorUniformLocation(GetSolidColorProgramAA(), &uniforms);
  else
    SolidColorUniformLocation(GetSolidColorProgram(), &uniforms);
  SetUseProgram(uniforms.program);

  GLC(Context(),
      Context()->uniform4f(uniforms.color_location,
                           (SkColorGetR(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetG(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetB(color) * (1.0f / 255.0f)) * alpha,
                           alpha));
  if (use_aa) {
    float viewport[4] = {
      static_cast<float>(viewport_.x()),
      static_cast<float>(viewport_.y()),
      static_cast<float>(viewport_.width()),
      static_cast<float>(viewport_.height()),
    };
    GLC(Context(),
        Context()->uniform4fv(uniforms.viewport_location, 1, viewport));
    GLC(Context(), Context()->uniform3fv(uniforms.edge_location, 8, edge));
  }

  // Enable blending when the quad properties require it or if we decided
  // to use antialiasing.
  SetBlendEnabled(quad->ShouldDrawWithBlending() || use_aa);

  // Normalize to tile_rect.
  local_quad.Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());

  SetShaderQuadF(local_quad, uniforms.quad_location);

  // The transform and vertex data are used to figure out the extents that the
  // un-antialiased quad should have and which vertex this is and the float
  // quad passed in via uniform is the actual geometry that gets used to draw
  // it. This is why this centered rect is used and not the original quad_rect.
  gfx::RectF centered_rect(gfx::PointF(-0.5f * tile_rect.width(),
                                       -0.5f * tile_rect.height()),
                           tile_rect.size());
  DrawQuadGeometry(frame, quad->quadTransform(),
                   centered_rect, uniforms.matrix_location);
}

struct TileProgramUniforms {
  unsigned program;
  unsigned matrix_location;
  unsigned viewport_location;
  unsigned quad_location;
  unsigned edge_location;
  unsigned vertex_tex_transform_location;
  unsigned sampler_location;
  unsigned fragment_tex_transform_location;
  unsigned alpha_location;
};

template <class T>
static void TileUniformLocation(T program, TileProgramUniforms* uniforms) {
  uniforms->program = program->program();
  uniforms->matrix_location = program->vertex_shader().matrix_location();
  uniforms->viewport_location = program->vertex_shader().viewport_location();
  uniforms->quad_location = program->vertex_shader().quad_location();
  uniforms->edge_location = program->vertex_shader().edge_location();
  uniforms->vertex_tex_transform_location =
      program->vertex_shader().vertex_tex_transform_location();

  uniforms->sampler_location = program->fragment_shader().sampler_location();
  uniforms->alpha_location = program->fragment_shader().alpha_location();
  uniforms->fragment_tex_transform_location =
      program->fragment_shader().fragment_tex_transform_location();
}

void GLRenderer::DrawTileQuad(const DrawingFrame* frame,
                              const TileDrawQuad* quad) {
  DrawContentQuad(frame, quad, quad->resource_id);
}

void GLRenderer::DrawContentQuad(const DrawingFrame* frame,
                                 const ContentDrawQuadBase* quad,
                                 ResourceProvider::ResourceId resource_id) {
  gfx::Rect tile_rect = quad->visible_rect;

  gfx::RectF tex_coord_rect = MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, quad->rect, tile_rect);
  float tex_to_geom_scale_x = quad->rect.width() / quad->tex_coord_rect.width();
  float tex_to_geom_scale_y =
      quad->rect.height() / quad->tex_coord_rect.height();

  gfx::RectF clamp_geom_rect(tile_rect);
  gfx::RectF clamp_tex_rect(tex_coord_rect);
  // Clamp texture coordinates to avoid sampling outside the layer
  // by deflating the tile region half a texel or half a texel
  // minus epsilon for one pixel layers. The resulting clamp region
  // is mapped to the unit square by the vertex shader and mapped
  // back to normalized texture coordinates by the fragment shader
  // after being clamped to 0-1 range.
  float tex_clamp_x = std::min(
      0.5f, 0.5f * clamp_tex_rect.width() - kAntiAliasingEpsilon);
  float tex_clamp_y = std::min(
      0.5f, 0.5f * clamp_tex_rect.height() - kAntiAliasingEpsilon);
  float geom_clamp_x = std::min(
      tex_clamp_x * tex_to_geom_scale_x,
      0.5f * clamp_geom_rect.width() - kAntiAliasingEpsilon);
  float geom_clamp_y = std::min(
      tex_clamp_y * tex_to_geom_scale_y,
      0.5f * clamp_geom_rect.height() - kAntiAliasingEpsilon);
  clamp_geom_rect.Inset(geom_clamp_x, geom_clamp_y, geom_clamp_x, geom_clamp_y);
  clamp_tex_rect.Inset(tex_clamp_x, tex_clamp_y, tex_clamp_x, tex_clamp_y);

  // Map clamping rectangle to unit square.
  float vertex_tex_translate_x = -clamp_geom_rect.x() / clamp_geom_rect.width();
  float vertex_tex_translate_y =
      -clamp_geom_rect.y() / clamp_geom_rect.height();
  float vertex_tex_scale_x = tile_rect.width() / clamp_geom_rect.width();
  float vertex_tex_scale_y = tile_rect.height() / clamp_geom_rect.height();

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_, &highp_threshold_cache_, highp_threshold_min_,
      quad->texture_size);

  // Map to normalized texture coordinates.
  gfx::Size texture_size = quad->texture_size;
  float fragment_tex_translate_x = clamp_tex_rect.x() / texture_size.width();
  float fragment_tex_translate_y = clamp_tex_rect.y() / texture_size.height();
  float fragment_tex_scale_x = clamp_tex_rect.width() / texture_size.width();
  float fragment_tex_scale_y = clamp_tex_rect.height() / texture_size.height();

  gfx::Transform device_transform =
      frame->window_matrix * frame->projection_matrix * quad->quadTransform();
  device_transform.FlattenTo2d();
  if (!device_transform.IsInvertible())
    return;

  gfx::QuadF local_quad = gfx::QuadF(gfx::RectF(tile_rect));
  float edge[24];
  bool use_aa = settings_->allow_antialiasing && SetupQuadForAntialiasing(
      device_transform, quad, &local_quad, edge);

  TileProgramUniforms uniforms;
  if (use_aa) {
    if (quad->swizzle_contents) {
      TileUniformLocation(GetTileProgramSwizzleAA(tex_coord_precision),
                          &uniforms);
    } else {
      TileUniformLocation(GetTileProgramAA(tex_coord_precision), &uniforms);
    }
  } else {
    if (quad->ShouldDrawWithBlending()) {
      if (quad->swizzle_contents) {
        TileUniformLocation(GetTileProgramSwizzle(tex_coord_precision),
                            &uniforms);
      } else {
        TileUniformLocation(GetTileProgram(tex_coord_precision), &uniforms);
      }
    } else {
      if (quad->swizzle_contents) {
        TileUniformLocation(GetTileProgramSwizzleOpaque(tex_coord_precision),
                            &uniforms);
      } else {
        TileUniformLocation(GetTileProgramOpaque(tex_coord_precision),
                            &uniforms);
      }
    }
  }

  SetUseProgram(uniforms.program);
  GLC(Context(), Context()->uniform1i(uniforms.sampler_location, 0));
  bool scaled = (tex_to_geom_scale_x != 1.f || tex_to_geom_scale_y != 1.f);
  GLenum filter = (use_aa || scaled ||
                   !quad->quadTransform().IsIdentityOrIntegerTranslation())
                  ? GL_LINEAR
                  : GL_NEAREST;
  ResourceProvider::ScopedSamplerGL quad_resource_lock(
      resource_provider_, resource_id, GL_TEXTURE_2D, filter);

  if (use_aa) {
    float viewport[4] = {
      static_cast<float>(viewport_.x()),
      static_cast<float>(viewport_.y()),
      static_cast<float>(viewport_.width()),
      static_cast<float>(viewport_.height()),
    };
    GLC(Context(),
        Context()->uniform4fv(uniforms.viewport_location, 1, viewport));
    GLC(Context(), Context()->uniform3fv(uniforms.edge_location, 8, edge));

    GLC(Context(),
        Context()->uniform4f(uniforms.vertex_tex_transform_location,
                             vertex_tex_translate_x,
                             vertex_tex_translate_y,
                             vertex_tex_scale_x,
                             vertex_tex_scale_y));
    GLC(Context(),
        Context()->uniform4f(uniforms.fragment_tex_transform_location,
                             fragment_tex_translate_x,
                             fragment_tex_translate_y,
                             fragment_tex_scale_x,
                             fragment_tex_scale_y));
  } else {
    // Move fragment shader transform to vertex shader. We can do this while
    // still producing correct results as fragment_tex_transform_location
    // should always be non-negative when tiles are transformed in a way
    // that could result in sampling outside the layer.
    vertex_tex_scale_x *= fragment_tex_scale_x;
    vertex_tex_scale_y *= fragment_tex_scale_y;
    vertex_tex_translate_x *= fragment_tex_scale_x;
    vertex_tex_translate_y *= fragment_tex_scale_y;
    vertex_tex_translate_x += fragment_tex_translate_x;
    vertex_tex_translate_y += fragment_tex_translate_y;

    GLC(Context(),
        Context()->uniform4f(uniforms.vertex_tex_transform_location,
                             vertex_tex_translate_x,
                             vertex_tex_translate_y,
                             vertex_tex_scale_x,
                             vertex_tex_scale_y));
  }

  // Enable blending when the quad properties require it or if we decided
  // to use antialiasing.
  SetBlendEnabled(quad->ShouldDrawWithBlending() || use_aa);

  // Normalize to tile_rect.
  local_quad.Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());

  SetShaderOpacity(quad->opacity(), uniforms.alpha_location);
  SetShaderQuadF(local_quad, uniforms.quad_location);

  // The transform and vertex data are used to figure out the extents that the
  // un-antialiased quad should have and which vertex this is and the float
  // quad passed in via uniform is the actual geometry that gets used to draw
  // it. This is why this centered rect is used and not the original quad_rect.
  gfx::RectF centered_rect(
      gfx::PointF(-0.5f * tile_rect.width(), -0.5f * tile_rect.height()),
      tile_rect.size());
  DrawQuadGeometry(
      frame, quad->quadTransform(), centered_rect, uniforms.matrix_location);
}

void GLRenderer::DrawYUVVideoQuad(const DrawingFrame* frame,
                                  const YUVVideoDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_, &highp_threshold_cache_, highp_threshold_min_,
      quad->shared_quad_state->visible_content_rect.bottom_right());

  bool use_alpha_plane = quad->a_plane_resource_id != 0;

  ResourceProvider::ScopedSamplerGL y_plane_lock(
      resource_provider_,
      quad->y_plane_resource_id,
      GL_TEXTURE_2D,
      GL_TEXTURE1,
      GL_LINEAR);
  ResourceProvider::ScopedSamplerGL u_plane_lock(
      resource_provider_,
      quad->u_plane_resource_id,
      GL_TEXTURE_2D,
      GL_TEXTURE2,
      GL_LINEAR);
  ResourceProvider::ScopedSamplerGL v_plane_lock(
      resource_provider_,
      quad->v_plane_resource_id,
      GL_TEXTURE_2D,
      GL_TEXTURE3,
      GL_LINEAR);
  scoped_ptr<ResourceProvider::ScopedSamplerGL> a_plane_lock;
  if (use_alpha_plane) {
    a_plane_lock.reset(new ResourceProvider::ScopedSamplerGL(
        resource_provider_,
        quad->a_plane_resource_id,
        GL_TEXTURE_2D,
        GL_TEXTURE4,
        GL_LINEAR));
  }

  int tex_scale_location = -1;
  int matrix_location = -1;
  int y_texture_location = -1;
  int u_texture_location = -1;
  int v_texture_location = -1;
  int a_texture_location = -1;
  int yuv_matrix_location = -1;
  int yuv_adj_location = -1;
  int alpha_location = -1;
  if (use_alpha_plane) {
    const VideoYUVAProgram* program = GetVideoYUVAProgram(tex_coord_precision);
    DCHECK(program && (program->initialized() || IsContextLost()));
    SetUseProgram(program->program());
    tex_scale_location = program->vertex_shader().tex_scale_location();
    matrix_location = program->vertex_shader().matrix_location();
    y_texture_location = program->fragment_shader().y_texture_location();
    u_texture_location = program->fragment_shader().u_texture_location();
    v_texture_location = program->fragment_shader().v_texture_location();
    a_texture_location = program->fragment_shader().a_texture_location();
    yuv_matrix_location = program->fragment_shader().yuv_matrix_location();
    yuv_adj_location = program->fragment_shader().yuv_adj_location();
    alpha_location = program->fragment_shader().alpha_location();
  } else {
    const VideoYUVProgram* program = GetVideoYUVProgram(tex_coord_precision);
    DCHECK(program && (program->initialized() || IsContextLost()));
    SetUseProgram(program->program());
    tex_scale_location = program->vertex_shader().tex_scale_location();
    matrix_location = program->vertex_shader().matrix_location();
    y_texture_location = program->fragment_shader().y_texture_location();
    u_texture_location = program->fragment_shader().u_texture_location();
    v_texture_location = program->fragment_shader().v_texture_location();
    yuv_matrix_location = program->fragment_shader().yuv_matrix_location();
    yuv_adj_location = program->fragment_shader().yuv_adj_location();
    alpha_location = program->fragment_shader().alpha_location();
  }

  GLC(Context(),
      Context()->uniform2f(tex_scale_location,
                           quad->tex_scale.width(),
                           quad->tex_scale.height()));
  GLC(Context(), Context()->uniform1i(y_texture_location, 1));
  GLC(Context(), Context()->uniform1i(u_texture_location, 2));
  GLC(Context(), Context()->uniform1i(v_texture_location, 3));
  if (use_alpha_plane)
    GLC(Context(), Context()->uniform1i(a_texture_location, 4));

  // These values are magic numbers that are used in the transformation from YUV
  // to RGB color values.  They are taken from the following webpage:
  // http://www.fourcc.org/fccyvrgb.php
  float yuv_to_rgb[9] = {
      1.164f, 1.164f, 1.164f,
      0.0f, -.391f, 2.018f,
      1.596f, -.813f, 0.0f,
  };
  GLC(Context(),
      Context()->uniformMatrix3fv(yuv_matrix_location, 1, 0, yuv_to_rgb));

  // These values map to 16, 128, and 128 respectively, and are computed
  // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
  // They are used in the YUV to RGBA conversion formula:
  //   Y - 16   : Gives 16 values of head and footroom for overshooting
  //   U - 128  : Turns unsigned U into signed U [-128,127]
  //   V - 128  : Turns unsigned V into signed V [-128,127]
  float yuv_adjust[3] = { -0.0625f, -0.5f, -0.5f, };
  GLC(Context(), Context()->uniform3fv(yuv_adj_location, 1, yuv_adjust));


  SetShaderOpacity(quad->opacity(), alpha_location);
  DrawQuadGeometry(frame, quad->quadTransform(), quad->rect, matrix_location);
}

void GLRenderer::DrawStreamVideoQuad(const DrawingFrame* frame,
                                     const StreamVideoDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  static float gl_matrix[16];

  DCHECK(capabilities_.using_egl_image);

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_, &highp_threshold_cache_, highp_threshold_min_,
      quad->shared_quad_state->visible_content_rect.bottom_right());

  const VideoStreamTextureProgram* program =
      GetVideoStreamTextureProgram(tex_coord_precision);
  SetUseProgram(program->program());

  ToGLMatrix(&gl_matrix[0], quad->matrix);
  GLC(Context(),
      Context()->uniformMatrix4fv(
          program->vertex_shader().tex_matrix_location(), 1, false, gl_matrix));

  ResourceProvider::ScopedReadLockGL lock(resource_provider_,
                                          quad->resource_id);
  DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(Context()));
  GLC(Context(),
      Context()->bindTexture(GL_TEXTURE_EXTERNAL_OES, lock.texture_id()));

  GLC(Context(),
      Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

  SetShaderOpacity(quad->opacity(),
                   program->fragment_shader().alpha_location());
  DrawQuadGeometry(frame,
                   quad->quadTransform(),
                   quad->rect,
                   program->vertex_shader().matrix_location());
}

void GLRenderer::DrawPictureQuadDirectToBackbuffer(
    const DrawingFrame* frame,
    const PictureDrawQuad* quad) {
  DCHECK(CanUseSkiaGPUBackend());
  DCHECK_EQ(quad->opacity(), 1.f) << "Need to composite to a bitmap or a "
                                     "render surface for non-1 opacity quads";

  // TODO(enne): This should be done more lazily / efficiently.
  gr_context_->resetContext();

  // Reset the canvas matrix to identity because the clip rect is in target
  // space.
  SkMatrix sk_identity;
  sk_identity.setIdentity();
  sk_canvas_->setMatrix(sk_identity);

  if (is_scissor_enabled_) {
    sk_canvas_->clipRect(gfx::RectToSkRect(scissor_rect_),
                         SkRegion::kReplace_Op);
  } else {
    sk_canvas_->clipRect(gfx::RectToSkRect(client_->DeviceViewport()),
                         SkRegion::kReplace_Op);
  }

  gfx::Transform contents_device_transform = frame->window_matrix *
    frame->projection_matrix * quad->quadTransform();
  contents_device_transform.Translate(quad->rect.x(),
                                      quad->rect.y());
  contents_device_transform.FlattenTo2d();
  SkMatrix sk_device_matrix;
  gfx::TransformToFlattenedSkMatrix(contents_device_transform,
                                    &sk_device_matrix);
  sk_canvas_->setMatrix(sk_device_matrix);

  quad->picture_pile->RasterDirect(
      sk_canvas_.get(), quad->content_rect, quad->contents_scale, NULL);

  // Flush any drawing buffers that have been deferred.
  sk_canvas_->flush();

  // TODO(enne): This should be done more lazily / efficiently.
  ReinitializeGLState();
}

void GLRenderer::DrawPictureQuad(const DrawingFrame* frame,
                                 const PictureDrawQuad* quad) {
  if (quad->can_draw_direct_to_backbuffer && CanUseSkiaGPUBackend()) {
    DrawPictureQuadDirectToBackbuffer(frame, quad);
    return;
  }

  if (on_demand_tile_raster_bitmap_.width() != quad->texture_size.width() ||
      on_demand_tile_raster_bitmap_.height() != quad->texture_size.height()) {
    on_demand_tile_raster_bitmap_.setConfig(
        SkBitmap::kARGB_8888_Config,
        quad->texture_size.width(),
        quad->texture_size.height());
    on_demand_tile_raster_bitmap_.allocPixels();

    if (on_demand_tile_raster_resource_id_)
      resource_provider_->DeleteResource(on_demand_tile_raster_resource_id_);

    on_demand_tile_raster_resource_id_ = resource_provider_->CreateGLTexture(
        quad->texture_size,
        GL_TEXTURE_POOL_UNMANAGED_CHROMIUM,
        GL_CLAMP_TO_EDGE,
        ResourceProvider::TextureUsageAny,
        quad->texture_format);
  }

  SkBitmapDevice device(on_demand_tile_raster_bitmap_);
  SkCanvas canvas(&device);

  quad->picture_pile->RasterToBitmap(&canvas, quad->content_rect,
                                     quad->contents_scale, NULL);

  uint8_t* bitmap_pixels = NULL;
  SkBitmap on_demand_tile_raster_bitmap_dest;
  SkBitmap::Config config = SkBitmapConfigFromFormat(quad->texture_format);
  if (on_demand_tile_raster_bitmap_.getConfig() != config) {
    on_demand_tile_raster_bitmap_.copyTo(&on_demand_tile_raster_bitmap_dest,
                                         config);
    // TODO(kaanb): The GL pipeline assumes a 4-byte alignment for the
    // bitmap data. This check will be removed once crbug.com/293728 is fixed.
    CHECK_EQ(0u, on_demand_tile_raster_bitmap_dest.rowBytes() % 4);
    bitmap_pixels = reinterpret_cast<uint8_t*>(
        on_demand_tile_raster_bitmap_dest.getPixels());
  } else {
    bitmap_pixels = reinterpret_cast<uint8_t*>(
        on_demand_tile_raster_bitmap_.getPixels());
  }

  resource_provider_->SetPixels(
      on_demand_tile_raster_resource_id_,
      bitmap_pixels,
      gfx::Rect(quad->texture_size),
      gfx::Rect(quad->texture_size),
      gfx::Vector2d());

  DrawContentQuad(frame, quad, on_demand_tile_raster_resource_id_);
}

struct TextureProgramBinding {
  template <class Program>
  void Set(Program* program, WebKit::WebGraphicsContext3D* context) {
    DCHECK(program && (program->initialized() || context->isContextLost()));
    program_id = program->program();
    sampler_location = program->fragment_shader().sampler_location();
    matrix_location = program->vertex_shader().matrix_location();
    background_color_location =
        program->fragment_shader().background_color_location();
  }
  int program_id;
  int sampler_location;
  int matrix_location;
  int background_color_location;
};

struct TexTransformTextureProgramBinding : TextureProgramBinding {
  template <class Program>
  void Set(Program* program, WebKit::WebGraphicsContext3D* context) {
    TextureProgramBinding::Set(program, context);
    tex_transform_location = program->vertex_shader().tex_transform_location();
    vertex_opacity_location =
        program->vertex_shader().vertex_opacity_location();
  }
  int tex_transform_location;
  int vertex_opacity_location;
};

void GLRenderer::FlushTextureQuadCache() {
  // Check to see if we have anything to draw.
  if (draw_cache_.program_id == 0)
    return;

  // Set the correct blending mode.
  SetBlendEnabled(draw_cache_.needs_blending);

  // Bind the program to the GL state.
  SetUseProgram(draw_cache_.program_id);

  // Bind the correct texture sampler location.
  GLC(Context(), Context()->uniform1i(draw_cache_.sampler_location, 0));

  // Assume the current active textures is 0.
  ResourceProvider::ScopedReadLockGL locked_quad(resource_provider_,
                                                 draw_cache_.resource_id);
  DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(Context()));
  GLC(Context(),
      Context()->bindTexture(GL_TEXTURE_2D, locked_quad.texture_id()));

  COMPILE_ASSERT(
      sizeof(Float4) == 4 * sizeof(float),  // NOLINT(runtime/sizeof)
      struct_is_densely_packed);
  COMPILE_ASSERT(
      sizeof(Float16) == 16 * sizeof(float),  // NOLINT(runtime/sizeof)
      struct_is_densely_packed);

  // Upload the tranforms for both points and uvs.
  GLC(context_,
      context_->uniformMatrix4fv(
          static_cast<int>(draw_cache_.matrix_location),
          static_cast<int>(draw_cache_.matrix_data.size()),
          false,
          reinterpret_cast<float*>(&draw_cache_.matrix_data.front())));
  GLC(context_,
      context_->uniform4fv(
          static_cast<int>(draw_cache_.uv_xform_location),
          static_cast<int>(draw_cache_.uv_xform_data.size()),
          reinterpret_cast<float*>(&draw_cache_.uv_xform_data.front())));

  if (draw_cache_.background_color != SK_ColorTRANSPARENT) {
    Float4 background_color = PremultipliedColor(draw_cache_.background_color);
    GLC(context_,
        context_->uniform4fv(
            draw_cache_.background_color_location, 1, background_color.data));
  }

  GLC(context_,
      context_->uniform1fv(
          static_cast<int>(draw_cache_.vertex_opacity_location),
          static_cast<int>(draw_cache_.vertex_opacity_data.size()),
          static_cast<float*>(&draw_cache_.vertex_opacity_data.front())));

  // Draw the quads!
  GLC(context_,
      context_->drawElements(GL_TRIANGLES,
                             6 * draw_cache_.matrix_data.size(),
                             GL_UNSIGNED_SHORT,
                             0));

  // Clear the cache.
  draw_cache_.program_id = 0;
  draw_cache_.uv_xform_data.resize(0);
  draw_cache_.vertex_opacity_data.resize(0);
  draw_cache_.matrix_data.resize(0);
}

void GLRenderer::EnqueueTextureQuad(const DrawingFrame* frame,
                                    const TextureDrawQuad* quad) {
  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_, &highp_threshold_cache_, highp_threshold_min_,
      quad->shared_quad_state->visible_content_rect.bottom_right());

  // Choose the correct texture program binding
  TexTransformTextureProgramBinding binding;
  if (quad->premultiplied_alpha) {
    if (quad->background_color == SK_ColorTRANSPARENT) {
      binding.Set(GetTextureProgram(tex_coord_precision), Context());
    } else {
      binding.Set(GetTextureBackgroundProgram(tex_coord_precision), Context());
    }
  } else {
    if (quad->background_color == SK_ColorTRANSPARENT) {
      binding.Set(GetNonPremultipliedTextureProgram(tex_coord_precision),
                  Context());
    } else {
      binding.Set(
          GetNonPremultipliedTextureBackgroundProgram(tex_coord_precision),
          Context());
    }
  }

  int resource_id = quad->resource_id;

  if (draw_cache_.program_id != binding.program_id ||
      draw_cache_.resource_id != resource_id ||
      draw_cache_.needs_blending != quad->ShouldDrawWithBlending() ||
      draw_cache_.background_color != quad->background_color ||
      draw_cache_.matrix_data.size() >= 8) {
    FlushTextureQuadCache();
    draw_cache_.program_id = binding.program_id;
    draw_cache_.resource_id = resource_id;
    draw_cache_.needs_blending = quad->ShouldDrawWithBlending();
    draw_cache_.background_color = quad->background_color;

    draw_cache_.uv_xform_location = binding.tex_transform_location;
    draw_cache_.background_color_location = binding.background_color_location;
    draw_cache_.vertex_opacity_location = binding.vertex_opacity_location;
    draw_cache_.matrix_location = binding.matrix_location;
    draw_cache_.sampler_location = binding.sampler_location;
  }

  // Generate the uv-transform
  draw_cache_.uv_xform_data.push_back(UVTransform(quad));

  // Generate the vertex opacity
  const float opacity = quad->opacity();
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[0] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[1] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[2] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[3] * opacity);

  // Generate the transform matrix
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, quad->quadTransform(), quad->rect);
  quad_rect_matrix = frame->projection_matrix * quad_rect_matrix;

  Float16 m;
  quad_rect_matrix.matrix().asColMajorf(m.data);
  draw_cache_.matrix_data.push_back(m);
}

void GLRenderer::DrawIOSurfaceQuad(const DrawingFrame* frame,
                                   const IOSurfaceDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_,  &highp_threshold_cache_, highp_threshold_min_,
      quad->shared_quad_state->visible_content_rect.bottom_right());

  TexTransformTextureProgramBinding binding;
  binding.Set(GetTextureIOSurfaceProgram(tex_coord_precision), Context());

  SetUseProgram(binding.program_id);
  GLC(Context(), Context()->uniform1i(binding.sampler_location, 0));
  if (quad->orientation == IOSurfaceDrawQuad::FLIPPED) {
    GLC(Context(),
        Context()->uniform4f(binding.tex_transform_location,
                             0,
                             quad->io_surface_size.height(),
                             quad->io_surface_size.width(),
                             quad->io_surface_size.height() * -1.0f));
  } else {
    GLC(Context(),
        Context()->uniform4f(binding.tex_transform_location,
                             0,
                             0,
                             quad->io_surface_size.width(),
                             quad->io_surface_size.height()));
  }

  const float vertex_opacity[] = { quad->opacity(), quad->opacity(),
                                   quad->opacity(), quad->opacity() };
  GLC(Context(),
      Context()->uniform1fv(
          binding.vertex_opacity_location, 4, vertex_opacity));

  ResourceProvider::ScopedReadLockGL lock(resource_provider_,
                                          quad->io_surface_resource_id);
  DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(Context()));
  GLC(Context(),
      Context()->bindTexture(GL_TEXTURE_RECTANGLE_ARB,
                             lock.texture_id()));

  DrawQuadGeometry(
      frame, quad->quadTransform(), quad->rect, binding.matrix_location);

  GLC(Context(), Context()->bindTexture(GL_TEXTURE_RECTANGLE_ARB, 0));
}

void GLRenderer::FinishDrawingFrame(DrawingFrame* frame) {
  current_framebuffer_lock_.reset();
  swap_buffer_rect_.Union(gfx::ToEnclosingRect(frame->root_damage_rect));

  GLC(context_, context_->disable(GL_BLEND));
  blend_shadow_ = false;
}

void GLRenderer::FinishDrawingQuadList() { FlushTextureQuadCache(); }

bool GLRenderer::FlippedFramebuffer() const { return true; }

void GLRenderer::EnsureScissorTestEnabled() {
  if (is_scissor_enabled_)
    return;

  FlushTextureQuadCache();
  GLC(context_, context_->enable(GL_SCISSOR_TEST));
  is_scissor_enabled_ = true;
}

void GLRenderer::EnsureScissorTestDisabled() {
  if (!is_scissor_enabled_)
    return;

  FlushTextureQuadCache();
  GLC(context_, context_->disable(GL_SCISSOR_TEST));
  is_scissor_enabled_ = false;
}

void GLRenderer::CopyCurrentRenderPassToBitmap(
    DrawingFrame* frame,
    scoped_ptr<CopyOutputRequest> request) {
  gfx::Rect copy_rect = frame->current_render_pass->output_rect;
  if (request->has_area())
    copy_rect.Intersect(request->area());
  GetFramebufferPixelsAsync(copy_rect, request.Pass());
}

void GLRenderer::ToGLMatrix(float* gl_matrix, const gfx::Transform& transform) {
  transform.matrix().asColMajorf(gl_matrix);
}

void GLRenderer::SetShaderQuadF(const gfx::QuadF& quad, int quad_location) {
  if (quad_location == -1)
    return;

  float gl_quad[8];
  gl_quad[0] = quad.p1().x();
  gl_quad[1] = quad.p1().y();
  gl_quad[2] = quad.p2().x();
  gl_quad[3] = quad.p2().y();
  gl_quad[4] = quad.p3().x();
  gl_quad[5] = quad.p3().y();
  gl_quad[6] = quad.p4().x();
  gl_quad[7] = quad.p4().y();
  GLC(context_, context_->uniform2fv(quad_location, 4, gl_quad));
}

void GLRenderer::SetShaderOpacity(float opacity, int alpha_location) {
  if (alpha_location != -1)
    GLC(context_, context_->uniform1f(alpha_location, opacity));
}

void GLRenderer::SetStencilEnabled(bool enabled) {
  if (enabled == stencil_shadow_)
    return;

  if (enabled)
    GLC(context_, context_->enable(GL_STENCIL_TEST));
  else
    GLC(context_, context_->disable(GL_STENCIL_TEST));
  stencil_shadow_ = enabled;
}

void GLRenderer::SetBlendEnabled(bool enabled) {
  if (enabled == blend_shadow_)
    return;

  if (enabled)
    GLC(context_, context_->enable(GL_BLEND));
  else
    GLC(context_, context_->disable(GL_BLEND));
  blend_shadow_ = enabled;
}

void GLRenderer::SetUseProgram(unsigned program) {
  if (program == program_shadow_)
    return;
  GLC(context_, context_->useProgram(program));
  program_shadow_ = program;
}

void GLRenderer::DrawQuadGeometry(const DrawingFrame* frame,
                                  const gfx::Transform& draw_transform,
                                  const gfx::RectF& quad_rect,
                                  int matrix_location) {
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, draw_transform, quad_rect);
  static float gl_matrix[16];
  ToGLMatrix(&gl_matrix[0], frame->projection_matrix * quad_rect_matrix);
  GLC(context_,
      context_->uniformMatrix4fv(matrix_location, 1, false, &gl_matrix[0]));

  GLC(context_, context_->drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0));
}

void GLRenderer::CopyTextureToFramebuffer(const DrawingFrame* frame,
                                          int texture_id,
                                          gfx::Rect rect,
                                          const gfx::Transform& draw_matrix,
                                          bool flip_vertically) {
  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      context_, &highp_threshold_cache_, highp_threshold_min_,
      rect.bottom_right());

  const RenderPassProgram* program = GetRenderPassProgram(tex_coord_precision);
  SetUseProgram(program->program());

  GLC(Context(), Context()->uniform1i(
      program->fragment_shader().sampler_location(), 0));

  if (flip_vertically) {
    GLC(Context(), Context()->uniform4f(
        program->vertex_shader().tex_transform_location(),
        0.f,
        1.f,
        1.f,
        -1.f));
  } else {
    GLC(Context(), Context()->uniform4f(
        program->vertex_shader().tex_transform_location(),
        0.f,
        0.f,
        1.f,
        1.f));
  }

  SetShaderOpacity(1.f, program->fragment_shader().alpha_location());
  DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(Context()));
  GLC(Context(), Context()->bindTexture(GL_TEXTURE_2D, texture_id));
  DrawQuadGeometry(
      frame, draw_matrix, rect, program->vertex_shader().matrix_location());
}

void GLRenderer::Finish() {
  TRACE_EVENT0("cc", "GLRenderer::finish");
  context_->finish();
}

void GLRenderer::SwapBuffers() {
  DCHECK(visible_);
  DCHECK(!is_backbuffer_discarded_);

  TRACE_EVENT0("cc", "GLRenderer::SwapBuffers");
  // We're done! Time to swapbuffers!

  CompositorFrame compositor_frame;
  compositor_frame.metadata = client_->MakeCompositorFrameMetadata();
  compositor_frame.gl_frame_data = make_scoped_ptr(new GLFrameData);
  compositor_frame.gl_frame_data->size = output_surface_->SurfaceSize();
  if (capabilities_.using_partial_swap) {
    // If supported, we can save significant bandwidth by only swapping the
    // damaged/scissored region (clamped to the viewport)
    swap_buffer_rect_.Intersect(client_->DeviceViewport());
    int flipped_y_pos_of_rect_bottom =
        client_->DeviceViewport().height() - swap_buffer_rect_.y() -
        swap_buffer_rect_.height();
    compositor_frame.gl_frame_data->sub_buffer_rect =
        gfx::Rect(swap_buffer_rect_.x(),
                  flipped_y_pos_of_rect_bottom,
                  swap_buffer_rect_.width(),
                  swap_buffer_rect_.height());
  } else {
    compositor_frame.gl_frame_data->sub_buffer_rect =
        gfx::Rect(output_surface_->SurfaceSize());
  }
  output_surface_->SwapBuffers(&compositor_frame);

  swap_buffer_rect_ = gfx::Rect();

  // We don't have real fences, so we mark read fences as passed
  // assuming a double-buffered GPU pipeline. A texture can be
  // written to after one full frame has past since it was last read.
  if (last_swap_fence_.get())
    static_cast<SimpleSwapFence*>(last_swap_fence_.get())->SetHasPassed();
  last_swap_fence_ = resource_provider_->GetReadLockFence();
  resource_provider_->SetReadLockFence(new SimpleSwapFence());
}

void GLRenderer::SetDiscardBackBufferWhenNotVisible(bool discard) {
  discard_backbuffer_when_not_visible_ = discard;
  EnforceMemoryPolicy();
}

void GLRenderer::EnforceMemoryPolicy() {
  if (!visible_) {
    TRACE_EVENT0("cc", "GLRenderer::EnforceMemoryPolicy dropping resources");
    ReleaseRenderPassTextures();
    if (discard_backbuffer_when_not_visible_)
      DiscardBackbuffer();
    resource_provider_->ReleaseCachedData();
    GLC(context_, context_->flush());
  }
}

void GLRenderer::DiscardBackbuffer() {
  if (is_backbuffer_discarded_)
    return;

  output_surface_->DiscardBackbuffer();

  is_backbuffer_discarded_ = true;

  // Damage tracker needs a full reset every time framebuffer is discarded.
  client_->SetFullRootLayerDamage();
}

void GLRenderer::EnsureBackbuffer() {
  if (!is_backbuffer_discarded_)
    return;

  output_surface_->EnsureBackbuffer();
  is_backbuffer_discarded_ = false;
}

void GLRenderer::GetFramebufferPixels(void* pixels, gfx::Rect rect) {
  if (!pixels || rect.IsEmpty())
    return;

  // This function assumes that it is reading the root frame buffer.
  DCHECK(!current_framebuffer_lock_);

  scoped_ptr<PendingAsyncReadPixels> pending_read(new PendingAsyncReadPixels);
  pending_async_read_pixels_.insert(pending_async_read_pixels_.begin(),
                                    pending_read.Pass());

  // This is a syncronous call since the callback is null.
  gfx::Rect window_rect = MoveFromDrawToWindowSpace(rect);
  DoGetFramebufferPixels(static_cast<uint8*>(pixels),
                         window_rect,
                         AsyncGetFramebufferPixelsCleanupCallback());
}

void GLRenderer::GetFramebufferPixelsAsync(
    gfx::Rect rect, scoped_ptr<CopyOutputRequest> request) {
  DCHECK(!request->IsEmpty());
  if (request->IsEmpty())
    return;
  if (rect.IsEmpty())
    return;

  gfx::Rect window_rect = MoveFromDrawToWindowSpace(rect);

  if (!request->force_bitmap_result()) {
    unsigned int texture_id = context_->createTexture();
    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, texture_id));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GetFramebufferTexture(texture_id, RGBA_8888, window_rect);

    gpu::Mailbox mailbox;
    unsigned sync_point = 0;
    GLC(context_, context_->genMailboxCHROMIUM(mailbox.name));
    if (mailbox.IsZero()) {
      context_->deleteTexture(texture_id);
      request->SendEmptyResult();
      return;
    }

    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, texture_id));
    GLC(context_, context_->produceTextureCHROMIUM(
        GL_TEXTURE_2D, mailbox.name));
    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, 0));
    sync_point = context_->insertSyncPoint();
    TextureMailbox texture_mailbox(mailbox, GL_TEXTURE_2D, sync_point);
    scoped_ptr<SingleReleaseCallback> release_callback =
        texture_mailbox_deleter_->GetReleaseCallback(
            output_surface_->context_provider(), texture_id);
    request->SendTextureResult(window_rect.size(),
                               texture_mailbox,
                               release_callback.Pass());
    return;
  }

  DCHECK(request->force_bitmap_result());

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    window_rect.width(),
                    window_rect.height());
  bitmap->allocPixels();

  scoped_ptr<SkAutoLockPixels> lock(new SkAutoLockPixels(*bitmap));

  // Save a pointer to the pixels, the bitmap is owned by the cleanup_callback.
  uint8* pixels = static_cast<uint8*>(bitmap->getPixels());

  AsyncGetFramebufferPixelsCleanupCallback cleanup_callback = base::Bind(
      &GLRenderer::PassOnSkBitmap,
      base::Unretained(this),
      base::Passed(&bitmap),
      base::Passed(&lock));

  scoped_ptr<PendingAsyncReadPixels> pending_read(new PendingAsyncReadPixels);
  pending_read->copy_request = request.Pass();
  pending_async_read_pixels_.insert(pending_async_read_pixels_.begin(),
                                    pending_read.Pass());

  // This is an asyncronous call since the callback is not null.
  DoGetFramebufferPixels(pixels, window_rect, cleanup_callback);
}

void GLRenderer::DoGetFramebufferPixels(
    uint8* dest_pixels,
    gfx::Rect window_rect,
    const AsyncGetFramebufferPixelsCleanupCallback& cleanup_callback) {
  DCHECK_GE(window_rect.x(), 0);
  DCHECK_GE(window_rect.y(), 0);
  DCHECK_LE(window_rect.right(), current_surface_size_.width());
  DCHECK_LE(window_rect.bottom(), current_surface_size_.height());

  bool is_async = !cleanup_callback.is_null();

  MakeContextCurrent();

  bool do_workaround = NeedsIOSurfaceReadbackWorkaround();

  unsigned temporary_texture = 0;
  unsigned temporary_fbo = 0;

  if (do_workaround) {
    // On Mac OS X, calling glReadPixels() against an FBO whose color attachment
    // is an IOSurface-backed texture causes corruption of future glReadPixels()
    // calls, even those on different OpenGL contexts. It is believed that this
    // is the root cause of top crasher
    // http://crbug.com/99393. <rdar://problem/10949687>

    temporary_texture = context_->createTexture();
    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, temporary_texture));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLC(context_, context_->texParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    // Copy the contents of the current (IOSurface-backed) framebuffer into a
    // temporary texture.
    GetFramebufferTexture(temporary_texture,
                          RGBA_8888,
                          gfx::Rect(current_surface_size_));
    temporary_fbo = context_->createFramebuffer();
    // Attach this texture to an FBO, and perform the readback from that FBO.
    GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, temporary_fbo));
    GLC(context_, context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                                 GL_COLOR_ATTACHMENT0,
                                                 GL_TEXTURE_2D,
                                                 temporary_texture,
                                                 0));

    DCHECK_EQ(static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE),
              context_->checkFramebufferStatus(GL_FRAMEBUFFER));
  }

  unsigned buffer = context_->createBuffer();
  GLC(context_, context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                                     buffer));
  GLC(context_, context_->bufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                                     4 * window_rect.size().GetArea(),
                                     NULL,
                                     GL_STREAM_READ));

  WebKit::WebGLId query = 0;
  if (is_async) {
    query = context_->createQueryEXT();
    GLC(context_, context_->beginQueryEXT(
        GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM,
        query));
  }

  GLC(context_,
      context_->readPixels(window_rect.x(),
                           window_rect.y(),
                           window_rect.width(),
                           window_rect.height(),
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           NULL));

  GLC(context_, context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                                     0));

  if (do_workaround) {
    // Clean up.
    GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, 0));
    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, 0));
    GLC(context_, context_->deleteFramebuffer(temporary_fbo));
    GLC(context_, context_->deleteTexture(temporary_texture));
  }

  base::Closure finished_callback =
      base::Bind(&GLRenderer::FinishedReadback,
                 base::Unretained(this),
                 cleanup_callback,
                 buffer,
                 query,
                 dest_pixels,
                 window_rect.size());
  // Save the finished_callback so it can be cancelled.
  pending_async_read_pixels_.front()->finished_read_pixels_callback.Reset(
      finished_callback);

  // Save the buffer to verify the callbacks happen in the expected order.
  pending_async_read_pixels_.front()->buffer = buffer;

  if (is_async) {
    GLC(context_, context_->endQueryEXT(
        GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM));
    SyncPointHelper::SignalQuery(
        context_,
        query,
        finished_callback);
  } else {
    resource_provider_->Finish();
    finished_callback.Run();
  }

  EnforceMemoryPolicy();
}

void GLRenderer::FinishedReadback(
    const AsyncGetFramebufferPixelsCleanupCallback& cleanup_callback,
    unsigned source_buffer,
    unsigned query,
    uint8* dest_pixels,
    gfx::Size size) {
  DCHECK(!pending_async_read_pixels_.empty());

  if (query != 0) {
    GLC(context_, context_->deleteQueryEXT(query));
  }

  PendingAsyncReadPixels* current_read = pending_async_read_pixels_.back();
  // Make sure we service the readbacks in order.
  DCHECK_EQ(source_buffer, current_read->buffer);

  uint8* src_pixels = NULL;

  if (source_buffer != 0) {
    GLC(context_, context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                                       source_buffer));
    src_pixels = static_cast<uint8*>(
        context_->mapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                                    GL_READ_ONLY));

    if (src_pixels) {
      size_t row_bytes = size.width() * 4;
      int num_rows = size.height();
      size_t total_bytes = num_rows * row_bytes;
      for (size_t dest_y = 0; dest_y < total_bytes; dest_y += row_bytes) {
        // Flip Y axis.
        size_t src_y = total_bytes - dest_y - row_bytes;
        // Swizzle OpenGL -> Skia byte order.
        for (size_t x = 0; x < row_bytes; x += 4) {
          dest_pixels[dest_y + x + SK_R32_SHIFT/8] = src_pixels[src_y + x + 0];
          dest_pixels[dest_y + x + SK_G32_SHIFT/8] = src_pixels[src_y + x + 1];
          dest_pixels[dest_y + x + SK_B32_SHIFT/8] = src_pixels[src_y + x + 2];
          dest_pixels[dest_y + x + SK_A32_SHIFT/8] = src_pixels[src_y + x + 3];
        }
      }

      GLC(context_, context_->unmapBufferCHROMIUM(
          GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM));
    }
    GLC(context_, context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                                       0));
    GLC(context_, context_->deleteBuffer(source_buffer));
  }

  // TODO(danakj): This can go away when synchronous readback is no more and its
  // contents can just move here.
  if (!cleanup_callback.is_null())
    cleanup_callback.Run(current_read->copy_request.Pass(), src_pixels != NULL);

  pending_async_read_pixels_.pop_back();
}

void GLRenderer::PassOnSkBitmap(
    scoped_ptr<SkBitmap> bitmap,
    scoped_ptr<SkAutoLockPixels> lock,
    scoped_ptr<CopyOutputRequest> request,
    bool success) {
  DCHECK(request->force_bitmap_result());

  lock.reset();
  if (success)
    request->SendBitmapResult(bitmap.Pass());
}

void GLRenderer::GetFramebufferTexture(
    unsigned texture_id, ResourceFormat texture_format, gfx::Rect window_rect) {
  DCHECK(texture_id);
  DCHECK_GE(window_rect.x(), 0);
  DCHECK_GE(window_rect.y(), 0);
  DCHECK_LE(window_rect.right(), current_surface_size_.width());
  DCHECK_LE(window_rect.bottom(), current_surface_size_.height());

  GLC(context_, context_->bindTexture(GL_TEXTURE_2D, texture_id));
  GLC(context_,
      context_->copyTexImage2D(
          GL_TEXTURE_2D,
          0,
          ResourceProvider::GetGLDataFormat(texture_format),
          window_rect.x(),
          window_rect.y(),
          window_rect.width(),
          window_rect.height(),
          0));
  GLC(context_, context_->bindTexture(GL_TEXTURE_2D, 0));
}

bool GLRenderer::UseScopedTexture(DrawingFrame* frame,
                                  const ScopedResource* texture,
                                  gfx::Rect viewport_rect) {
  DCHECK(texture->id());
  frame->current_render_pass = NULL;
  frame->current_texture = texture;

  return BindFramebufferToTexture(frame, texture, viewport_rect);
}

void GLRenderer::BindFramebufferToOutputSurface(DrawingFrame* frame) {
  current_framebuffer_lock_.reset();
  output_surface_->BindFramebuffer();

  if (output_surface_->HasExternalStencilTest()) {
    SetStencilEnabled(true);
    GLC(context_, context_->stencilFunc(GL_EQUAL, 1, 1));
  } else {
    SetStencilEnabled(false);
  }
}

bool GLRenderer::BindFramebufferToTexture(DrawingFrame* frame,
                                          const ScopedResource* texture,
                                          gfx::Rect target_rect) {
  DCHECK(texture->id());

  current_framebuffer_lock_.reset();

  SetStencilEnabled(false);
  GLC(context_,
      context_->bindFramebuffer(GL_FRAMEBUFFER, offscreen_framebuffer_id_));
  current_framebuffer_lock_ =
      make_scoped_ptr(new ResourceProvider::ScopedWriteLockGL(
          resource_provider_, texture->id()));
  unsigned texture_id = current_framebuffer_lock_->texture_id();
  GLC(context_,
      context_->framebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0));

  DCHECK(context_->checkFramebufferStatus(GL_FRAMEBUFFER) ==
         GL_FRAMEBUFFER_COMPLETE || IsContextLost());

  InitializeViewport(frame,
                     target_rect,
                     gfx::Rect(target_rect.size()),
                     target_rect.size());
  return true;
}

void GLRenderer::SetScissorTestRect(gfx::Rect scissor_rect) {
  EnsureScissorTestEnabled();

  // Don't unnecessarily ask the context to change the scissor, because it
  // may cause undesired GPU pipeline flushes.
  if (scissor_rect == scissor_rect_)
    return;

  scissor_rect_ = scissor_rect;
  FlushTextureQuadCache();
  GLC(context_,
      context_->scissor(scissor_rect.x(),
                        scissor_rect.y(),
                        scissor_rect.width(),
                        scissor_rect.height()));
}

void GLRenderer::SetDrawViewport(gfx::Rect window_space_viewport) {
  viewport_ = window_space_viewport;
  GLC(context_, context_->viewport(window_space_viewport.x(),
                                   window_space_viewport.y(),
                                   window_space_viewport.width(),
                                   window_space_viewport.height()));
}

bool GLRenderer::MakeContextCurrent() { return context_->makeContextCurrent(); }

bool GLRenderer::InitializeSharedObjects() {
  TRACE_EVENT0("cc", "GLRenderer::InitializeSharedObjects");
  MakeContextCurrent();

  // Create an FBO for doing offscreen rendering.
  GLC(context_, offscreen_framebuffer_id_ = context_->createFramebuffer());

  // We will always need these programs to render, so create the programs
  // eagerly so that the shader compilation can start while we do other work.
  // Other programs are created lazily on first access.
  shared_geometry_ = make_scoped_ptr(
      new GeometryBinding(context_, QuadVertexRect()));
  render_pass_program_ = make_scoped_ptr(
      new RenderPassProgram(context_, TexCoordPrecisionMedium));
  render_pass_program_highp_ = make_scoped_ptr(
      new RenderPassProgram(context_, TexCoordPrecisionHigh));
  tile_program_ = make_scoped_ptr(
      new TileProgram(context_, TexCoordPrecisionMedium));
  tile_program_opaque_ = make_scoped_ptr(
      new TileProgramOpaque(context_, TexCoordPrecisionMedium));
  tile_program_highp_ = make_scoped_ptr(
      new TileProgram(context_, TexCoordPrecisionHigh));
  tile_program_opaque_highp_ = make_scoped_ptr(
      new TileProgramOpaque(context_, TexCoordPrecisionHigh));

  GLC(context_, context_->flush());

  return true;
}

const GLRenderer::TileCheckerboardProgram*
GLRenderer::GetTileCheckerboardProgram() {
  if (!tile_checkerboard_program_)
    tile_checkerboard_program_ = make_scoped_ptr(
        new TileCheckerboardProgram(context_, TexCoordPrecisionNA));
  if (!tile_checkerboard_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::checkerboardProgram::initalize");
    tile_checkerboard_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_checkerboard_program_.get();
}

const GLRenderer::DebugBorderProgram* GLRenderer::GetDebugBorderProgram() {
  if (!debug_border_program_)
    debug_border_program_ = make_scoped_ptr(
        new DebugBorderProgram(context_, TexCoordPrecisionNA));
  if (!debug_border_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::debugBorderProgram::initialize");
    debug_border_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return debug_border_program_.get();
}

const GLRenderer::SolidColorProgram* GLRenderer::GetSolidColorProgram() {
  if (!solid_color_program_)
    solid_color_program_ = make_scoped_ptr(
        new SolidColorProgram(context_, TexCoordPrecisionNA));
  if (!solid_color_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::solidColorProgram::initialize");
    solid_color_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return solid_color_program_.get();
}

const GLRenderer::SolidColorProgramAA* GLRenderer::GetSolidColorProgramAA() {
  if (!solid_color_program_aa_) {
    solid_color_program_aa_ =
        make_scoped_ptr(new SolidColorProgramAA(context_, TexCoordPrecisionNA));
  }
  if (!solid_color_program_aa_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::solidColorProgramAA::initialize");
    solid_color_program_aa_->Initialize(context_, is_using_bind_uniform_);
  }
  return solid_color_program_aa_.get();
}

const GLRenderer::RenderPassProgram* GLRenderer::GetRenderPassProgram(
      TexCoordPrecision precision) {
  scoped_ptr<RenderPassProgram>& program =
      (precision == TexCoordPrecisionHigh) ? render_pass_program_highp_
                                           : render_pass_program_;
  DCHECK(program);
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassProgramAA* GLRenderer::GetRenderPassProgramAA(
      TexCoordPrecision precision) {
  scoped_ptr<RenderPassProgramAA>& program =
      (precision == TexCoordPrecisionHigh) ? render_pass_program_aa_highp_
                                           : render_pass_program_aa_;
  if (!program)
    program =
        make_scoped_ptr(new RenderPassProgramAA(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassProgramAA::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassMaskProgram*
GLRenderer::GetRenderPassMaskProgram(TexCoordPrecision precision) {
  scoped_ptr<RenderPassMaskProgram>& program =
      (precision == TexCoordPrecisionHigh) ? render_pass_mask_program_highp_
                                           : render_pass_mask_program_;
  if (!program)
    program = make_scoped_ptr(new RenderPassMaskProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassMaskProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassMaskProgramAA*
GLRenderer::GetRenderPassMaskProgramAA(TexCoordPrecision precision) {
  scoped_ptr<RenderPassMaskProgramAA>& program =
      (precision == TexCoordPrecisionHigh) ? render_pass_mask_program_aa_highp_
                                           : render_pass_mask_program_aa_;
  if (!program)
    program =
        make_scoped_ptr(new RenderPassMaskProgramAA(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassMaskProgramAA::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassColorMatrixProgram*
GLRenderer::GetRenderPassColorMatrixProgram(TexCoordPrecision precision) {
  scoped_ptr<RenderPassColorMatrixProgram>& program =
      (precision == TexCoordPrecisionHigh) ?
          render_pass_color_matrix_program_highp_ :
          render_pass_color_matrix_program_;
  if (!program)
    program = make_scoped_ptr(
        new RenderPassColorMatrixProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassColorMatrixProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassColorMatrixProgramAA*
GLRenderer::GetRenderPassColorMatrixProgramAA(TexCoordPrecision precision) {
  scoped_ptr<RenderPassColorMatrixProgramAA>& program =
      (precision == TexCoordPrecisionHigh) ?
          render_pass_color_matrix_program_aa_highp_ :
          render_pass_color_matrix_program_aa_;
  if (!program)
    program = make_scoped_ptr(
        new RenderPassColorMatrixProgramAA(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc",
                 "GLRenderer::renderPassColorMatrixProgramAA::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassMaskColorMatrixProgram*
GLRenderer::GetRenderPassMaskColorMatrixProgram(TexCoordPrecision precision) {
  scoped_ptr<RenderPassMaskColorMatrixProgram>& program =
      (precision == TexCoordPrecisionHigh) ?
          render_pass_mask_color_matrix_program_highp_ :
          render_pass_mask_color_matrix_program_;
  if (!program)
    program = make_scoped_ptr(
        new RenderPassMaskColorMatrixProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc",
                 "GLRenderer::renderPassMaskColorMatrixProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::RenderPassMaskColorMatrixProgramAA*
GLRenderer::GetRenderPassMaskColorMatrixProgramAA(TexCoordPrecision precision) {
  scoped_ptr<RenderPassMaskColorMatrixProgramAA>& program =
      (precision == TexCoordPrecisionHigh) ?
          render_pass_mask_color_matrix_program_aa_highp_ :
          render_pass_mask_color_matrix_program_aa_;
  if (!program)
    program = make_scoped_ptr(
        new RenderPassMaskColorMatrixProgramAA(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc",
                 "GLRenderer::renderPassMaskColorMatrixProgramAA::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TileProgram* GLRenderer::GetTileProgram(
    TexCoordPrecision precision) {
  scoped_ptr<TileProgram>& program =
      (precision == TexCoordPrecisionHigh) ? tile_program_highp_
                                           : tile_program_;
  DCHECK(program);
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TileProgramOpaque* GLRenderer::GetTileProgramOpaque(
    TexCoordPrecision precision) {
  scoped_ptr<TileProgramOpaque>& program =
      (precision == TexCoordPrecisionHigh) ? tile_program_opaque_highp_
                                           : tile_program_opaque_;
  DCHECK(program);
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramOpaque::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TileProgramAA* GLRenderer::GetTileProgramAA(
    TexCoordPrecision precision) {
  scoped_ptr<TileProgramAA>& program =
      (precision == TexCoordPrecisionHigh) ? tile_program_aa_highp_
                                           : tile_program_aa_;
  if (!program)
    program = make_scoped_ptr(new TileProgramAA(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramAA::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TileProgramSwizzle* GLRenderer::GetTileProgramSwizzle(
    TexCoordPrecision precision) {
  scoped_ptr<TileProgramSwizzle>& program =
      (precision == TexCoordPrecisionHigh) ? tile_program_swizzle_highp_
                                           : tile_program_swizzle_;
  if (!program)
    program = make_scoped_ptr(new TileProgramSwizzle(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzle::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TileProgramSwizzleOpaque*
GLRenderer::GetTileProgramSwizzleOpaque(TexCoordPrecision precision) {
  scoped_ptr<TileProgramSwizzleOpaque>& program =
      (precision == TexCoordPrecisionHigh) ? tile_program_swizzle_opaque_highp_
                                           : tile_program_swizzle_opaque_;
  if (!program)
    program = make_scoped_ptr(
        new TileProgramSwizzleOpaque(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzleOpaque::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TileProgramSwizzleAA* GLRenderer::GetTileProgramSwizzleAA(
    TexCoordPrecision precision) {
  scoped_ptr<TileProgramSwizzleAA>& program =
      (precision == TexCoordPrecisionHigh) ? tile_program_swizzle_aa_highp_
                                           : tile_program_swizzle_aa_;
  if (!program)
    program = make_scoped_ptr(new TileProgramSwizzleAA(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzleAA::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TextureProgram* GLRenderer::GetTextureProgram(
    TexCoordPrecision precision) {
  scoped_ptr<TextureProgram>& program =
      (precision == TexCoordPrecisionHigh) ? texture_program_highp_
                                           : texture_program_;
  if (!program)
    program = make_scoped_ptr(new TextureProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::textureProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::NonPremultipliedTextureProgram*
    GLRenderer::GetNonPremultipliedTextureProgram(TexCoordPrecision precision) {
  scoped_ptr<NonPremultipliedTextureProgram>& program =
      (precision == TexCoordPrecisionHigh) ?
         nonpremultiplied_texture_program_highp_ :
         nonpremultiplied_texture_program_;
  if (!program) {
    program = make_scoped_ptr(
        new NonPremultipliedTextureProgram(context_, precision));
  }
  if (!program->initialized()) {
    TRACE_EVENT0("cc",
                 "GLRenderer::NonPremultipliedTextureProgram::Initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TextureBackgroundProgram*
GLRenderer::GetTextureBackgroundProgram(TexCoordPrecision precision) {
  scoped_ptr<TextureBackgroundProgram>& program =
      (precision == TexCoordPrecisionHigh) ? texture_background_program_highp_
                                           : texture_background_program_;
  if (!program) {
    program = make_scoped_ptr(
        new TextureBackgroundProgram(context_, precision));
  }
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::textureProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::NonPremultipliedTextureBackgroundProgram*
GLRenderer::GetNonPremultipliedTextureBackgroundProgram(
    TexCoordPrecision precision) {
  scoped_ptr<NonPremultipliedTextureBackgroundProgram>& program =
      (precision == TexCoordPrecisionHigh) ?
         nonpremultiplied_texture_background_program_highp_ :
         nonpremultiplied_texture_background_program_;
  if (!program) {
    program = make_scoped_ptr(
        new NonPremultipliedTextureBackgroundProgram(context_, precision));
  }
  if (!program->initialized()) {
    TRACE_EVENT0("cc",
                 "GLRenderer::NonPremultipliedTextureProgram::Initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::TextureIOSurfaceProgram*
GLRenderer::GetTextureIOSurfaceProgram(TexCoordPrecision precision) {
  scoped_ptr<TextureIOSurfaceProgram>& program =
      (precision == TexCoordPrecisionHigh) ? texture_io_surface_program_highp_
                                           : texture_io_surface_program_;
  if (!program)
    program =
        make_scoped_ptr(new TextureIOSurfaceProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::textureIOSurfaceProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::VideoYUVProgram* GLRenderer::GetVideoYUVProgram(
    TexCoordPrecision precision) {
  scoped_ptr<VideoYUVProgram>& program =
      (precision == TexCoordPrecisionHigh) ? video_yuv_program_highp_
                                           : video_yuv_program_;
  if (!program)
    program = make_scoped_ptr(new VideoYUVProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::videoYUVProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::VideoYUVAProgram* GLRenderer::GetVideoYUVAProgram(
    TexCoordPrecision precision) {
  scoped_ptr<VideoYUVAProgram>& program =
      (precision == TexCoordPrecisionHigh) ? video_yuva_program_highp_
                                           : video_yuva_program_;
  if (!program)
    program = make_scoped_ptr(new VideoYUVAProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::videoYUVAProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

const GLRenderer::VideoStreamTextureProgram*
GLRenderer::GetVideoStreamTextureProgram(TexCoordPrecision precision) {
  if (!Capabilities().using_egl_image)
    return NULL;
  scoped_ptr<VideoStreamTextureProgram>& program =
      (precision == TexCoordPrecisionHigh) ? video_stream_texture_program_highp_
                                           : video_stream_texture_program_;
  if (!program)
    program =
        make_scoped_ptr(new VideoStreamTextureProgram(context_, precision));
  if (!program->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::streamTextureProgram::initialize");
    program->Initialize(context_, is_using_bind_uniform_);
  }
  return program.get();
}

void GLRenderer::CleanupSharedObjects() {
  MakeContextCurrent();

  shared_geometry_.reset();

  if (tile_program_)
    tile_program_->Cleanup(context_);
  if (tile_program_opaque_)
    tile_program_opaque_->Cleanup(context_);
  if (tile_program_swizzle_)
    tile_program_swizzle_->Cleanup(context_);
  if (tile_program_swizzle_opaque_)
    tile_program_swizzle_opaque_->Cleanup(context_);
  if (tile_program_aa_)
    tile_program_aa_->Cleanup(context_);
  if (tile_program_swizzle_aa_)
    tile_program_swizzle_aa_->Cleanup(context_);
  if (tile_checkerboard_program_)
    tile_checkerboard_program_->Cleanup(context_);

  if (tile_program_highp_)
    tile_program_highp_->Cleanup(context_);
  if (tile_program_opaque_highp_)
    tile_program_opaque_highp_->Cleanup(context_);
  if (tile_program_swizzle_highp_)
    tile_program_swizzle_highp_->Cleanup(context_);
  if (tile_program_swizzle_opaque_highp_)
    tile_program_swizzle_opaque_highp_->Cleanup(context_);
  if (tile_program_aa_highp_)
    tile_program_aa_highp_->Cleanup(context_);
  if (tile_program_swizzle_aa_highp_)
    tile_program_swizzle_aa_highp_->Cleanup(context_);

  if (render_pass_mask_program_)
    render_pass_mask_program_->Cleanup(context_);
  if (render_pass_program_)
    render_pass_program_->Cleanup(context_);
  if (render_pass_mask_program_aa_)
    render_pass_mask_program_aa_->Cleanup(context_);
  if (render_pass_program_aa_)
    render_pass_program_aa_->Cleanup(context_);
  if (render_pass_color_matrix_program_)
    render_pass_color_matrix_program_->Cleanup(context_);
  if (render_pass_mask_color_matrix_program_aa_)
    render_pass_mask_color_matrix_program_aa_->Cleanup(context_);
  if (render_pass_color_matrix_program_aa_)
    render_pass_color_matrix_program_aa_->Cleanup(context_);
  if (render_pass_mask_color_matrix_program_)
    render_pass_mask_color_matrix_program_->Cleanup(context_);

  if (render_pass_mask_program_highp_)
    render_pass_mask_program_highp_->Cleanup(context_);
  if (render_pass_program_highp_)
    render_pass_program_highp_->Cleanup(context_);
  if (render_pass_mask_program_aa_highp_)
    render_pass_mask_program_aa_highp_->Cleanup(context_);
  if (render_pass_program_aa_highp_)
    render_pass_program_aa_highp_->Cleanup(context_);
  if (render_pass_color_matrix_program_highp_)
    render_pass_color_matrix_program_highp_->Cleanup(context_);
  if (render_pass_mask_color_matrix_program_aa_highp_)
    render_pass_mask_color_matrix_program_aa_highp_->Cleanup(context_);
  if (render_pass_color_matrix_program_aa_highp_)
    render_pass_color_matrix_program_aa_highp_->Cleanup(context_);
  if (render_pass_mask_color_matrix_program_highp_)
    render_pass_mask_color_matrix_program_highp_->Cleanup(context_);

  if (texture_program_)
    texture_program_->Cleanup(context_);
  if (nonpremultiplied_texture_program_)
    nonpremultiplied_texture_program_->Cleanup(context_);
  if (texture_background_program_)
    texture_background_program_->Cleanup(context_);
  if (nonpremultiplied_texture_background_program_)
    nonpremultiplied_texture_background_program_->Cleanup(context_);
  if (texture_io_surface_program_)
    texture_io_surface_program_->Cleanup(context_);

  if (texture_program_highp_)
    texture_program_highp_->Cleanup(context_);
  if (nonpremultiplied_texture_program_highp_)
    nonpremultiplied_texture_program_highp_->Cleanup(context_);
  if (texture_background_program_highp_)
    texture_background_program_highp_->Cleanup(context_);
  if (nonpremultiplied_texture_background_program_highp_)
    nonpremultiplied_texture_background_program_highp_->Cleanup(context_);
  if (texture_io_surface_program_highp_)
    texture_io_surface_program_highp_->Cleanup(context_);

  if (video_yuv_program_)
    video_yuv_program_->Cleanup(context_);
  if (video_yuva_program_)
    video_yuva_program_->Cleanup(context_);
  if (video_stream_texture_program_)
    video_stream_texture_program_->Cleanup(context_);

  if (video_yuv_program_highp_)
    video_yuv_program_highp_->Cleanup(context_);
  if (video_yuva_program_highp_)
    video_yuva_program_highp_->Cleanup(context_);
  if (video_stream_texture_program_highp_)
    video_stream_texture_program_highp_->Cleanup(context_);

  if (debug_border_program_)
    debug_border_program_->Cleanup(context_);
  if (solid_color_program_)
    solid_color_program_->Cleanup(context_);
  if (solid_color_program_aa_)
    solid_color_program_aa_->Cleanup(context_);

  if (offscreen_framebuffer_id_)
    GLC(context_, context_->deleteFramebuffer(offscreen_framebuffer_id_));

  if (on_demand_tile_raster_resource_id_)
    resource_provider_->DeleteResource(on_demand_tile_raster_resource_id_);

  ReleaseRenderPassTextures();
}

void GLRenderer::ReinitializeGrCanvas() {
  if (!CanUseSkiaGPUBackend())
    return;

  GrBackendRenderTargetDesc desc;
  desc.fWidth = client_->DeviceViewport().width();
  desc.fHeight = client_->DeviceViewport().height();
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fSampleCnt = 1;
  desc.fStencilBits = 8;
  desc.fRenderTargetHandle = 0;

  skia::RefPtr<GrSurface> surface(
      skia::AdoptRef(gr_context_->wrapBackendRenderTarget(desc)));
  skia::RefPtr<SkBaseDevice> device(
      skia::AdoptRef(SkGpuDevice::Create(surface.get())));
  sk_canvas_ = skia::AdoptRef(new SkCanvas(device.get()));
}

void GLRenderer::ReinitializeGLState() {
  // Bind the common vertex attributes used for drawing all the layers.
  shared_geometry_->PrepareForDraw();

  GLC(context_, context_->disable(GL_DEPTH_TEST));
  GLC(context_, context_->disable(GL_CULL_FACE));
  GLC(context_, context_->colorMask(true, true, true, true));
  GLC(context_, context_->disable(GL_STENCIL_TEST));
  stencil_shadow_ = false;
  GLC(context_, context_->enable(GL_BLEND));
  blend_shadow_ = true;
  GLC(context_, context_->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
  GLC(context_, context_->activeTexture(GL_TEXTURE0));
  program_shadow_ = 0;

  // Make sure scissoring starts as disabled.
  is_scissor_enabled_ = false;
  GLC(context_, context_->disable(GL_SCISSOR_TEST));
}

bool GLRenderer::CanUseSkiaGPUBackend() const {
  // The Skia GPU backend requires a stencil buffer.  See ReinitializeGrCanvas
  // implementation.
  return gr_context_ && context_->getContextAttributes().stencil;
}

bool GLRenderer::IsContextLost() {
  return (context_->getGraphicsResetStatusARB() != GL_NO_ERROR);
}

}  // namespace cc
