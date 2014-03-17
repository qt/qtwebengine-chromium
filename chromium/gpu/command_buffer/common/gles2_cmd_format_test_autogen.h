// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// This file contains unit tests for gles2 commmands
// It is included by gles2_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(GLES2FormatTest, ActiveTexture) {
  cmds::ActiveTexture& cmd = *GetBufferAs<cmds::ActiveTexture>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::ActiveTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, AttachShader) {
  cmds::AttachShader& cmd = *GetBufferAs<cmds::AttachShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::AttachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindAttribLocation) {
  cmds::BindAttribLocation& cmd = *GetBufferAs<cmds::BindAttribLocation>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::BindAttribLocation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.name_shm_offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindAttribLocationBucket) {
  cmds::BindAttribLocationBucket& cmd =
      *GetBufferAs<cmds::BindAttribLocationBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::BindAttribLocationBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindBuffer) {
  cmds::BindBuffer& cmd = *GetBufferAs<cmds::BindBuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BindBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.buffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindFramebuffer) {
  cmds::BindFramebuffer& cmd = *GetBufferAs<cmds::BindFramebuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BindFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.framebuffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindRenderbuffer) {
  cmds::BindRenderbuffer& cmd = *GetBufferAs<cmds::BindRenderbuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BindRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.renderbuffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindTexture) {
  cmds::BindTexture& cmd = *GetBufferAs<cmds::BindTexture>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BindTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendColor) {
  cmds::BlendColor& cmd = *GetBufferAs<cmds::BlendColor>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12),
      static_cast<GLclampf>(13),
      static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::BlendColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquation) {
  cmds::BlendEquation& cmd = *GetBufferAs<cmds::BlendEquation>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::BlendEquation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquationSeparate) {
  cmds::BlendEquationSeparate& cmd =
      *GetBufferAs<cmds::BlendEquationSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BlendEquationSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.modeRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.modeAlpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFunc) {
  cmds::BlendFunc& cmd = *GetBufferAs<cmds::BlendFunc>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BlendFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.sfactor);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dfactor);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFuncSeparate) {
  cmds::BlendFuncSeparate& cmd = *GetBufferAs<cmds::BlendFuncSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::BlendFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.srcRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dstRGB);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.srcAlpha);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.dstAlpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BufferData) {
  cmds::BufferData& cmd = *GetBufferAs<cmds::BufferData>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizeiptr>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<GLenum>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::BufferData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizeiptr>(12), cmd.size);
  EXPECT_EQ(static_cast<uint32>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.usage);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BufferSubData) {
  cmds::BufferSubData& cmd = *GetBufferAs<cmds::BufferSubData>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLintptr>(12),
      static_cast<GLsizeiptr>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::BufferSubData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLintptr>(12), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.size);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CheckFramebufferStatus) {
  cmds::CheckFramebufferStatus& cmd =
      *GetBufferAs<cmds::CheckFramebufferStatus>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::CheckFramebufferStatus::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Clear) {
  cmds::Clear& cmd = *GetBufferAs<cmds::Clear>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLbitfield>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::Clear::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLbitfield>(11), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearColor) {
  cmds::ClearColor& cmd = *GetBufferAs<cmds::ClearColor>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12),
      static_cast<GLclampf>(13),
      static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::ClearColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearDepthf) {
  cmds::ClearDepthf& cmd = *GetBufferAs<cmds::ClearDepthf>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::ClearDepthf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.depth);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearStencil) {
  cmds::ClearStencil& cmd = *GetBufferAs<cmds::ClearStencil>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::ClearStencil::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.s);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ColorMask) {
  cmds::ColorMask& cmd = *GetBufferAs<cmds::ColorMask>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLboolean>(11),
      static_cast<GLboolean>(12),
      static_cast<GLboolean>(13),
      static_cast<GLboolean>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::ColorMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.red);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.green);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompileShader) {
  cmds::CompileShader& cmd = *GetBufferAs<cmds::CompileShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::CompileShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage2D) {
  cmds::CompressedTexImage2D& cmd = *GetBufferAs<cmds::CompressedTexImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLsizei>(17),
      static_cast<uint32>(18),
      static_cast<uint32>(19));
  EXPECT_EQ(static_cast<uint32>(cmds::CompressedTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32>(18), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(19), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage2DBucket) {
  cmds::CompressedTexImage2DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexImage2DBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLuint>(17));
  EXPECT_EQ(static_cast<uint32>(cmds::CompressedTexImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage2D) {
  cmds::CompressedTexSubImage2D& cmd =
      *GetBufferAs<cmds::CompressedTexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLsizei>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(cmds::CompressedTexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32>(19), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage2DBucket) {
  cmds::CompressedTexSubImage2DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexSubImage2DBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLuint>(18));
  EXPECT_EQ(static_cast<uint32>(cmds::CompressedTexSubImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexImage2D) {
  cmds::CopyTexImage2D& cmd = *GetBufferAs<cmds::CopyTexImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLsizei>(16),
      static_cast<GLsizei>(17),
      static_cast<GLint>(18));
  EXPECT_EQ(static_cast<uint32>(cmds::CopyTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLint>(14), cmd.x);
  EXPECT_EQ(static_cast<GLint>(15), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  EXPECT_EQ(static_cast<GLint>(18), cmd.border);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexSubImage2D) {
  cmds::CopyTexSubImage2D& cmd = *GetBufferAs<cmds::CopyTexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLint>(16),
      static_cast<GLsizei>(17),
      static_cast<GLsizei>(18));
  EXPECT_EQ(static_cast<uint32>(cmds::CopyTexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateProgram) {
  cmds::CreateProgram& cmd = *GetBufferAs<cmds::CreateProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::CreateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateShader) {
  cmds::CreateShader& cmd = *GetBufferAs<cmds::CreateShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::CreateShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.type);
  EXPECT_EQ(static_cast<uint32>(12), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CullFace) {
  cmds::CullFace& cmd = *GetBufferAs<cmds::CullFace>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::CullFace::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteBuffers) {
  cmds::DeleteBuffers& cmd = *GetBufferAs<cmds::DeleteBuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.buffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.buffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteBuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::DeleteBuffersImmediate& cmd =
      *GetBufferAs<cmds::DeleteBuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteFramebuffers) {
  cmds::DeleteFramebuffers& cmd = *GetBufferAs<cmds::DeleteFramebuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteFramebuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.framebuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.framebuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteFramebuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::DeleteFramebuffersImmediate& cmd =
      *GetBufferAs<cmds::DeleteFramebuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteProgram) {
  cmds::DeleteProgram& cmd = *GetBufferAs<cmds::DeleteProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteRenderbuffers) {
  cmds::DeleteRenderbuffers& cmd = *GetBufferAs<cmds::DeleteRenderbuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteRenderbuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.renderbuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.renderbuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteRenderbuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::DeleteRenderbuffersImmediate& cmd =
      *GetBufferAs<cmds::DeleteRenderbuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteShader) {
  cmds::DeleteShader& cmd = *GetBufferAs<cmds::DeleteShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteTextures) {
  cmds::DeleteTextures& cmd = *GetBufferAs<cmds::DeleteTextures>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteTextures::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.textures_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.textures_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteTexturesImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::DeleteTexturesImmediate& cmd =
      *GetBufferAs<cmds::DeleteTexturesImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DepthFunc) {
  cmds::DepthFunc& cmd = *GetBufferAs<cmds::DepthFunc>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::DepthFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DepthMask) {
  cmds::DepthMask& cmd = *GetBufferAs<cmds::DepthMask>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLboolean>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::DepthMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.flag);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DepthRangef) {
  cmds::DepthRangef& cmd = *GetBufferAs<cmds::DepthRangef>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::DepthRangef::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.zNear);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.zFar);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DetachShader) {
  cmds::DetachShader& cmd = *GetBufferAs<cmds::DetachShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::DetachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Disable) {
  cmds::Disable& cmd = *GetBufferAs<cmds::Disable>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::Disable::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DisableVertexAttribArray) {
  cmds::DisableVertexAttribArray& cmd =
      *GetBufferAs<cmds::DisableVertexAttribArray>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::DisableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArrays) {
  cmds::DrawArrays& cmd = *GetBufferAs<cmds::DrawArrays>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DrawArrays::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElements) {
  cmds::DrawElements& cmd = *GetBufferAs<cmds::DrawElements>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::DrawElements::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Enable) {
  cmds::Enable& cmd = *GetBufferAs<cmds::Enable>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::Enable::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableVertexAttribArray) {
  cmds::EnableVertexAttribArray& cmd =
      *GetBufferAs<cmds::EnableVertexAttribArray>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::EnableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Finish) {
  cmds::Finish& cmd = *GetBufferAs<cmds::Finish>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::Finish::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Flush) {
  cmds::Flush& cmd = *GetBufferAs<cmds::Flush>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::Flush::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferRenderbuffer) {
  cmds::FramebufferRenderbuffer& cmd =
      *GetBufferAs<cmds::FramebufferRenderbuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::FramebufferRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.renderbuffertarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.renderbuffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTexture2D) {
  cmds::FramebufferTexture2D& cmd = *GetBufferAs<cmds::FramebufferTexture2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14),
      static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::FramebufferTexture2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.textarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(15), cmd.level);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FrontFace) {
  cmds::FrontFace& cmd = *GetBufferAs<cmds::FrontFace>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::FrontFace::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenBuffers) {
  cmds::GenBuffers& cmd = *GetBufferAs<cmds::GenBuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GenBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.buffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.buffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenBuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::GenBuffersImmediate& cmd = *GetBufferAs<cmds::GenBuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::GenBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GenerateMipmap) {
  cmds::GenerateMipmap& cmd = *GetBufferAs<cmds::GenerateMipmap>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::GenerateMipmap::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenFramebuffers) {
  cmds::GenFramebuffers& cmd = *GetBufferAs<cmds::GenFramebuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GenFramebuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.framebuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.framebuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenFramebuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::GenFramebuffersImmediate& cmd =
      *GetBufferAs<cmds::GenFramebuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::GenFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GenRenderbuffers) {
  cmds::GenRenderbuffers& cmd = *GetBufferAs<cmds::GenRenderbuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GenRenderbuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.renderbuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.renderbuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenRenderbuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::GenRenderbuffersImmediate& cmd =
      *GetBufferAs<cmds::GenRenderbuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::GenRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GenTextures) {
  cmds::GenTextures& cmd = *GetBufferAs<cmds::GenTextures>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GenTextures::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.textures_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.textures_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenTexturesImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::GenTexturesImmediate& cmd = *GetBufferAs<cmds::GenTexturesImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::GenTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GetActiveAttrib) {
  cmds::GetActiveAttrib& cmd = *GetBufferAs<cmds::GetActiveAttrib>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::GetActiveAttrib::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetActiveUniform) {
  cmds::GetActiveUniform& cmd = *GetBufferAs<cmds::GetActiveUniform>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::GetActiveUniform::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetAttachedShaders) {
  cmds::GetAttachedShaders& cmd = *GetBufferAs<cmds::GetAttachedShaders>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetAttachedShaders::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Write test for GetAttribLocation
// TODO(gman): Write test for GetAttribLocationBucket
TEST_F(GLES2FormatTest, GetBooleanv) {
  cmds::GetBooleanv& cmd = *GetBufferAs<cmds::GetBooleanv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GetBooleanv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetBufferParameteriv) {
  cmds::GetBufferParameteriv& cmd = *GetBufferAs<cmds::GetBufferParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetBufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetError) {
  cmds::GetError& cmd = *GetBufferAs<cmds::GetError>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetError::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFloatv) {
  cmds::GetFloatv& cmd = *GetBufferAs<cmds::GetFloatv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GetFloatv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFramebufferAttachmentParameteriv) {
  cmds::GetFramebufferAttachmentParameteriv& cmd =
      *GetBufferAs<cmds::GetFramebufferAttachmentParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(
      static_cast<uint32>(cmds::GetFramebufferAttachmentParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetIntegerv) {
  cmds::GetIntegerv& cmd = *GetBufferAs<cmds::GetIntegerv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GetIntegerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramiv) {
  cmds::GetProgramiv& cmd = *GetBufferAs<cmds::GetProgramiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetProgramiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInfoLog) {
  cmds::GetProgramInfoLog& cmd = *GetBufferAs<cmds::GetProgramInfoLog>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetProgramInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetRenderbufferParameteriv) {
  cmds::GetRenderbufferParameteriv& cmd =
      *GetBufferAs<cmds::GetRenderbufferParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetRenderbufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderiv) {
  cmds::GetShaderiv& cmd = *GetBufferAs<cmds::GetShaderiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetShaderiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderInfoLog) {
  cmds::GetShaderInfoLog& cmd = *GetBufferAs<cmds::GetShaderInfoLog>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetShaderInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderPrecisionFormat) {
  cmds::GetShaderPrecisionFormat& cmd =
      *GetBufferAs<cmds::GetShaderPrecisionFormat>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetShaderPrecisionFormat::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.shadertype);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.precisiontype);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderSource) {
  cmds::GetShaderSource& cmd = *GetBufferAs<cmds::GetShaderSource>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetString) {
  cmds::GetString& cmd = *GetBufferAs<cmds::GetString>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetString::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.name);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTexParameterfv) {
  cmds::GetTexParameterfv& cmd = *GetBufferAs<cmds::GetTexParameterfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetTexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTexParameteriv) {
  cmds::GetTexParameteriv& cmd = *GetBufferAs<cmds::GetTexParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetTexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformfv) {
  cmds::GetUniformfv& cmd = *GetBufferAs<cmds::GetUniformfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetUniformfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformiv) {
  cmds::GetUniformiv& cmd = *GetBufferAs<cmds::GetUniformiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetUniformiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Write test for GetUniformLocation
// TODO(gman): Write test for GetUniformLocationBucket
TEST_F(GLES2FormatTest, GetVertexAttribfv) {
  cmds::GetVertexAttribfv& cmd = *GetBufferAs<cmds::GetVertexAttribfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetVertexAttribfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribiv) {
  cmds::GetVertexAttribiv& cmd = *GetBufferAs<cmds::GetVertexAttribiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetVertexAttribiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribPointerv) {
  cmds::GetVertexAttribPointerv& cmd =
      *GetBufferAs<cmds::GetVertexAttribPointerv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::GetVertexAttribPointerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.pointer_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.pointer_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Hint) {
  cmds::Hint& cmd = *GetBufferAs<cmds::Hint>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::Hint::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsBuffer) {
  cmds::IsBuffer& cmd = *GetBufferAs<cmds::IsBuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsEnabled) {
  cmds::IsEnabled& cmd = *GetBufferAs<cmds::IsEnabled>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsEnabled::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsFramebuffer) {
  cmds::IsFramebuffer& cmd = *GetBufferAs<cmds::IsFramebuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.framebuffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsProgram) {
  cmds::IsProgram& cmd = *GetBufferAs<cmds::IsProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsRenderbuffer) {
  cmds::IsRenderbuffer& cmd = *GetBufferAs<cmds::IsRenderbuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.renderbuffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsShader) {
  cmds::IsShader& cmd = *GetBufferAs<cmds::IsShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsTexture) {
  cmds::IsTexture& cmd = *GetBufferAs<cmds::IsTexture>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LineWidth) {
  cmds::LineWidth& cmd = *GetBufferAs<cmds::LineWidth>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLfloat>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::LineWidth::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.width);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LinkProgram) {
  cmds::LinkProgram& cmd = *GetBufferAs<cmds::LinkProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::LinkProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PixelStorei) {
  cmds::PixelStorei& cmd = *GetBufferAs<cmds::PixelStorei>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::PixelStorei::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(12), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PolygonOffset) {
  cmds::PolygonOffset& cmd = *GetBufferAs<cmds::PolygonOffset>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLfloat>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::PolygonOffset::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.factor);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.units);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReadPixels) {
  cmds::ReadPixels& cmd = *GetBufferAs<cmds::ReadPixels>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14),
      static_cast<GLenum>(15),
      static_cast<GLenum>(16),
      static_cast<uint32>(17),
      static_cast<uint32>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20),
      static_cast<GLboolean>(21));
  EXPECT_EQ(static_cast<uint32>(cmds::ReadPixels::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.type);
  EXPECT_EQ(static_cast<uint32>(17), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(18), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<uint32>(19), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.result_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(21), cmd.async);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReleaseShaderCompiler) {
  cmds::ReleaseShaderCompiler& cmd =
      *GetBufferAs<cmds::ReleaseShaderCompiler>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::ReleaseShaderCompiler::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorage) {
  cmds::RenderbufferStorage& cmd = *GetBufferAs<cmds::RenderbufferStorage>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::RenderbufferStorage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SampleCoverage) {
  cmds::SampleCoverage& cmd = *GetBufferAs<cmds::SampleCoverage>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLboolean>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::SampleCoverage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.value);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.invert);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Scissor) {
  cmds::Scissor& cmd = *GetBufferAs<cmds::Scissor>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Scissor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderBinary) {
  cmds::ShaderBinary& cmd = *GetBufferAs<cmds::ShaderBinary>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<GLenum>(14),
      static_cast<uint32>(15),
      static_cast<uint32>(16),
      static_cast<GLsizei>(17));
  EXPECT_EQ(static_cast<uint32>(cmds::ShaderBinary::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.shaders_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.shaders_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.binaryformat);
  EXPECT_EQ(static_cast<uint32>(15), cmd.binary_shm_id);
  EXPECT_EQ(static_cast<uint32>(16), cmd.binary_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.length);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderSource) {
  cmds::ShaderSource& cmd = *GetBufferAs<cmds::ShaderSource>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::ShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderSourceBucket) {
  cmds::ShaderSourceBucket& cmd = *GetBufferAs<cmds::ShaderSourceBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::ShaderSourceBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.data_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilFunc) {
  cmds::StencilFunc& cmd = *GetBufferAs<cmds::StencilFunc>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::StencilFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  EXPECT_EQ(static_cast<GLint>(12), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilFuncSeparate) {
  cmds::StencilFuncSeparate& cmd = *GetBufferAs<cmds::StencilFuncSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLint>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::StencilFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.func);
  EXPECT_EQ(static_cast<GLint>(13), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilMask) {
  cmds::StencilMask& cmd = *GetBufferAs<cmds::StencilMask>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::StencilMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilMaskSeparate) {
  cmds::StencilMaskSeparate& cmd = *GetBufferAs<cmds::StencilMaskSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::StencilMaskSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilOp) {
  cmds::StencilOp& cmd = *GetBufferAs<cmds::StencilOp>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::StencilOp::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zpass);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilOpSeparate) {
  cmds::StencilOpSeparate& cmd = *GetBufferAs<cmds::StencilOpSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::StencilOpSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.zpass);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexImage2D) {
  cmds::TexImage2D& cmd = *GetBufferAs<cmds::TexImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLenum>(17),
      static_cast<GLenum>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(cmds::TexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.pixels_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterf) {
  cmds::TexParameterf& cmd = *GetBufferAs<cmds::TexParameterf>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::TexParameterf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterfv) {
  cmds::TexParameterfv& cmd = *GetBufferAs<cmds::TexParameterfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::TexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterfvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  cmds::TexParameterfvImmediate& cmd =
      *GetBufferAs<cmds::TexParameterfvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::TexParameterfvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, TexParameteri) {
  cmds::TexParameteri& cmd = *GetBufferAs<cmds::TexParameteri>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::TexParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameteriv) {
  cmds::TexParameteriv& cmd = *GetBufferAs<cmds::TexParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::TexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
  };
  cmds::TexParameterivImmediate& cmd =
      *GetBufferAs<cmds::TexParameterivImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::TexParameterivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, TexSubImage2D) {
  cmds::TexSubImage2D& cmd = *GetBufferAs<cmds::TexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLenum>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20),
      static_cast<GLboolean>(21));
  EXPECT_EQ(static_cast<uint32>(cmds::TexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(21), cmd.internal);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1f) {
  cmds::Uniform1f& cmd = *GetBufferAs<cmds::Uniform1f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1fv) {
  cmds::Uniform1fv& cmd = *GetBufferAs<cmds::Uniform1fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform1fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  cmds::Uniform1fvImmediate& cmd = *GetBufferAs<cmds::Uniform1fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 1;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform1i) {
  cmds::Uniform1i& cmd = *GetBufferAs<cmds::Uniform1i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform1i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1iv) {
  cmds::Uniform1iv& cmd = *GetBufferAs<cmds::Uniform1iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform1iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
  };
  cmds::Uniform1ivImmediate& cmd = *GetBufferAs<cmds::Uniform1ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 1;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform1ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform2f) {
  cmds::Uniform2f& cmd = *GetBufferAs<cmds::Uniform2f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2fv) {
  cmds::Uniform2fv& cmd = *GetBufferAs<cmds::Uniform2fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  cmds::Uniform2fvImmediate& cmd = *GetBufferAs<cmds::Uniform2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 2;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform2i) {
  cmds::Uniform2i& cmd = *GetBufferAs<cmds::Uniform2i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform2i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2iv) {
  cmds::Uniform2iv& cmd = *GetBufferAs<cmds::Uniform2iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform2iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  cmds::Uniform2ivImmediate& cmd = *GetBufferAs<cmds::Uniform2ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 2;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform2ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform3f) {
  cmds::Uniform3f& cmd = *GetBufferAs<cmds::Uniform3f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3fv) {
  cmds::Uniform3fv& cmd = *GetBufferAs<cmds::Uniform3fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
  };
  cmds::Uniform3fvImmediate& cmd = *GetBufferAs<cmds::Uniform3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 3;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform3i) {
  cmds::Uniform3i& cmd = *GetBufferAs<cmds::Uniform3i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform3i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3iv) {
  cmds::Uniform3iv& cmd = *GetBufferAs<cmds::Uniform3iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform3iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
    static_cast<GLint>(kSomeBaseValueToTestWith + 4),
    static_cast<GLint>(kSomeBaseValueToTestWith + 5),
  };
  cmds::Uniform3ivImmediate& cmd = *GetBufferAs<cmds::Uniform3ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 3;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform3ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform4f) {
  cmds::Uniform4f& cmd = *GetBufferAs<cmds::Uniform4f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14),
      static_cast<GLfloat>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform4f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4fv) {
  cmds::Uniform4fv& cmd = *GetBufferAs<cmds::Uniform4fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
  };
  cmds::Uniform4fvImmediate& cmd = *GetBufferAs<cmds::Uniform4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 4;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform4i) {
  cmds::Uniform4i& cmd = *GetBufferAs<cmds::Uniform4i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform4i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLint>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4iv) {
  cmds::Uniform4iv& cmd = *GetBufferAs<cmds::Uniform4iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform4iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
    static_cast<GLint>(kSomeBaseValueToTestWith + 4),
    static_cast<GLint>(kSomeBaseValueToTestWith + 5),
    static_cast<GLint>(kSomeBaseValueToTestWith + 6),
    static_cast<GLint>(kSomeBaseValueToTestWith + 7),
  };
  cmds::Uniform4ivImmediate& cmd = *GetBufferAs<cmds::Uniform4ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 4;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::Uniform4ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UniformMatrix2fv) {
  cmds::UniformMatrix2fv& cmd = *GetBufferAs<cmds::UniformMatrix2fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLboolean>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::UniformMatrix2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
  };
  cmds::UniformMatrix2fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 4;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::UniformMatrix2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UniformMatrix3fv) {
  cmds::UniformMatrix3fv& cmd = *GetBufferAs<cmds::UniformMatrix3fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLboolean>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::UniformMatrix3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
  };
  cmds::UniformMatrix3fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 9;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::UniformMatrix3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UniformMatrix4fv) {
  cmds::UniformMatrix4fv& cmd = *GetBufferAs<cmds::UniformMatrix4fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLboolean>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::UniformMatrix4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 18),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 19),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 20),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 21),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 22),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 23),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 24),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 25),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 26),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 27),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 28),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 29),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 30),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 31),
  };
  cmds::UniformMatrix4fvImmediate& cmd =
      *GetBufferAs<cmds::UniformMatrix4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 16;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::UniformMatrix4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UseProgram) {
  cmds::UseProgram& cmd = *GetBufferAs<cmds::UseProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::UseProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ValidateProgram) {
  cmds::ValidateProgram& cmd = *GetBufferAs<cmds::ValidateProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::ValidateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1f) {
  cmds::VertexAttrib1f& cmd = *GetBufferAs<cmds::VertexAttrib1f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1fv) {
  cmds::VertexAttrib1fv& cmd = *GetBufferAs<cmds::VertexAttrib1fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib1fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  cmds::VertexAttrib1fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib1fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttrib2f) {
  cmds::VertexAttrib2f& cmd = *GetBufferAs<cmds::VertexAttrib2f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib2fv) {
  cmds::VertexAttrib2fv& cmd = *GetBufferAs<cmds::VertexAttrib2fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  cmds::VertexAttrib2fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib2fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttrib3f) {
  cmds::VertexAttrib3f& cmd = *GetBufferAs<cmds::VertexAttrib3f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib3fv) {
  cmds::VertexAttrib3fv& cmd = *GetBufferAs<cmds::VertexAttrib3fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
  };
  cmds::VertexAttrib3fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib3fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttrib4f) {
  cmds::VertexAttrib4f& cmd = *GetBufferAs<cmds::VertexAttrib4f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14),
      static_cast<GLfloat>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib4f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib4fv) {
  cmds::VertexAttrib4fv& cmd = *GetBufferAs<cmds::VertexAttrib4fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  cmds::VertexAttrib4fvImmediate& cmd =
      *GetBufferAs<cmds::VertexAttrib4fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttrib4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttribPointer) {
  cmds::VertexAttribPointer& cmd = *GetBufferAs<cmds::VertexAttribPointer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLboolean>(14),
      static_cast<GLsizei>(15),
      static_cast<GLuint>(16));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttribPointer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLint>(12), cmd.size);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.normalized);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.stride);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Viewport) {
  cmds::Viewport& cmd = *GetBufferAs<cmds::Viewport>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::Viewport::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlitFramebufferCHROMIUM) {
  cmds::BlitFramebufferCHROMIUM& cmd =
      *GetBufferAs<cmds::BlitFramebufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLint>(16),
      static_cast<GLint>(17),
      static_cast<GLint>(18),
      static_cast<GLbitfield>(19),
      static_cast<GLenum>(20));
  EXPECT_EQ(static_cast<uint32>(cmds::BlitFramebufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.srcX0);
  EXPECT_EQ(static_cast<GLint>(12), cmd.srcY0);
  EXPECT_EQ(static_cast<GLint>(13), cmd.srcX1);
  EXPECT_EQ(static_cast<GLint>(14), cmd.srcY1);
  EXPECT_EQ(static_cast<GLint>(15), cmd.dstX0);
  EXPECT_EQ(static_cast<GLint>(16), cmd.dstY0);
  EXPECT_EQ(static_cast<GLint>(17), cmd.dstX1);
  EXPECT_EQ(static_cast<GLint>(18), cmd.dstY1);
  EXPECT_EQ(static_cast<GLbitfield>(19), cmd.mask);
  EXPECT_EQ(static_cast<GLenum>(20), cmd.filter);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorageMultisampleCHROMIUM) {
  cmds::RenderbufferStorageMultisampleCHROMIUM& cmd =
      *GetBufferAs<cmds::RenderbufferStorageMultisampleCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15));
  EXPECT_EQ(
      static_cast<uint32>(
          cmds::RenderbufferStorageMultisampleCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorageMultisampleEXT) {
  cmds::RenderbufferStorageMultisampleEXT& cmd =
      *GetBufferAs<cmds::RenderbufferStorageMultisampleEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15));
  EXPECT_EQ(
      static_cast<uint32>(cmds::RenderbufferStorageMultisampleEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTexture2DMultisampleEXT) {
  cmds::FramebufferTexture2DMultisampleEXT& cmd =
      *GetBufferAs<cmds::FramebufferTexture2DMultisampleEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14),
      static_cast<GLint>(15),
      static_cast<GLsizei>(16));
  EXPECT_EQ(
      static_cast<uint32>(cmds::FramebufferTexture2DMultisampleEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.textarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(15), cmd.level);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.samples);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexStorage2DEXT) {
  cmds::TexStorage2DEXT& cmd = *GetBufferAs<cmds::TexStorage2DEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::TexStorage2DEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.levels);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalFormat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenQueriesEXT) {
  cmds::GenQueriesEXT& cmd = *GetBufferAs<cmds::GenQueriesEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GenQueriesEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.queries_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.queries_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenQueriesEXTImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::GenQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::GenQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::GenQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteQueriesEXT) {
  cmds::DeleteQueriesEXT& cmd = *GetBufferAs<cmds::DeleteQueriesEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteQueriesEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.queries_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.queries_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteQueriesEXTImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::DeleteQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::DeleteQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, BeginQueryEXT) {
  cmds::BeginQueryEXT& cmd = *GetBufferAs<cmds::BeginQueryEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::BeginQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.sync_data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EndQueryEXT) {
  cmds::EndQueryEXT& cmd = *GetBufferAs<cmds::EndQueryEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::EndQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.submit_count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, InsertEventMarkerEXT) {
  cmds::InsertEventMarkerEXT& cmd = *GetBufferAs<cmds::InsertEventMarkerEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::InsertEventMarkerEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PushGroupMarkerEXT) {
  cmds::PushGroupMarkerEXT& cmd = *GetBufferAs<cmds::PushGroupMarkerEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::PushGroupMarkerEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PopGroupMarkerEXT) {
  cmds::PopGroupMarkerEXT& cmd = *GetBufferAs<cmds::PopGroupMarkerEXT>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::PopGroupMarkerEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenVertexArraysOES) {
  cmds::GenVertexArraysOES& cmd = *GetBufferAs<cmds::GenVertexArraysOES>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::GenVertexArraysOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.arrays_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.arrays_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenVertexArraysOESImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::GenVertexArraysOESImmediate& cmd =
      *GetBufferAs<cmds::GenVertexArraysOESImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::GenVertexArraysOESImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteVertexArraysOES) {
  cmds::DeleteVertexArraysOES& cmd =
      *GetBufferAs<cmds::DeleteVertexArraysOES>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteVertexArraysOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.arrays_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.arrays_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteVertexArraysOESImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  cmds::DeleteVertexArraysOESImmediate& cmd =
      *GetBufferAs<cmds::DeleteVertexArraysOESImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteVertexArraysOESImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, IsVertexArrayOES) {
  cmds::IsVertexArrayOES& cmd = *GetBufferAs<cmds::IsVertexArrayOES>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::IsVertexArrayOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.array);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindVertexArrayOES) {
  cmds::BindVertexArrayOES& cmd = *GetBufferAs<cmds::BindVertexArrayOES>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::BindVertexArrayOES::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.array);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SwapBuffers) {
  cmds::SwapBuffers& cmd = *GetBufferAs<cmds::SwapBuffers>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::SwapBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetMaxValueInBufferCHROMIUM) {
  cmds::GetMaxValueInBufferCHROMIUM& cmd =
      *GetBufferAs<cmds::GetMaxValueInBufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14),
      static_cast<uint32>(15),
      static_cast<uint32>(16));
  EXPECT_EQ(static_cast<uint32>(cmds::GetMaxValueInBufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(16), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenSharedIdsCHROMIUM) {
  cmds::GenSharedIdsCHROMIUM& cmd = *GetBufferAs<cmds::GenSharedIdsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<GLsizei>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::GenSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id_offset);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.n);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteSharedIdsCHROMIUM) {
  cmds::DeleteSharedIdsCHROMIUM& cmd =
      *GetBufferAs<cmds::DeleteSharedIdsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::DeleteSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.n);
  EXPECT_EQ(static_cast<uint32>(13), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RegisterSharedIdsCHROMIUM) {
  cmds::RegisterSharedIdsCHROMIUM& cmd =
      *GetBufferAs<cmds::RegisterSharedIdsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::RegisterSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.n);
  EXPECT_EQ(static_cast<uint32>(13), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableFeatureCHROMIUM) {
  cmds::EnableFeatureCHROMIUM& cmd =
      *GetBufferAs<cmds::EnableFeatureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::EnableFeatureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ResizeCHROMIUM) {
  cmds::ResizeCHROMIUM& cmd = *GetBufferAs<cmds::ResizeCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::ResizeCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.width);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.height);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.scale_factor);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetRequestableExtensionsCHROMIUM) {
  cmds::GetRequestableExtensionsCHROMIUM& cmd =
      *GetBufferAs<cmds::GetRequestableExtensionsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(
      static_cast<uint32>(cmds::GetRequestableExtensionsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RequestExtensionCHROMIUM) {
  cmds::RequestExtensionCHROMIUM& cmd =
      *GetBufferAs<cmds::RequestExtensionCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::RequestExtensionCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetMultipleIntegervCHROMIUM) {
  cmds::GetMultipleIntegervCHROMIUM& cmd =
      *GetBufferAs<cmds::GetMultipleIntegervCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11),
      static_cast<uint32>(12),
      static_cast<GLuint>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15),
      static_cast<GLsizeiptr>(16));
  EXPECT_EQ(static_cast<uint32>(cmds::GetMultipleIntegervCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.pnames_shm_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.pnames_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.count);
  EXPECT_EQ(static_cast<uint32>(14), cmd.results_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.results_shm_offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(16), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInfoCHROMIUM) {
  cmds::GetProgramInfoCHROMIUM& cmd =
      *GetBufferAs<cmds::GetProgramInfoCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetProgramInfoCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateStreamTextureCHROMIUM) {
  cmds::CreateStreamTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::CreateStreamTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::CreateStreamTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.client_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DestroyStreamTextureCHROMIUM) {
  cmds::DestroyStreamTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::DestroyStreamTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::DestroyStreamTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTranslatedShaderSourceANGLE) {
  cmds::GetTranslatedShaderSourceANGLE& cmd =
      *GetBufferAs<cmds::GetTranslatedShaderSourceANGLE>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::GetTranslatedShaderSourceANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PostSubBufferCHROMIUM) {
  cmds::PostSubBufferCHROMIUM& cmd =
      *GetBufferAs<cmds::PostSubBufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::PostSubBufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLint>(13), cmd.width);
  EXPECT_EQ(static_cast<GLint>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexImageIOSurface2DCHROMIUM) {
  cmds::TexImageIOSurface2DCHROMIUM& cmd =
      *GetBufferAs<cmds::TexImageIOSurface2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLsizei>(13),
      static_cast<GLuint>(14),
      static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::TexImageIOSurface2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.height);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.ioSurfaceId);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.plane);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTextureCHROMIUM) {
  cmds::CopyTextureCHROMIUM& cmd = *GetBufferAs<cmds::CopyTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLenum>(16));
  EXPECT_EQ(static_cast<uint32>(cmds::CopyTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.source_id);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.dest_id);
  EXPECT_EQ(static_cast<GLint>(14), cmd.level);
  EXPECT_EQ(static_cast<GLint>(15), cmd.internalformat);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.dest_type);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArraysInstancedANGLE) {
  cmds::DrawArraysInstancedANGLE& cmd =
      *GetBufferAs<cmds::DrawArraysInstancedANGLE>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::DrawArraysInstancedANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.primcount);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElementsInstancedANGLE) {
  cmds::DrawElementsInstancedANGLE& cmd =
      *GetBufferAs<cmds::DrawElementsInstancedANGLE>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14),
      static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::DrawElementsInstancedANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.primcount);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttribDivisorANGLE) {
  cmds::VertexAttribDivisorANGLE& cmd =
      *GetBufferAs<cmds::VertexAttribDivisorANGLE>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::VertexAttribDivisorANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.divisor);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Write test for GenMailboxCHROMIUM
