// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class ContextGroupTest : public testing::Test {
 public:
  ContextGroupTest() {
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    decoder_.reset(new MockGLES2Decoder());
    group_ = scoped_refptr<ContextGroup>(
        new ContextGroup(NULL, NULL, NULL, NULL, true));
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_ptr<MockGLES2Decoder> decoder_;
  scoped_refptr<ContextGroup> group_;
};

TEST_F(ContextGroupTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_EQ(0u, group_->max_vertex_attribs());
  EXPECT_EQ(0u, group_->max_texture_units());
  EXPECT_EQ(0u, group_->max_texture_image_units());
  EXPECT_EQ(0u, group_->max_vertex_texture_image_units());
  EXPECT_EQ(0u, group_->max_fragment_uniform_vectors());
  EXPECT_EQ(0u, group_->max_varying_vectors());
  EXPECT_EQ(0u, group_->max_vertex_uniform_vectors());
  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

TEST_F(ContextGroupTest, InitializeNoExtensions) {
  TestHelper::SetupContextGroupInitExpectations(gl_.get(),
      DisallowedFeatures(), "");
  group_->Initialize(decoder_.get(), DisallowedFeatures());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kNumVertexAttribs),
            group_->max_vertex_attribs());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kNumTextureUnits),
            group_->max_texture_units());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxTextureImageUnits),
            group_->max_texture_image_units());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxVertexTextureImageUnits),
             group_->max_vertex_texture_image_units());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxFragmentUniformVectors),
            group_->max_fragment_uniform_vectors());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxVaryingVectors),
            group_->max_varying_vectors());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxVertexUniformVectors),
            group_->max_vertex_uniform_vectors());
  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);

  group_->Destroy(decoder_.get(), false);
  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

TEST_F(ContextGroupTest, MultipleContexts) {
  scoped_ptr<MockGLES2Decoder> decoder2_(new MockGLES2Decoder());
  TestHelper::SetupContextGroupInitExpectations(gl_.get(),
      DisallowedFeatures(), "");
  group_->Initialize(decoder_.get(), DisallowedFeatures());
  group_->Initialize(decoder2_.get(), DisallowedFeatures());

  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);

  group_->Destroy(decoder_.get(), false);

  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);

  group_->Destroy(decoder2_.get(), false);

  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

}  // namespace gles2
}  // namespace gpu


