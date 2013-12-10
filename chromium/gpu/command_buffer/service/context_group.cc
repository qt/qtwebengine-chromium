// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
namespace gles2 {

ContextGroup::ContextGroup(
    MailboxManager* mailbox_manager,
    ImageManager* image_manager,
    MemoryTracker* memory_tracker,
    StreamTextureManager* stream_texture_manager,
    bool bind_generates_resource)
    : mailbox_manager_(mailbox_manager ? mailbox_manager : new MailboxManager),
      image_manager_(image_manager ? image_manager : new ImageManager),
      memory_tracker_(memory_tracker),
      stream_texture_manager_(stream_texture_manager),
      enforce_gl_minimums_(CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnforceGLMinimums)),
      bind_generates_resource_(bind_generates_resource),
      max_vertex_attribs_(0u),
      max_texture_units_(0u),
      max_texture_image_units_(0u),
      max_vertex_texture_image_units_(0u),
      max_fragment_uniform_vectors_(0u),
      max_varying_vectors_(0u),
      max_vertex_uniform_vectors_(0u),
      max_color_attachments_(1u),
      max_draw_buffers_(1u),
      program_cache_(NULL),
      feature_info_(new FeatureInfo()),
      draw_buffer_(GL_BACK) {
  {
    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    manager->Initialize();
  }

  id_namespaces_[id_namespaces::kBuffers].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kFramebuffers].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kProgramsAndShaders].reset(
      new NonReusedIdAllocator);
  id_namespaces_[id_namespaces::kRenderbuffers].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kTextures].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kQueries].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kVertexArrays].reset(new IdAllocator);
}

static void GetIntegerv(GLenum pname, uint32* var) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *var = value;
}