TEST_F(GLES2FormatTest, ProduceTextureCHROMIUM) {
  cmds::ProduceTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::ProduceTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::ProduceTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<uint32>(12), cmd.mailbox_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.mailbox_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ProduceTextureCHROMIUMImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
  };
  cmds::ProduceTextureCHROMIUMImmediate& cmd =
      *GetBufferAs<cmds::ProduceTextureCHROMIUMImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::ProduceTextureCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, ConsumeTextureCHROMIUM) {
  cmds::ConsumeTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::ConsumeTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::ConsumeTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<uint32>(12), cmd.mailbox_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.mailbox_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ConsumeTextureCHROMIUMImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
    static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
  };
  cmds::ConsumeTextureCHROMIUMImmediate& cmd =
      *GetBufferAs<cmds::ConsumeTextureCHROMIUMImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::ConsumeTextureCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, BindUniformLocationCHROMIUM) {
  cmds::BindUniformLocationCHROMIUM& cmd =
      *GetBufferAs<cmds::BindUniformLocationCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(cmds::BindUniformLocationCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.name_shm_offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindUniformLocationCHROMIUMBucket) {
  cmds::BindUniformLocationCHROMIUMBucket& cmd =
      *GetBufferAs<cmds::BindUniformLocationCHROMIUMBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(
      static_cast<uint32>(cmds::BindUniformLocationCHROMIUMBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindTexImage2DCHROMIUM) {
  cmds::BindTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::BindTexImage2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::BindTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.imageId);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReleaseTexImage2DCHROMIUM) {
  cmds::ReleaseTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::ReleaseTexImage2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::ReleaseTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.imageId);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TraceBeginCHROMIUM) {
  cmds::TraceBeginCHROMIUM& cmd = *GetBufferAs<cmds::TraceBeginCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::TraceBeginCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TraceEndCHROMIUM) {
  cmds::TraceEndCHROMIUM& cmd = *GetBufferAs<cmds::TraceEndCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::TraceEndCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, AsyncTexSubImage2DCHROMIUM) {
  cmds::AsyncTexSubImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::AsyncTexSubImage2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLenum>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(cmds::AsyncTexSubImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32>(19), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, AsyncTexImage2DCHROMIUM) {
  cmds::AsyncTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::AsyncTexImage2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLenum>(17),
      static_cast<GLenum>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(cmds::AsyncTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.pixels_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, WaitAsyncTexImage2DCHROMIUM) {
  cmds::WaitAsyncTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::WaitAsyncTexImage2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::WaitAsyncTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DiscardFramebufferEXT) {
  cmds::DiscardFramebufferEXT& cmd =
      *GetBufferAs<cmds::DiscardFramebufferEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(cmds::DiscardFramebufferEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.attachments_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.attachments_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DiscardFramebufferEXTImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
    static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
    static_cast<GLenum>(kSomeBaseValueToTestWith + 1),
  };
  cmds::DiscardFramebufferEXTImmediate& cmd =
      *GetBufferAs<cmds::DiscardFramebufferEXTImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::DiscardFramebufferEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(1), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, LoseContextCHROMIUM) {
  cmds::LoseContextCHROMIUM& cmd = *GetBufferAs<cmds::LoseContextCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(cmds::LoseContextCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.current);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.other);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Write test for InsertSyncPointCHROMIUM
TEST_F(GLES2FormatTest, WaitSyncPointCHROMIUM) {
  cmds::WaitSyncPointCHROMIUM& cmd =
      *GetBufferAs<cmds::WaitSyncPointCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(cmds::WaitSyncPointCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sync_point);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawBuffersEXT) {
  cmds::DrawBuffersEXT& cmd = *GetBufferAs<cmds::DrawBuffersEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(cmds::DrawBuffersEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.count);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bufs_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.bufs_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawBuffersEXTImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLenum data[] = {
    static_cast<GLenum>(kSomeBaseValueToTestWith + 0),
  };
  cmds::DrawBuffersEXTImmediate& cmd =
      *GetBufferAs<cmds::DrawBuffersEXTImmediate>();
  const GLsizei kNumElements = 1;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLenum) * 1;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(1),
      data);
  EXPECT_EQ(static_cast<uint32>(cmds::DrawBuffersEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(1), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, DiscardBackbufferCHROMIUM) {
  cmds::DiscardBackbufferCHROMIUM& cmd =
      *GetBufferAs<cmds::DiscardBackbufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(cmds::DiscardBackbufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

