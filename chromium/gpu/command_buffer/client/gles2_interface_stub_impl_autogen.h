// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// This file is included by gles2_interface_stub.cc.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_STUB_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_STUB_IMPL_AUTOGEN_H_

void GLES2InterfaceStub::ActiveTexture(GLenum /* texture */) {
}
void GLES2InterfaceStub::AttachShader(
    GLuint /* program */, GLuint /* shader */) {
}
void GLES2InterfaceStub::BindAttribLocation(
    GLuint /* program */, GLuint /* index */, const char* /* name */) {
}
void GLES2InterfaceStub::BindBuffer(GLenum /* target */, GLuint /* buffer */) {
}
void GLES2InterfaceStub::BindFramebuffer(
    GLenum /* target */, GLuint /* framebuffer */) {
}
void GLES2InterfaceStub::BindRenderbuffer(
    GLenum /* target */, GLuint /* renderbuffer */) {
}
void GLES2InterfaceStub::BindTexture(
    GLenum /* target */, GLuint /* texture */) {
}
void GLES2InterfaceStub::BlendColor(
    GLclampf /* red */, GLclampf /* green */, GLclampf /* blue */,
    GLclampf /* alpha */) {
}
void GLES2InterfaceStub::BlendEquation(GLenum /* mode */) {
}
void GLES2InterfaceStub::BlendEquationSeparate(
    GLenum /* modeRGB */, GLenum /* modeAlpha */) {
}
void GLES2InterfaceStub::BlendFunc(
    GLenum /* sfactor */, GLenum /* dfactor */) {
}
void GLES2InterfaceStub::BlendFuncSeparate(
    GLenum /* srcRGB */, GLenum /* dstRGB */, GLenum /* srcAlpha */,
    GLenum /* dstAlpha */) {
}
void GLES2InterfaceStub::BufferData(
    GLenum /* target */, GLsizeiptr /* size */, const void* /* data */,
    GLenum /* usage */) {
}
void GLES2InterfaceStub::BufferSubData(
    GLenum /* target */, GLintptr /* offset */, GLsizeiptr /* size */,
    const void* /* data */) {
}
GLenum GLES2InterfaceStub::CheckFramebufferStatus(GLenum /* target */) {
  return 0;
}
void GLES2InterfaceStub::Clear(GLbitfield /* mask */) {
}
void GLES2InterfaceStub::ClearColor(
    GLclampf /* red */, GLclampf /* green */, GLclampf /* blue */,
    GLclampf /* alpha */) {
}
void GLES2InterfaceStub::ClearDepthf(GLclampf /* depth */) {
}
void GLES2InterfaceStub::ClearStencil(GLint /* s */) {
}
void GLES2InterfaceStub::ColorMask(
    GLboolean /* red */, GLboolean /* green */, GLboolean /* blue */,
    GLboolean /* alpha */) {
}
void GLES2InterfaceStub::CompileShader(GLuint /* shader */) {
}
void GLES2InterfaceStub::CompressedTexImage2D(
    GLenum /* target */, GLint /* level */, GLenum /* internalformat */,
    GLsizei /* width */, GLsizei /* height */, GLint /* border */,
    GLsizei /* imageSize */, const void* /* data */) {
}
void GLES2InterfaceStub::CompressedTexSubImage2D(
    GLenum /* target */, GLint /* level */, GLint /* xoffset */,
    GLint /* yoffset */, GLsizei /* width */, GLsizei /* height */,
    GLenum /* format */, GLsizei /* imageSize */, const void* /* data */) {
}
void GLES2InterfaceStub::CopyTexImage2D(
    GLenum /* target */, GLint /* level */, GLenum /* internalformat */,
    GLint /* x */, GLint /* y */, GLsizei /* width */, GLsizei /* height */,
    GLint /* border */) {
}
void GLES2InterfaceStub::CopyTexSubImage2D(
    GLenum /* target */, GLint /* level */, GLint /* xoffset */,
    GLint /* yoffset */, GLint /* x */, GLint /* y */, GLsizei /* width */,
    GLsizei /* height */) {
}
GLuint GLES2InterfaceStub::CreateProgram() {
  return 0;
}
GLuint GLES2InterfaceStub::CreateShader(GLenum /* type */) {
  return 0;
}
void GLES2InterfaceStub::CullFace(GLenum /* mode */) {
}
void GLES2InterfaceStub::DeleteBuffers(
    GLsizei /* n */, const GLuint* /* buffers */) {
}
void GLES2InterfaceStub::DeleteFramebuffers(
    GLsizei /* n */, const GLuint* /* framebuffers */) {
}
void GLES2InterfaceStub::DeleteProgram(GLuint /* program */) {
}
void GLES2InterfaceStub::DeleteRenderbuffers(
    GLsizei /* n */, const GLuint* /* renderbuffers */) {
}
void GLES2InterfaceStub::DeleteShader(GLuint /* shader */) {
}
void GLES2InterfaceStub::DeleteTextures(
    GLsizei /* n */, const GLuint* /* textures */) {
}
void GLES2InterfaceStub::DepthFunc(GLenum /* func */) {
}
void GLES2InterfaceStub::DepthMask(GLboolean /* flag */) {
}
void GLES2InterfaceStub::DepthRangef(
    GLclampf /* zNear */, GLclampf /* zFar */) {
}
void GLES2InterfaceStub::DetachShader(
    GLuint /* program */, GLuint /* shader */) {
}
void GLES2InterfaceStub::Disable(GLenum /* cap */) {
}
void GLES2InterfaceStub::DisableVertexAttribArray(GLuint /* index */) {
}
void GLES2InterfaceStub::DrawArrays(
    GLenum /* mode */, GLint /* first */, GLsizei /* count */) {
}
void GLES2InterfaceStub::DrawElements(
    GLenum /* mode */, GLsizei /* count */, GLenum /* type */,
    const void* /* indices */) {
}
void GLES2InterfaceStub::Enable(GLenum /* cap */) {
}
void GLES2InterfaceStub::EnableVertexAttribArray(GLuint /* index */) {
}
void GLES2InterfaceStub::Finish() {
}
void GLES2InterfaceStub::Flush() {
}
void GLES2InterfaceStub::FramebufferRenderbuffer(
    GLenum /* target */, GLenum /* attachment */,
    GLenum /* renderbuffertarget */, GLuint /* renderbuffer */) {
}
void GLES2InterfaceStub::FramebufferTexture2D(
    GLenum /* target */, GLenum /* attachment */, GLenum /* textarget */,
    GLuint /* texture */, GLint /* level */) {
}
void GLES2InterfaceStub::FrontFace(GLenum /* mode */) {
}
void GLES2InterfaceStub::GenBuffers(GLsizei /* n */, GLuint* /* buffers */) {
}
void GLES2InterfaceStub::GenerateMipmap(GLenum /* target */) {
}
void GLES2InterfaceStub::GenFramebuffers(
    GLsizei /* n */, GLuint* /* framebuffers */) {
}
void GLES2InterfaceStub::GenRenderbuffers(
    GLsizei /* n */, GLuint* /* renderbuffers */) {
}
void GLES2InterfaceStub::GenTextures(GLsizei /* n */, GLuint* /* textures */) {
}
void GLES2InterfaceStub::GetActiveAttrib(
    GLuint /* program */, GLuint /* index */, GLsizei /* bufsize */,
    GLsizei* /* length */, GLint* /* size */, GLenum* /* type */,
    char* /* name */) {
}
void GLES2InterfaceStub::GetActiveUniform(
    GLuint /* program */, GLuint /* index */, GLsizei /* bufsize */,
    GLsizei* /* length */, GLint* /* size */, GLenum* /* type */,
    char* /* name */) {
}
void GLES2InterfaceStub::GetAttachedShaders(
    GLuint /* program */, GLsizei /* maxcount */, GLsizei* /* count */,
    GLuint* /* shaders */) {
}
GLint GLES2InterfaceStub::GetAttribLocation(
    GLuint /* program */, const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetBooleanv(
    GLenum /* pname */, GLboolean* /* params */) {
}
void GLES2InterfaceStub::GetBufferParameteriv(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
}
GLenum GLES2InterfaceStub::GetError() {
  return 0;
}
void GLES2InterfaceStub::GetFloatv(GLenum /* pname */, GLfloat* /* params */) {
}
void GLES2InterfaceStub::GetFramebufferAttachmentParameteriv(
    GLenum /* target */, GLenum /* attachment */, GLenum /* pname */,
    GLint* /* params */) {
}
void GLES2InterfaceStub::GetIntegerv(GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetProgramiv(
    GLuint /* program */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetProgramInfoLog(
    GLuint /* program */, GLsizei /* bufsize */, GLsizei* /* length */,
    char* /* infolog */) {
}
void GLES2InterfaceStub::GetRenderbufferParameteriv(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetShaderiv(
    GLuint /* shader */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetShaderInfoLog(
    GLuint /* shader */, GLsizei /* bufsize */, GLsizei* /* length */,
    char* /* infolog */) {
}
void GLES2InterfaceStub::GetShaderPrecisionFormat(
    GLenum /* shadertype */, GLenum /* precisiontype */, GLint* /* range */,
    GLint* /* precision */) {
}
void GLES2InterfaceStub::GetShaderSource(
    GLuint /* shader */, GLsizei /* bufsize */, GLsizei* /* length */,
    char* /* source */) {
}
const GLubyte* GLES2InterfaceStub::GetString(GLenum /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetTexParameterfv(
    GLenum /* target */, GLenum /* pname */, GLfloat* /* params */) {
}
void GLES2InterfaceStub::GetTexParameteriv(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetUniformfv(
    GLuint /* program */, GLint /* location */, GLfloat* /* params */) {
}
void GLES2InterfaceStub::GetUniformiv(
    GLuint /* program */, GLint /* location */, GLint* /* params */) {
}
GLint GLES2InterfaceStub::GetUniformLocation(
    GLuint /* program */, const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetVertexAttribfv(
    GLuint /* index */, GLenum /* pname */, GLfloat* /* params */) {
}
void GLES2InterfaceStub::GetVertexAttribiv(
    GLuint /* index */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetVertexAttribPointerv(
    GLuint /* index */, GLenum /* pname */, void** /* pointer */) {
}
void GLES2InterfaceStub::Hint(GLenum /* target */, GLenum /* mode */) {
}
GLboolean GLES2InterfaceStub::IsBuffer(GLuint /* buffer */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsEnabled(GLenum /* cap */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsFramebuffer(GLuint /* framebuffer */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsProgram(GLuint /* program */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsRenderbuffer(GLuint /* renderbuffer */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsShader(GLuint /* shader */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsTexture(GLuint /* texture */) {
  return 0;
}
void GLES2InterfaceStub::LineWidth(GLfloat /* width */) {
}
void GLES2InterfaceStub::LinkProgram(GLuint /* program */) {
}
void GLES2InterfaceStub::PixelStorei(GLenum /* pname */, GLint /* param */) {
}
void GLES2InterfaceStub::PolygonOffset(
    GLfloat /* factor */, GLfloat /* units */) {
}
void GLES2InterfaceStub::ReadPixels(
    GLint /* x */, GLint /* y */, GLsizei /* width */, GLsizei /* height */,
    GLenum /* format */, GLenum /* type */, void* /* pixels */) {
}
void GLES2InterfaceStub::ReleaseShaderCompiler() {
}
void GLES2InterfaceStub::RenderbufferStorage(
    GLenum /* target */, GLenum /* internalformat */, GLsizei /* width */,
    GLsizei /* height */) {
}
void GLES2InterfaceStub::SampleCoverage(
    GLclampf /* value */, GLboolean /* invert */) {
}
void GLES2InterfaceStub::Scissor(
    GLint /* x */, GLint /* y */, GLsizei /* width */, GLsizei /* height */) {
}
void GLES2InterfaceStub::ShaderBinary(
    GLsizei /* n */, const GLuint* /* shaders */, GLenum /* binaryformat */,
    const void* /* binary */, GLsizei /* length */) {
}
void GLES2InterfaceStub::ShaderSource(
    GLuint /* shader */, GLsizei /* count */, const GLchar* const* /* str */,
    const GLint* /* length */) {
}
void GLES2InterfaceStub::ShallowFinishCHROMIUM() {
}
void GLES2InterfaceStub::ShallowFlushCHROMIUM() {
}
void GLES2InterfaceStub::StencilFunc(
    GLenum /* func */, GLint /* ref */, GLuint /* mask */) {
}
void GLES2InterfaceStub::StencilFuncSeparate(
    GLenum /* face */, GLenum /* func */, GLint /* ref */, GLuint /* mask */) {
}
void GLES2InterfaceStub::StencilMask(GLuint /* mask */) {
}
void GLES2InterfaceStub::StencilMaskSeparate(
    GLenum /* face */, GLuint /* mask */) {
}
void GLES2InterfaceStub::StencilOp(
    GLenum /* fail */, GLenum /* zfail */, GLenum /* zpass */) {
}
void GLES2InterfaceStub::StencilOpSeparate(
    GLenum /* face */, GLenum /* fail */, GLenum /* zfail */,
    GLenum /* zpass */) {
}
void GLES2InterfaceStub::TexImage2D(
    GLenum /* target */, GLint /* level */, GLint /* internalformat */,
    GLsizei /* width */, GLsizei /* height */, GLint /* border */,
    GLenum /* format */, GLenum /* type */, const void* /* pixels */) {
}
void GLES2InterfaceStub::TexParameterf(
    GLenum /* target */, GLenum /* pname */, GLfloat /* param */) {
}
void GLES2InterfaceStub::TexParameterfv(
    GLenum /* target */, GLenum /* pname */, const GLfloat* /* params */) {
}
void GLES2InterfaceStub::TexParameteri(
    GLenum /* target */, GLenum /* pname */, GLint /* param */) {
}
void GLES2InterfaceStub::TexParameteriv(
    GLenum /* target */, GLenum /* pname */, const GLint* /* params */) {
}
void GLES2InterfaceStub::TexSubImage2D(
    GLenum /* target */, GLint /* level */, GLint /* xoffset */,
    GLint /* yoffset */, GLsizei /* width */, GLsizei /* height */,
    GLenum /* format */, GLenum /* type */, const void* /* pixels */) {
}
void GLES2InterfaceStub::Uniform1f(GLint /* location */, GLfloat /* x */) {
}
void GLES2InterfaceStub::Uniform1fv(
    GLint /* location */, GLsizei /* count */, const GLfloat* /* v */) {
}
void GLES2InterfaceStub::Uniform1i(GLint /* location */, GLint /* x */) {
}
void GLES2InterfaceStub::Uniform1iv(
    GLint /* location */, GLsizei /* count */, const GLint* /* v */) {
}
void GLES2InterfaceStub::Uniform2f(
    GLint /* location */, GLfloat /* x */, GLfloat /* y */) {
}
void GLES2InterfaceStub::Uniform2fv(
    GLint /* location */, GLsizei /* count */, const GLfloat* /* v */) {
}
void GLES2InterfaceStub::Uniform2i(
    GLint /* location */, GLint /* x */, GLint /* y */) {
}
void GLES2InterfaceStub::Uniform2iv(
    GLint /* location */, GLsizei /* count */, const GLint* /* v */) {
}
void GLES2InterfaceStub::Uniform3f(
    GLint /* location */, GLfloat /* x */, GLfloat /* y */, GLfloat /* z */) {
}
void GLES2InterfaceStub::Uniform3fv(
    GLint /* location */, GLsizei /* count */, const GLfloat* /* v */) {
}
void GLES2InterfaceStub::Uniform3i(
    GLint /* location */, GLint /* x */, GLint /* y */, GLint /* z */) {
}
void GLES2InterfaceStub::Uniform3iv(
    GLint /* location */, GLsizei /* count */, const GLint* /* v */) {
}
void GLES2InterfaceStub::Uniform4f(
    GLint /* location */, GLfloat /* x */, GLfloat /* y */, GLfloat /* z */,
    GLfloat /* w */) {
}
void GLES2InterfaceStub::Uniform4fv(
    GLint /* location */, GLsizei /* count */, const GLfloat* /* v */) {
}
void GLES2InterfaceStub::Uniform4i(
    GLint /* location */, GLint /* x */, GLint /* y */, GLint /* z */,
    GLint /* w */) {
}
void GLES2InterfaceStub::Uniform4iv(
    GLint /* location */, GLsizei /* count */, const GLint* /* v */) {
}
void GLES2InterfaceStub::UniformMatrix2fv(
    GLint /* location */, GLsizei /* count */, GLboolean /* transpose */,
    const GLfloat* /* value */) {
}
void GLES2InterfaceStub::UniformMatrix3fv(
    GLint /* location */, GLsizei /* count */, GLboolean /* transpose */,
    const GLfloat* /* value */) {
}
void GLES2InterfaceStub::UniformMatrix4fv(
    GLint /* location */, GLsizei /* count */, GLboolean /* transpose */,
    const GLfloat* /* value */) {
}
void GLES2InterfaceStub::UseProgram(GLuint /* program */) {
}
void GLES2InterfaceStub::ValidateProgram(GLuint /* program */) {
}
void GLES2InterfaceStub::VertexAttrib1f(GLuint /* indx */, GLfloat /* x */) {
}
void GLES2InterfaceStub::VertexAttrib1fv(
    GLuint /* indx */, const GLfloat* /* values */) {
}
void GLES2InterfaceStub::VertexAttrib2f(
    GLuint /* indx */, GLfloat /* x */, GLfloat /* y */) {
}
void GLES2InterfaceStub::VertexAttrib2fv(
    GLuint /* indx */, const GLfloat* /* values */) {
}
void GLES2InterfaceStub::VertexAttrib3f(
    GLuint /* indx */, GLfloat /* x */, GLfloat /* y */, GLfloat /* z */) {
}
void GLES2InterfaceStub::VertexAttrib3fv(
    GLuint /* indx */, const GLfloat* /* values */) {
}
void GLES2InterfaceStub::VertexAttrib4f(
    GLuint /* indx */, GLfloat /* x */, GLfloat /* y */, GLfloat /* z */,
    GLfloat /* w */) {
}
void GLES2InterfaceStub::VertexAttrib4fv(
    GLuint /* indx */, const GLfloat* /* values */) {
}
void GLES2InterfaceStub::VertexAttribPointer(
    GLuint /* indx */, GLint /* size */, GLenum /* type */,
    GLboolean /* normalized */, GLsizei /* stride */, const void* /* ptr */) {
}
void GLES2InterfaceStub::Viewport(
    GLint /* x */, GLint /* y */, GLsizei /* width */, GLsizei /* height */) {
}
void GLES2InterfaceStub::BlitFramebufferCHROMIUM(
    GLint /* srcX0 */, GLint /* srcY0 */, GLint /* srcX1 */, GLint /* srcY1 */,
    GLint /* dstX0 */, GLint /* dstY0 */, GLint /* dstX1 */, GLint /* dstY1 */,
    GLbitfield /* mask */, GLenum /* filter */) {
}
void GLES2InterfaceStub::RenderbufferStorageMultisampleCHROMIUM(
    GLenum /* target */, GLsizei /* samples */, GLenum /* internalformat */,
    GLsizei /* width */, GLsizei /* height */) {
}
void GLES2InterfaceStub::RenderbufferStorageMultisampleEXT(
    GLenum /* target */, GLsizei /* samples */, GLenum /* internalformat */,
    GLsizei /* width */, GLsizei /* height */) {
}
void GLES2InterfaceStub::FramebufferTexture2DMultisampleEXT(
    GLenum /* target */, GLenum /* attachment */, GLenum /* textarget */,
    GLuint /* texture */, GLint /* level */, GLsizei /* samples */) {
}
void GLES2InterfaceStub::TexStorage2DEXT(
    GLenum /* target */, GLsizei /* levels */, GLenum /* internalFormat */,
    GLsizei /* width */, GLsizei /* height */) {
}
void GLES2InterfaceStub::GenQueriesEXT(
    GLsizei /* n */, GLuint* /* queries */) {
}
void GLES2InterfaceStub::DeleteQueriesEXT(
    GLsizei /* n */, const GLuint* /* queries */) {
}
GLboolean GLES2InterfaceStub::IsQueryEXT(GLuint /* id */) {
  return 0;
}
void GLES2InterfaceStub::BeginQueryEXT(GLenum /* target */, GLuint /* id */) {
}
void GLES2InterfaceStub::EndQueryEXT(GLenum /* target */) {
}
void GLES2InterfaceStub::GetQueryivEXT(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetQueryObjectuivEXT(
    GLuint /* id */, GLenum /* pname */, GLuint* /* params */) {
}
void GLES2InterfaceStub::InsertEventMarkerEXT(
    GLsizei /* length */, const GLchar* /* marker */) {
}
void GLES2InterfaceStub::PushGroupMarkerEXT(
    GLsizei /* length */, const GLchar* /* marker */) {
}
void GLES2InterfaceStub::PopGroupMarkerEXT() {
}
void GLES2InterfaceStub::GenVertexArraysOES(
    GLsizei /* n */, GLuint* /* arrays */) {
}
void GLES2InterfaceStub::DeleteVertexArraysOES(
    GLsizei /* n */, const GLuint* /* arrays */) {
}
GLboolean GLES2InterfaceStub::IsVertexArrayOES(GLuint /* array */) {
  return 0;
}
void GLES2InterfaceStub::BindVertexArrayOES(GLuint /* array */) {
}
void GLES2InterfaceStub::SwapBuffers() {
}
GLuint GLES2InterfaceStub::GetMaxValueInBufferCHROMIUM(
    GLuint /* buffer_id */, GLsizei /* count */, GLenum /* type */,
    GLuint /* offset */) {
  return 0;
}
void GLES2InterfaceStub::GenSharedIdsCHROMIUM(
    GLuint /* namespace_id */, GLuint /* id_offset */, GLsizei /* n */,
    GLuint* /* ids */) {
}
void GLES2InterfaceStub::DeleteSharedIdsCHROMIUM(
    GLuint /* namespace_id */, GLsizei /* n */, const GLuint* /* ids */) {
}
void GLES2InterfaceStub::RegisterSharedIdsCHROMIUM(
    GLuint /* namespace_id */, GLsizei /* n */, const GLuint* /* ids */) {
}
GLboolean GLES2InterfaceStub::EnableFeatureCHROMIUM(
    const char* /* feature */) {
  return 0;
}
void* GLES2InterfaceStub::MapBufferCHROMIUM(
    GLuint /* target */, GLenum /* access */) {
  return 0;
}
GLboolean GLES2InterfaceStub::UnmapBufferCHROMIUM(GLuint /* target */) {
  return 0;
}
void* GLES2InterfaceStub::MapImageCHROMIUM(
    GLuint /* image_id */, GLenum /* access */) {
  return 0;
}
void GLES2InterfaceStub::UnmapImageCHROMIUM(GLuint /* image_id */) {
}
void* GLES2InterfaceStub::MapBufferSubDataCHROMIUM(
    GLuint /* target */, GLintptr /* offset */, GLsizeiptr /* size */,
    GLenum /* access */) {
  return 0;
}
void GLES2InterfaceStub::UnmapBufferSubDataCHROMIUM(const void* /* mem */) {
}
void* GLES2InterfaceStub::MapTexSubImage2DCHROMIUM(
    GLenum /* target */, GLint /* level */, GLint /* xoffset */,
    GLint /* yoffset */, GLsizei /* width */, GLsizei /* height */,
    GLenum /* format */, GLenum /* type */, GLenum /* access */) {
  return 0;
}
void GLES2InterfaceStub::UnmapTexSubImage2DCHROMIUM(const void* /* mem */) {
}
void GLES2InterfaceStub::ResizeCHROMIUM(
    GLuint /* width */, GLuint /* height */, GLfloat /* scale_factor */) {
}
const GLchar* GLES2InterfaceStub::GetRequestableExtensionsCHROMIUM() {
  return 0;
}
void GLES2InterfaceStub::RequestExtensionCHROMIUM(
    const char* /* extension */) {
}
void GLES2InterfaceStub::RateLimitOffscreenContextCHROMIUM() {
}
void GLES2InterfaceStub::GetMultipleIntegervCHROMIUM(
    const GLenum* /* pnames */, GLuint /* count */, GLint* /* results */,
    GLsizeiptr /* size */) {
}
void GLES2InterfaceStub::GetProgramInfoCHROMIUM(
    GLuint /* program */, GLsizei /* bufsize */, GLsizei* /* size */,
    void* /* info */) {
}
GLuint GLES2InterfaceStub::CreateStreamTextureCHROMIUM(GLuint /* texture */) {
  return 0;
}
void GLES2InterfaceStub::DestroyStreamTextureCHROMIUM(GLuint /* texture */) {
}
GLuint GLES2InterfaceStub::CreateImageCHROMIUM(
    GLsizei /* width */, GLsizei /* height */, GLenum /* internalformat */) {
  return 0;
}
void GLES2InterfaceStub::DestroyImageCHROMIUM(GLuint /* image_id */) {
}
void GLES2InterfaceStub::GetImageParameterivCHROMIUM(
    GLuint /* image_id */, GLenum /* pname */, GLint* /* params */) {
}
void GLES2InterfaceStub::GetTranslatedShaderSourceANGLE(
    GLuint /* shader */, GLsizei /* bufsize */, GLsizei* /* length */,
    char* /* source */) {
}
void GLES2InterfaceStub::PostSubBufferCHROMIUM(
    GLint /* x */, GLint /* y */, GLint /* width */, GLint /* height */) {
}
void GLES2InterfaceStub::TexImageIOSurface2DCHROMIUM(
    GLenum /* target */, GLsizei /* width */, GLsizei /* height */,
    GLuint /* ioSurfaceId */, GLuint /* plane */) {
}
void GLES2InterfaceStub::CopyTextureCHROMIUM(
    GLenum /* target */, GLenum /* source_id */, GLenum /* dest_id */,
    GLint /* level */, GLint /* internalformat */, GLenum /* dest_type */) {
}
void GLES2InterfaceStub::DrawArraysInstancedANGLE(
    GLenum /* mode */, GLint /* first */, GLsizei /* count */,
    GLsizei /* primcount */) {
}
void GLES2InterfaceStub::DrawElementsInstancedANGLE(
    GLenum /* mode */, GLsizei /* count */, GLenum /* type */,
    const void* /* indices */, GLsizei /* primcount */) {
}
void GLES2InterfaceStub::VertexAttribDivisorANGLE(
    GLuint /* index */, GLuint /* divisor */) {
}
void GLES2InterfaceStub::GenMailboxCHROMIUM(GLbyte* /* mailbox */) {
}
void GLES2InterfaceStub::ProduceTextureCHROMIUM(
    GLenum /* target */, const GLbyte* /* mailbox */) {
}
void GLES2InterfaceStub::ConsumeTextureCHROMIUM(
    GLenum /* target */, const GLbyte* /* mailbox */) {
}
void GLES2InterfaceStub::BindUniformLocationCHROMIUM(
    GLuint /* program */, GLint /* location */, const char* /* name */) {
}
void GLES2InterfaceStub::BindTexImage2DCHROMIUM(
    GLenum /* target */, GLint /* imageId */) {
}
void GLES2InterfaceStub::ReleaseTexImage2DCHROMIUM(
    GLenum /* target */, GLint /* imageId */) {
}
void GLES2InterfaceStub::TraceBeginCHROMIUM(const char* /* name */) {
}
void GLES2InterfaceStub::TraceEndCHROMIUM() {
}
void GLES2InterfaceStub::AsyncTexSubImage2DCHROMIUM(
    GLenum /* target */, GLint /* level */, GLint /* xoffset */,
    GLint /* yoffset */, GLsizei /* width */, GLsizei /* height */,
    GLenum /* format */, GLenum /* type */, const void* /* data */) {
}
void GLES2InterfaceStub::AsyncTexImage2DCHROMIUM(
    GLenum /* target */, GLint /* level */, GLint /* internalformat */,
    GLsizei /* width */, GLsizei /* height */, GLint /* border */,
    GLenum /* format */, GLenum /* type */, const void* /* pixels */) {
}
void GLES2InterfaceStub::WaitAsyncTexImage2DCHROMIUM(GLenum /* target */) {
}
void GLES2InterfaceStub::DiscardFramebufferEXT(
    GLenum /* target */, GLsizei /* count */,
    const GLenum* /* attachments */) {
}
void GLES2InterfaceStub::LoseContextCHROMIUM(
    GLenum /* current */, GLenum /* other */) {
}
GLuint GLES2InterfaceStub::InsertSyncPointCHROMIUM() {
  return 0;
}
void GLES2InterfaceStub::WaitSyncPointCHROMIUM(GLuint /* sync_point */) {
}
void GLES2InterfaceStub::DrawBuffersEXT(
    GLsizei /* count */, const GLenum* /* bufs */) {
}
void GLES2InterfaceStub::DiscardBackbufferCHROMIUM() {
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_STUB_IMPL_AUTOGEN_H_