bool ContextGroup::Initialize(
    GLES2Decoder* decoder,
    const DisallowedFeatures& disallowed_features) {
  // If we've already initialized the group just add the context.
  if (HaveContexts()) {
    decoders_.push_back(base::AsWeakPtr<GLES2Decoder>(decoder));
    return true;
  }

  if (!feature_info_->Initialize(disallowed_features)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because FeatureInfo "
               << "initialization failed.";
    return false;
  }

  const GLint kMinRenderbufferSize = 512;  // GL says 1 pixel!
  GLint max_renderbuffer_size = 0;
  if (!QueryGLFeature(
      GL_MAX_RENDERBUFFER_SIZE, kMinRenderbufferSize,
      &max_renderbuffer_size)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because maximum "
               << "renderbuffer size too small.";
    return false;
  }
  GLint max_samples = 0;
  if (feature_info_->feature_flags().chromium_framebuffer_multisample ||
      feature_info_->feature_flags().multisampled_render_to_texture) {
    if (feature_info_->feature_flags(
            ).use_img_for_multisampled_render_to_texture) {
      glGetIntegerv(GL_MAX_SAMPLES_IMG, &max_samples);
    } else {
      glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    }
  }

  if (feature_info_->feature_flags().ext_draw_buffers) {
    GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &max_color_attachments_);
    if (max_color_attachments_ < 1)
      max_color_attachments_ = 1;
    GetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &max_draw_buffers_);
    if (max_draw_buffers_ < 1)
      max_draw_buffers_ = 1;
    draw_buffer_ = GL_BACK;
  }

  const bool depth24_supported = feature_info_->feature_flags().oes_depth24;

  buffer_manager_.reset(
      new BufferManager(memory_tracker_.get(), feature_info_.get()));
  framebuffer_manager_.reset(
      new FramebufferManager(max_draw_buffers_, max_color_attachments_));
  renderbuffer_manager_.reset(new RenderbufferManager(
      memory_tracker_.get(), max_renderbuffer_size, max_samples,
      depth24_supported));
  shader_manager_.reset(new ShaderManager());

  // Lookup GL things we need to know.
  const GLint kGLES2RequiredMinimumVertexAttribs = 8u;
  if (!QueryGLFeatureU(
      GL_MAX_VERTEX_ATTRIBS, kGLES2RequiredMinimumVertexAttribs,
      &max_vertex_attribs_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "vertex attributes supported.";
    return false;
  }

  const GLuint kGLES2RequiredMinimumTextureUnits = 8u;
  if (!QueryGLFeatureU(
      GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, kGLES2RequiredMinimumTextureUnits,
      &max_texture_units_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "texture units supported.";
    return false;
  }

  GLint max_texture_size = 0;
  GLint max_cube_map_texture_size = 0;
  const GLint kMinTextureSize = 2048;  // GL actually says 64!?!?
  const GLint kMinCubeMapSize = 256;  // GL actually says 16!?!?
  if (!QueryGLFeature(
      GL_MAX_TEXTURE_SIZE, kMinTextureSize, &max_texture_size) ||
      !QueryGLFeature(
      GL_MAX_CUBE_MAP_TEXTURE_SIZE, kMinCubeMapSize,
      &max_cube_map_texture_size)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because maximum texture size"
               << "is too small.";
    return false;
  }

  if (feature_info_->workarounds().max_texture_size) {
    max_texture_size = std::min(
        max_texture_size, feature_info_->workarounds().max_texture_size);
  }
  if (feature_info_->workarounds().max_cube_map_texture_size) {
    max_cube_map_texture_size = std::min(
        max_cube_map_texture_size,
        feature_info_->workarounds().max_cube_map_texture_size);
  }

  texture_manager_.reset(new TextureManager(memory_tracker_.get(),
                                            feature_info_.get(),
                                            max_texture_size,
                                            max_cube_map_texture_size));
  texture_manager_->set_framebuffer_manager(framebuffer_manager_.get());
  texture_manager_->set_stream_texture_manager(stream_texture_manager_);

  const GLint kMinTextureImageUnits = 8;
  const GLint kMinVertexTextureImageUnits = 0;
  if (!QueryGLFeatureU(
      GL_MAX_TEXTURE_IMAGE_UNITS, kMinTextureImageUnits,
      &max_texture_image_units_) ||
      !QueryGLFeatureU(
      GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, kMinVertexTextureImageUnits,
      &max_vertex_texture_image_units_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "texture units.";
    return false;
  }

  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,
        &max_fragment_uniform_vectors_);
    GetIntegerv(GL_MAX_VARYING_VECTORS, &max_varying_vectors_);
    GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vertex_uniform_vectors_);
  } else {
    GetIntegerv(
        GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_fragment_uniform_vectors_);
    max_fragment_uniform_vectors_ /= 4;
    GetIntegerv(GL_MAX_VARYING_FLOATS, &max_varying_vectors_);
    max_varying_vectors_ /= 4;
    GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_vertex_uniform_vectors_);
    max_vertex_uniform_vectors_ /= 4;
  }

  const GLint kMinFragmentUniformVectors = 16;
  const GLint kMinVaryingVectors = 8;
  const GLint kMinVertexUniformVectors = 128;
  if (!CheckGLFeatureU(
      kMinFragmentUniformVectors, &max_fragment_uniform_vectors_) ||
      !CheckGLFeatureU(kMinVaryingVectors, &max_varying_vectors_) ||
      !CheckGLFeatureU(
      kMinVertexUniformVectors, &max_vertex_uniform_vectors_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "uniforms or varyings supported.";
    return false;
  }

  // TODO(gman): Use workarounds similar to max_texture_size above to implement.
  if (gfx::GetGLImplementation() == gfx::kGLImplementationOSMesaGL) {
    // Some shaders in Skia needed more than the min.
    max_fragment_uniform_vectors_ =
       std::min(static_cast<uint32>(kMinFragmentUniformVectors * 2),
                max_fragment_uniform_vectors_);
    max_varying_vectors_ =
       std::min(static_cast<uint32>(kMinVaryingVectors * 2),
                max_varying_vectors_);
    max_vertex_uniform_vectors_ =
       std::min(static_cast<uint32>(kMinVertexUniformVectors * 2),
                max_vertex_uniform_vectors_);
  }

  program_manager_.reset(new ProgramManager(
      program_cache_, max_varying_vectors_));

  if (!texture_manager_->Initialize()) {
    LOG(ERROR) << "Context::Group::Initialize failed because texture manager "
               << "failed to initialize.";
    return false;
  }

  decoders_.push_back(base::AsWeakPtr<GLES2Decoder>(decoder));
  return true;
}

