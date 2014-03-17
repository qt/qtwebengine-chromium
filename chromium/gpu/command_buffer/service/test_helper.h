// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_

#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"

namespace gpu {
namespace gles2 {

struct DisallowedFeatures;
class Buffer;
class BufferManager;
class MockErrorState;
class TextureRef;
class TextureManager;

class TestHelper {
 public:
  static const GLuint kServiceBlackTexture2dId = 701;
  static const GLuint kServiceDefaultTexture2dId = 702;
  static const GLuint kServiceBlackTextureCubemapId = 703;
  static const GLuint kServiceDefaultTextureCubemapId = 704;
  static const GLuint kServiceBlackExternalTextureId = 705;
  static const GLuint kServiceDefaultExternalTextureId = 706;
  static const GLuint kServiceBlackRectangleTextureId = 707;
  static const GLuint kServiceDefaultRectangleTextureId = 708;

  static const GLint kMaxSamples = 4;
  static const GLint kMaxRenderbufferSize = 1024;
  static const GLint kMaxTextureSize = 2048;
  static const GLint kMaxCubeMapTextureSize = 256;
  static const GLint kNumVertexAttribs = 16;
  static const GLint kNumTextureUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxVertexTextureImageUnits = 2;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxFragmentUniformComponents =
      kMaxFragmentUniformVectors * 4;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVaryingFloats = kMaxVaryingVectors * 4;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kMaxVertexUniformComponents = kMaxVertexUniformVectors * 4;

  struct AttribInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  struct UniformInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint fake_location;
    GLint real_location;
    GLint desired_location;
    const char* good_name;
  };

  static void SetupContextGroupInitExpectations(
      ::gfx::MockGLInterface* gl,
      const DisallowedFeatures& disallowed_features,
      const char* extensions);
  static void SetupFeatureInfoInitExpectations(
      ::gfx::MockGLInterface* gl, const char* extensions);
  static void SetupFeatureInfoInitExpectationsWithGLVersion(
      ::gfx::MockGLInterface* gl,
      const char* extensions,
      const char* gl_renderer,
      const char* gl_version);
  static void SetupTextureManagerInitExpectations(
      ::gfx::MockGLInterface* gl, const char* extensions);
  static void SetupTextureManagerDestructionExpectations(
      ::gfx::MockGLInterface* gl, const char* extensions);

  static void SetupExpectationsForClearingUniforms(
      ::gfx::MockGLInterface* gl, UniformInfo* uniforms, size_t num_uniforms);

  static void SetupShader(
      ::gfx::MockGLInterface* gl,
      AttribInfo* attribs, size_t num_attribs,
      UniformInfo* uniforms, size_t num_uniforms,
      GLuint service_id);

  static void SetupProgramSuccessExpectations(::gfx::MockGLInterface* gl,
      AttribInfo* attribs, size_t num_attribs,
      UniformInfo* uniforms, size_t num_uniforms,
      GLuint service_id);

  static void DoBufferData(
      ::gfx::MockGLInterface* gl, MockErrorState* error_state,
      BufferManager* manager, Buffer* buffer, GLsizeiptr size, GLenum usage,
      const GLvoid* data, GLenum error);

  static void SetTexParameterWithExpectations(
      ::gfx::MockGLInterface* gl, MockErrorState* error_state,
      TextureManager* manager, TextureRef* texture_ref,
      GLenum pname, GLint value, GLenum error);

 private:
  static void SetupTextureInitializationExpectations(
       ::gfx::MockGLInterface* gl, GLenum target);
  static void SetupTextureDestructionExpectations(
       ::gfx::MockGLInterface* gl, GLenum target);
};

// This object temporaritly Sets what gfx::GetGLImplementation returns. During
// testing the GLImplementation is set to kGLImplemenationMockGL but lots of
// code branches based on what gfx::GetGLImplementation returns.
class ScopedGLImplementationSetter {
 public:
  explicit ScopedGLImplementationSetter(gfx::GLImplementation implementation);
  ~ScopedGLImplementationSetter();

 private:
  gfx::GLImplementation old_implementation_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_