namespace {

bool IsNull(const base::WeakPtr<gles2::GLES2Decoder>& decoder) {
  return !decoder.get();
}

template <typename T>
class WeakPtrEquals {
 public:
  explicit WeakPtrEquals(T* t) : t_(t) {}

  bool operator()(const base::WeakPtr<T>& t) {
    return t.get() == t_;
  }

 private:
  T* const t_;
};

}  // namespace anonymous

bool ContextGroup::HaveContexts() {
  decoders_.erase(std::remove_if(decoders_.begin(), decoders_.end(), IsNull),
                  decoders_.end());
  return !decoders_.empty();
}

void ContextGroup::Destroy(GLES2Decoder* decoder, bool have_context) {
  decoders_.erase(std::remove_if(decoders_.begin(), decoders_.end(),
                                 WeakPtrEquals<gles2::GLES2Decoder>(decoder)),
                  decoders_.end());
  // If we still have contexts do nothing.
  if (HaveContexts()) {
    return;
  }

  if (buffer_manager_ != NULL) {
    buffer_manager_->Destroy(have_context);
    buffer_manager_.reset();
  }

  if (framebuffer_manager_ != NULL) {
    framebuffer_manager_->Destroy(have_context);
    if (texture_manager_)
      texture_manager_->set_framebuffer_manager(NULL);
    framebuffer_manager_.reset();
  }

  if (renderbuffer_manager_ != NULL) {
    renderbuffer_manager_->Destroy(have_context);
    renderbuffer_manager_.reset();
  }

  if (texture_manager_ != NULL) {
    texture_manager_->Destroy(have_context);
    texture_manager_.reset();
  }

  if (program_manager_ != NULL) {
    program_manager_->Destroy(have_context);
    program_manager_.reset();
  }

  if (shader_manager_ != NULL) {
    shader_manager_->Destroy(have_context);
    shader_manager_.reset();
  }

  memory_tracker_ = NULL;
  stream_texture_manager_ = NULL;
}

IdAllocatorInterface* ContextGroup::GetIdAllocator(unsigned namespace_id) {
  if (namespace_id >= arraysize(id_namespaces_))
    return NULL;

  return id_namespaces_[namespace_id].get();
}

uint32 ContextGroup::GetMemRepresented() const {
  uint32 total = 0;
  if (buffer_manager_.get())
    total += buffer_manager_->mem_represented();
  if (renderbuffer_manager_.get())
    total += renderbuffer_manager_->mem_represented();
  if (texture_manager_.get())
    total += texture_manager_->mem_represented();
  return total;
}

void ContextGroup::LoseContexts(GLenum reset_status) {
  for (size_t ii = 0; ii < decoders_.size(); ++ii) {
    if (decoders_[ii].get()) {
      decoders_[ii]->LoseContext(reset_status);
    }
  }
}

ContextGroup::~ContextGroup() {
  CHECK(!HaveContexts());
}

bool ContextGroup::CheckGLFeature(GLint min_required, GLint* v) {
  GLint value = *v;
  if (enforce_gl_minimums_) {
    value = std::min(min_required, value);
  }
  *v = value;
  return value >= min_required;
}

bool ContextGroup::CheckGLFeatureU(GLint min_required, uint32* v) {
  GLint value = *v;
  if (enforce_gl_minimums_) {
    value = std::min(min_required, value);
  }
  *v = value;
  return value >= min_required;
}

bool ContextGroup::QueryGLFeature(
    GLenum pname, GLint min_required, GLint* v) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *v = value;
  return CheckGLFeature(min_required, v);
}

bool ContextGroup::QueryGLFeatureU(
    GLenum pname, GLint min_required, uint32* v) {
  uint32 value = 0;
  GetIntegerv(pname, &value);
  bool result = CheckGLFeatureU(min_required, &value);
  *v = value;
  return result;
}

}  // namespace gles2
}  // namespace gpu
