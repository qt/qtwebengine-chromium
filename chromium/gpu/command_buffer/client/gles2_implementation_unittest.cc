// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for GLES2Implementation.

#include "gpu/command_buffer/client/gles2_implementation.h"

#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/program_info_manager.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
#define GLES2_SUPPORT_CLIENT_SIDE_ARRAYS
#endif

using testing::_;
using testing::AtLeast;
using testing::AnyNumber;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Sequence;
using testing::StrictMock;
using testing::Truly;
using testing::Return;

namespace gpu {
namespace gles2 {

ACTION_P2(SetMemory, dst, obj) {
  memcpy(dst, &obj, sizeof(obj));
}

ACTION_P3(SetMemoryFromArray, dst, array, size) {
  memcpy(dst, array, size);
}

// Used to help set the transfer buffer result to SizedResult of a single value.
template <typename T>
class SizedResultHelper {
 public:
  explicit SizedResultHelper(T result)
      : size_(sizeof(result)),
        result_(result) {
  }

 private:
  uint32 size_;
  T result_;
};

// Struct to make it easy to pass a vec4 worth of floats.
struct FourFloats {
  FourFloats(float _x, float _y, float _z, float _w)
      : x(_x),
        y(_y),
        z(_z),
        w(_w) {
  }

  float x;
  float y;
  float z;
  float w;
};

#pragma pack(push, 1)
// Struct that holds 7 characters.
struct Str7 {
  char str[7];
};
#pragma pack(pop)

class MockTransferBuffer : public TransferBufferInterface {
 public:
  struct ExpectedMemoryInfo {
    uint32 offset;
    int32 id;
    uint8* ptr;
  };

  MockTransferBuffer(
      CommandBuffer* command_buffer,
      unsigned int size,
      unsigned int result_size,
      unsigned int alignment)
      : command_buffer_(command_buffer),
        size_(size),
        result_size_(result_size),
        alignment_(alignment),
        actual_buffer_index_(0),
        expected_buffer_index_(0),
        last_alloc_(NULL),
        expected_offset_(result_size),
        actual_offset_(result_size) {
    // We have to allocate the buffers here because
    // we need to know their address before GLES2Implementation::Initialize
    // is called.
    for (int ii = 0; ii < kNumBuffers; ++ii) {
      buffers_[ii] = command_buffer_->CreateTransferBuffer(
          size_ + ii * alignment_,
          &buffer_ids_[ii]);
      EXPECT_NE(-1, buffer_ids_[ii]);
    }
  }

  virtual ~MockTransferBuffer() { }

  virtual bool Initialize(
      unsigned int starting_buffer_size,
      unsigned int result_size,
      unsigned int /* min_buffer_size */,
      unsigned int /* max_buffer_size */,
      unsigned int alignment,
      unsigned int size_to_flush) OVERRIDE;
  virtual int GetShmId() OVERRIDE;
  virtual void* GetResultBuffer() OVERRIDE;
  virtual int GetResultOffset() OVERRIDE;
  virtual void Free() OVERRIDE;
  virtual bool HaveBuffer() const OVERRIDE;
  virtual void* AllocUpTo(
      unsigned int size, unsigned int* size_allocated) OVERRIDE;
  virtual void* Alloc(unsigned int size) OVERRIDE;
  virtual RingBuffer::Offset GetOffset(void* pointer) const OVERRIDE;
  virtual void FreePendingToken(void* p, unsigned int /* token */) OVERRIDE;

  size_t MaxTransferBufferSize() {
    return size_ - result_size_;
  }

  unsigned int RoundToAlignment(unsigned int size) {
    return (size + alignment_ - 1) & ~(alignment_ - 1);
  }

  bool InSync() {
    return expected_buffer_index_ == actual_buffer_index_ &&
           expected_offset_ == actual_offset_;
  }

  ExpectedMemoryInfo GetExpectedMemory(size_t size) {
    ExpectedMemoryInfo mem;
    mem.offset = AllocateExpectedTransferBuffer(size);
    mem.id = GetExpectedTransferBufferId();
    mem.ptr = static_cast<uint8*>(
       GetExpectedTransferAddressFromOffset(mem.offset, size));
    return mem;
  }

  ExpectedMemoryInfo GetExpectedResultMemory(size_t size) {
    ExpectedMemoryInfo mem;
    mem.offset = GetExpectedResultBufferOffset();
    mem.id = GetExpectedResultBufferId();
    mem.ptr = static_cast<uint8*>(
        GetExpectedTransferAddressFromOffset(mem.offset, size));
    return mem;
  }

 private:
  static const int kNumBuffers = 2;

  uint8* actual_buffer() const {
    return static_cast<uint8*>(buffers_[actual_buffer_index_].ptr);
  }

  uint8* expected_buffer() const {
    return static_cast<uint8*>(buffers_[expected_buffer_index_].ptr);
  }

  uint32 AllocateExpectedTransferBuffer(size_t size) {
    EXPECT_LE(size, MaxTransferBufferSize());

    // Toggle which buffer we get each time to simulate the buffer being
    // reallocated.
    expected_buffer_index_ = (expected_buffer_index_ + 1) % kNumBuffers;

    if (expected_offset_ + size > size_) {
      expected_offset_ = result_size_;
    }
    uint32 offset = expected_offset_;
    expected_offset_ += RoundToAlignment(size);

    // Make sure each buffer has a different offset.
    return offset + expected_buffer_index_ * alignment_;
  }

  void* GetExpectedTransferAddressFromOffset(uint32 offset, size_t size) {
    EXPECT_GE(offset, expected_buffer_index_ * alignment_);
    EXPECT_LE(offset + size, size_ + expected_buffer_index_ * alignment_);
    return expected_buffer() + offset;
  }

  int GetExpectedResultBufferId() {
    return buffer_ids_[expected_buffer_index_];
  }

  uint32 GetExpectedResultBufferOffset() {
    return expected_buffer_index_ * alignment_;
  }

  int GetExpectedTransferBufferId() {
    return buffer_ids_[expected_buffer_index_];
  }

  CommandBuffer* command_buffer_;
  size_t size_;
  size_t result_size_;
  uint32 alignment_;
  int buffer_ids_[kNumBuffers];
  gpu::Buffer buffers_[kNumBuffers];
  int actual_buffer_index_;
  int expected_buffer_index_;
  void* last_alloc_;
  uint32 expected_offset_;
  uint32 actual_offset_;

  DISALLOW_COPY_AND_ASSIGN(MockTransferBuffer);
};

bool MockTransferBuffer::Initialize(
    unsigned int starting_buffer_size,
    unsigned int result_size,
    unsigned int /* min_buffer_size */,
    unsigned int /* max_buffer_size */,
    unsigned int alignment,
    unsigned int /* size_to_flush */) {
  // Just check they match.
  return size_ == starting_buffer_size &&
         result_size_ == result_size &&
         alignment_ == alignment;
};

int MockTransferBuffer::GetShmId() {
  return buffer_ids_[actual_buffer_index_];
}

void* MockTransferBuffer::GetResultBuffer() {
  return actual_buffer() + actual_buffer_index_ * alignment_;
}

int MockTransferBuffer::GetResultOffset() {
  return actual_buffer_index_ * alignment_;
}

void MockTransferBuffer::Free() {
  GPU_NOTREACHED();
}

bool MockTransferBuffer::HaveBuffer() const {
  return true;
}

void* MockTransferBuffer::AllocUpTo(
    unsigned int size, unsigned int* size_allocated) {
  EXPECT_TRUE(size_allocated != NULL);
  EXPECT_TRUE(last_alloc_ == NULL);

  // Toggle which buffer we get each time to simulate the buffer being
  // reallocated.
  actual_buffer_index_ = (actual_buffer_index_ + 1) % kNumBuffers;

  size = std::min(static_cast<size_t>(size), MaxTransferBufferSize());
  if (actual_offset_ + size > size_) {
    actual_offset_ = result_size_;
  }
  uint32 offset = actual_offset_;
  actual_offset_ += RoundToAlignment(size);
  *size_allocated = size;

  // Make sure each buffer has a different offset.
  last_alloc_ = actual_buffer() + offset + actual_buffer_index_ * alignment_;
  return last_alloc_;
}

void* MockTransferBuffer::Alloc(unsigned int size) {
  EXPECT_LE(size, MaxTransferBufferSize());
  unsigned int temp = 0;
  void* p = AllocUpTo(size, &temp);
  EXPECT_EQ(temp, size);
  return p;
}

RingBuffer::Offset MockTransferBuffer::GetOffset(void* pointer) const {
  // Make sure each buffer has a different offset.
  return static_cast<uint8*>(pointer) - actual_buffer();
}

void MockTransferBuffer::FreePendingToken(void* p, unsigned int /* token */) {
  EXPECT_EQ(last_alloc_, p);
  last_alloc_ = NULL;
}

class GLES2ImplementationTest : public testing::Test {
 protected:
  static const uint8 kInitialValue = 0xBD;
  static const int32 kNumCommandEntries = 500;
  static const int32 kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const size_t kTransferBufferSize = 512;

  static const GLint kMaxCombinedTextureImageUnits = 8;
  static const GLint kMaxCubeMapTextureSize = 64;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxRenderbufferSize = 64;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxTextureSize = 128;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVertexAttribs = 8;
  static const GLint kMaxVertexTextureImageUnits = 0;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kNumCompressedTextureFormats = 0;
  static const GLint kNumShaderBinaryFormats = 0;
  static const GLuint kStartId = 1024;
  static const GLuint kBuffersStartId =
      GLES2Implementation::kClientSideArrayId + 2;
  static const GLuint kFramebuffersStartId = 1;
  static const GLuint kProgramsAndShadersStartId = 1;
  static const GLuint kRenderbuffersStartId = 1;
  static const GLuint kTexturesStartId = 1;
  static const GLuint kQueriesStartId = 1;
  static const GLuint kVertexArraysStartId = 1;

  typedef MockTransferBuffer::ExpectedMemoryInfo ExpectedMemoryInfo;

  GLES2ImplementationTest()
      : commands_(NULL),
        token_(0) {
  }

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  bool NoCommandsWritten() {
    Buffer ring_buffer = helper_->get_ring_buffer();
    const uint8* cmds = reinterpret_cast<const uint8*>(ring_buffer.ptr);
    const uint8* end = cmds + ring_buffer.size;
    for (; cmds < end; ++cmds) {
      if (*cmds != kInitialValue) {
        return false;
      }
    }
    return true;
  }

  QueryTracker::Query* GetQuery(GLuint id) {
    return gl_->query_tracker_->GetQuery(id);
  }

  void Initialize(bool bind_generates_resource) {
    command_buffer_.reset(new StrictMock<MockClientCommandBuffer>());
    ASSERT_TRUE(command_buffer_->Initialize());

    transfer_buffer_.reset(new MockTransferBuffer(
        command_buffer(),
        kTransferBufferSize,
        GLES2Implementation::kStartingOffset,
        GLES2Implementation::kAlignment));

    helper_.reset(new GLES2CmdHelper(command_buffer()));
    helper_->Initialize(kCommandBufferSizeBytes);

    gpu_control_.reset(new StrictMock<MockClientGpuControl>());

    GLES2Implementation::GLStaticState state;
    GLES2Implementation::GLStaticState::IntState& int_state = state.int_state;
    int_state.max_combined_texture_image_units = kMaxCombinedTextureImageUnits;
    int_state.max_cube_map_texture_size = kMaxCubeMapTextureSize;
    int_state.max_fragment_uniform_vectors = kMaxFragmentUniformVectors;
    int_state.max_renderbuffer_size = kMaxRenderbufferSize;
    int_state.max_texture_image_units = kMaxTextureImageUnits;
    int_state.max_texture_size = kMaxTextureSize;
    int_state.max_varying_vectors = kMaxVaryingVectors;
    int_state.max_vertex_attribs = kMaxVertexAttribs;
    int_state.max_vertex_texture_image_units = kMaxVertexTextureImageUnits;
    int_state.max_vertex_uniform_vectors = kMaxVertexUniformVectors;
    int_state.num_compressed_texture_formats = kNumCompressedTextureFormats;
    int_state.num_shader_binary_formats = kNumShaderBinaryFormats;

    // This just happens to work for now because IntState has 1 GLint per state.
    // If IntState gets more complicated this code will need to get more
    // complicated.
    ExpectedMemoryInfo mem1 = GetExpectedMemory(
        sizeof(GLES2Implementation::GLStaticState::IntState) * 2 +
        sizeof(cmds::GetShaderPrecisionFormat::Result) * 12);

    {
      InSequence sequence;

      EXPECT_CALL(*command_buffer(), OnFlush())
          .WillOnce(SetMemory(mem1.ptr + sizeof(int_state), int_state))
          .RetiresOnSaturation();
      GetNextToken();  // eat the token that starting up will use.

      gl_.reset(new GLES2Implementation(
          helper_.get(),
          NULL,
          transfer_buffer_.get(),
          bind_generates_resource,
          gpu_control_.get()));
      ASSERT_TRUE(gl_->Initialize(
          kTransferBufferSize,
          kTransferBufferSize,
          kTransferBufferSize,
          GLES2Implementation::kNoLimit));
    }

    EXPECT_CALL(*command_buffer(), OnFlush())
        .Times(1)
        .RetiresOnSaturation();
    helper_->CommandBufferHelper::Finish();
    ::testing::Mock::VerifyAndClearExpectations(gl_.get());

    Buffer ring_buffer = helper_->get_ring_buffer();
    commands_ = static_cast<CommandBufferEntry*>(ring_buffer.ptr) +
                command_buffer()->GetState().put_offset;
    ClearCommands();
    EXPECT_TRUE(transfer_buffer_->InSync());

    ::testing::Mock::VerifyAndClearExpectations(command_buffer());
  }

  MockClientCommandBuffer* command_buffer() const {
    return command_buffer_.get();
  }

  int GetNextToken() {
    return ++token_;
  }

  const void* GetPut() {
    return helper_->GetSpace(0);
  }

  void ClearCommands() {
    Buffer ring_buffer = helper_->get_ring_buffer();
    memset(ring_buffer.ptr, kInitialValue, ring_buffer.size);
  }

  size_t MaxTransferBufferSize() {
    return transfer_buffer_->MaxTransferBufferSize();
  }

  ExpectedMemoryInfo GetExpectedMemory(size_t size) {
    return transfer_buffer_->GetExpectedMemory(size);
  }

  ExpectedMemoryInfo GetExpectedResultMemory(size_t size) {
    return transfer_buffer_->GetExpectedResultMemory(size);
  }

  // Sets the ProgramInfoManager. The manager will be owned
  // by the ShareGroup.
  void SetProgramInfoManager(ProgramInfoManager* manager) {
    gl_->share_group()->set_program_info_manager(manager);
  }

  int CheckError() {
    ExpectedMemoryInfo result =
        GetExpectedResultMemory(sizeof(cmds::GetError::Result));
    EXPECT_CALL(*command_buffer(), OnFlush())
        .WillOnce(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
        .RetiresOnSaturation();
    return gl_->GetError();
  }

  bool GetBucketContents(uint32 bucket_id, std::vector<int8>* data) {
    return gl_->GetBucketContents(bucket_id, data);
  }

  Sequence sequence_;
  scoped_ptr<MockClientCommandBuffer> command_buffer_;
  scoped_ptr<MockClientGpuControl> gpu_control_;
  scoped_ptr<GLES2CmdHelper> helper_;
  scoped_ptr<MockTransferBuffer> transfer_buffer_;
  scoped_ptr<GLES2Implementation> gl_;
  CommandBufferEntry* commands_;
  int token_;
};

void GLES2ImplementationTest::SetUp() {
  Initialize(true);
}

void GLES2ImplementationTest::TearDown() {
  Mock::VerifyAndClear(gl_.get());
  EXPECT_CALL(*command_buffer(), OnFlush()).Times(AnyNumber());
  // For command buffer.
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(AtLeast(1));
  gl_.reset();
}

class GLES2ImplementationStrictSharedTest : public GLES2ImplementationTest {
 protected:
  virtual void SetUp() OVERRIDE;
};

void GLES2ImplementationStrictSharedTest::SetUp() {
  Initialize(false);
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const uint8 GLES2ImplementationTest::kInitialValue;
const int32 GLES2ImplementationTest::kNumCommandEntries;
const int32 GLES2ImplementationTest::kCommandBufferSizeBytes;
const size_t GLES2ImplementationTest::kTransferBufferSize;
const GLint GLES2ImplementationTest::kMaxCombinedTextureImageUnits;
const GLint GLES2ImplementationTest::kMaxCubeMapTextureSize;
const GLint GLES2ImplementationTest::kMaxFragmentUniformVectors;
const GLint GLES2ImplementationTest::kMaxRenderbufferSize;
const GLint GLES2ImplementationTest::kMaxTextureImageUnits;
const GLint GLES2ImplementationTest::kMaxTextureSize;
const GLint GLES2ImplementationTest::kMaxVaryingVectors;
const GLint GLES2ImplementationTest::kMaxVertexAttribs;
const GLint GLES2ImplementationTest::kMaxVertexTextureImageUnits;
const GLint GLES2ImplementationTest::kMaxVertexUniformVectors;
const GLint GLES2ImplementationTest::kNumCompressedTextureFormats;
const GLint GLES2ImplementationTest::kNumShaderBinaryFormats;
const GLuint GLES2ImplementationTest::kStartId;
const GLuint GLES2ImplementationTest::kBuffersStartId;
const GLuint GLES2ImplementationTest::kFramebuffersStartId;
const GLuint GLES2ImplementationTest::kProgramsAndShadersStartId;
const GLuint GLES2ImplementationTest::kRenderbuffersStartId;
const GLuint GLES2ImplementationTest::kTexturesStartId;
const GLuint GLES2ImplementationTest::kQueriesStartId;
const GLuint GLES2ImplementationTest::kVertexArraysStartId;
#endif

TEST_F(GLES2ImplementationTest, Basic) {
  EXPECT_TRUE(gl_->share_group() != NULL);
}

TEST_F(GLES2ImplementationTest, GetBucketContents) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const uint32 kTestSize = MaxTransferBufferSize() + 32;

  scoped_ptr<uint8[]> buf(new uint8 [kTestSize]);
  uint8* expected_data = buf.get();
  for (uint32 ii = 0; ii < kTestSize; ++ii) {
    expected_data[ii] = ii * 3;
  }

  struct Cmds {
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::GetBucketData get_bucket_data;
    cmd::SetToken set_token2;
    cmd::SetBucketSize set_bucket_size2;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(sizeof(uint32));
  ExpectedMemoryInfo mem2 = GetExpectedMemory(
      kTestSize - MaxTransferBufferSize());

  Cmds expected;
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.get_bucket_data.Init(
      kBucketId, MaxTransferBufferSize(),
      kTestSize - MaxTransferBufferSize(), mem2.id, mem2.offset);
  expected.set_bucket_size2.Init(kBucketId, 0);
  expected.set_token2.Init(GetNextToken());

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(
          SetMemory(result1.ptr, kTestSize),
          SetMemoryFromArray(
              mem1.ptr, expected_data, MaxTransferBufferSize())))
      .WillOnce(SetMemoryFromArray(
          mem2.ptr, expected_data + MaxTransferBufferSize(),
          kTestSize - MaxTransferBufferSize()))
      .RetiresOnSaturation();

  std::vector<int8> data;
  GetBucketContents(kBucketId, &data);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ASSERT_EQ(kTestSize, data.size());
  EXPECT_EQ(0, memcmp(expected_data, &data[0], data.size()));
}

TEST_F(GLES2ImplementationTest, GetShaderPrecisionFormat) {
  struct Cmds {
    cmds::GetShaderPrecisionFormat cmd;
  };
  typedef cmds::GetShaderPrecisionFormat::Result Result;

  // The first call for mediump should trigger a command buffer request.
  GLint range1[2] = {0, 0};
  GLint precision1 = 0;
  Cmds expected1;
  ExpectedMemoryInfo client_result1 = GetExpectedResultMemory(4);
  expected1.cmd.Init(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT,
                     client_result1.id, client_result1.offset);
  Result server_result1 = {true, 14, 14, 10};
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(client_result1.ptr, server_result1))
      .RetiresOnSaturation();
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT,
                                range1, &precision1);
  const void* commands2 = GetPut();
  EXPECT_NE(commands_, commands2);
  EXPECT_EQ(0, memcmp(&expected1, commands_, sizeof(expected1)));
  EXPECT_EQ(range1[0], 14);
  EXPECT_EQ(range1[1], 14);
  EXPECT_EQ(precision1, 10);

  // The second call for mediump should use the cached value and avoid
  // triggering a command buffer request, so we do not expect a call to
  // OnFlush() here. We do expect the results to be correct though.
  GLint range2[2] = {0, 0};
  GLint precision2 = 0;
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT,
                                range2, &precision2);
  const void* commands3 = GetPut();
  EXPECT_EQ(commands2, commands3);
  EXPECT_EQ(range2[0], 14);
  EXPECT_EQ(range2[1], 14);
  EXPECT_EQ(precision2, 10);

  // If we then make a request for highp, we should get another command
  // buffer request since it hasn't been cached yet.
  GLint range3[2] = {0, 0};
  GLint precision3 = 0;
  Cmds expected3;
  ExpectedMemoryInfo result3 = GetExpectedResultMemory(4);
  expected3.cmd.Init(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT,
                     result3.id, result3.offset);
  Result result3_source = {true, 62, 62, 16};
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result3.ptr, result3_source))
      .RetiresOnSaturation();
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT,
                                range3, &precision3);
  const void* commands4 = GetPut();
  EXPECT_NE(commands3, commands4);
  EXPECT_EQ(0, memcmp(&expected3, commands3, sizeof(expected3)));
  EXPECT_EQ(range3[0], 62);
  EXPECT_EQ(range3[1], 62);
  EXPECT_EQ(precision3, 16);
}

TEST_F(GLES2ImplementationTest, ShaderSource) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kShaderId = 456;
  const char* kString1 = "foobar";
  const char* kString2 = "barfoo";
  const size_t kString1Size = strlen(kString1);
  const size_t kString2Size = strlen(kString2);
  const size_t kString3Size = 1;  // Want the NULL;
  const size_t kSourceSize = kString1Size + kString2Size + kString3Size;
  const size_t kPaddedString1Size =
      transfer_buffer_->RoundToAlignment(kString1Size);
  const size_t kPaddedString2Size =
      transfer_buffer_->RoundToAlignment(kString2Size);
  const size_t kPaddedString3Size =
      transfer_buffer_->RoundToAlignment(kString3Size);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_data1;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data2;
    cmd::SetToken set_token2;
    cmd::SetBucketData set_bucket_data3;
    cmd::SetToken set_token3;
    cmds::ShaderSourceBucket shader_source_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedString1Size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kPaddedString2Size);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kPaddedString3Size);

  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_data1.Init(
      kBucketId, 0, kString1Size, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data2.Init(
      kBucketId, kString1Size, kString2Size, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_bucket_data3.Init(
      kBucketId, kString1Size + kString2Size,
      kString3Size, mem3.id, mem3.offset);
  expected.set_token3.Init(GetNextToken());
  expected.shader_source_bucket.Init(kShaderId, kBucketId);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* strings[] = {
    kString1,
    kString2,
  };
  gl_->ShaderSource(kShaderId, 2, strings, NULL);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GetShaderSource) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kShaderId = 456;
  const Str7 kString = {"foobar"};
  const char kBad = 0x12;
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetShaderSource get_shader_source;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(sizeof(uint32));

  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_shader_source.Init(kShaderId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  char buf[sizeof(kString) + 1];
  memset(buf, kBad, sizeof(buf));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .RetiresOnSaturation();

  GLsizei length = 0;
  gl_->GetShaderSource(kShaderId, sizeof(buf), &length, buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(sizeof(kString) - 1, static_cast<size_t>(length));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

TEST_F(GLES2ImplementationTest, DrawArraysClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawArrays draw;
    cmds::BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLint kFirst = 1;
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kFirst, kCount);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawArrays(GL_POINTS, kFirst, kCount);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArraysInstancedANGLEClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::VertexAttribDivisorANGLE divisor;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawArraysInstancedANGLE draw;
    cmds::BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLint kFirst = 1;
  const GLsizei kCount = 2;
  const GLuint kDivisor = 1;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      1 * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.divisor.Init(kAttribIndex2, kDivisor);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kFirst, kCount, 1);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribDivisorANGLE(kAttribIndex2, kDivisor);
  gl_->DrawArraysInstancedANGLE(GL_POINTS, kFirst, kCount, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint16 indices[] = {
    1, 2,
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_index_emu;
    cmds::BufferData set_index_size;
    cmds::BufferSubData copy_data0;
    cmd::SetToken set_token0;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElements draw;
    cmds::BindBuffer restore;
    cmds::BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kIndexSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, mem1.id, mem1.offset);
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, 0);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_SHORT, indices);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffersIndexUint) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint32 indices[] = {
    1, 2,
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_index_emu;
    cmds::BufferData set_index_size;
    cmds::BufferSubData copy_data0;
    cmd::SetToken set_token0;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElements draw;
    cmds::BindBuffer restore;
    cmds::BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kIndexSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, mem1.id, mem1.offset);
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_INT, 0);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_INT, indices);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffersInvalidIndexUint) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint32 indices[] = {
    1, 0x90000000
  };

  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;

  EXPECT_CALL(*command_buffer(), OnFlush())
      .Times(1)
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_INT, indices);

  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), gl_->GetError());
}

TEST_F(GLES2ImplementationTest,
       DrawElementsClientSideBuffersServiceSideIndices) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_index;
    cmds::GetMaxValueInBufferCHROMIUM get_max;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElements draw;
    cmds::BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kClientIndexBufferId = 0x789;
  const GLuint kIndexOffset = 0x40;
  const GLuint kMaxIndex = 2;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedResultMemory(sizeof(uint32));
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);


  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index.Init(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  expected.get_max.Init(kClientIndexBufferId, kCount, GL_UNSIGNED_SHORT,
                        kIndexOffset, mem1.id, mem1.offset);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, kIndexOffset);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(mem1.ptr,kMaxIndex))
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_SHORT,
                    reinterpret_cast<const void*>(kIndexOffset));
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsInstancedANGLEClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint16 indices[] = {
    1, 2,
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::VertexAttribDivisorANGLE divisor;
    cmds::BindBuffer bind_to_index_emu;
    cmds::BufferData set_index_size;
    cmds::BufferSubData copy_data0;
    cmd::SetToken set_token0;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElementsInstancedANGLE draw;
    cmds::BindBuffer restore;
    cmds::BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      1 * kNumComponents2 * sizeof(verts[0][0]);
  const GLuint kDivisor = 1;
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kIndexSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.divisor.Init(kAttribIndex2, kDivisor);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, mem1.id, mem1.offset);
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, 0, 1);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribDivisorANGLE(kAttribIndex2, kDivisor);
  gl_->DrawElementsInstancedANGLE(
      GL_POINTS, kCount, GL_UNSIGNED_SHORT, indices, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GetVertexBufferPointerv) {
  static const float verts[1] = { 0.0f, };
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kStride1 = 12;
  const GLsizei kStride2 = 0;
  const GLuint kBufferId = 0x123;
  const GLint kOffset2 = 0x456;

  // It's all cached on the client side so no get commands are issued.
  struct Cmds {
    cmds::BindBuffer bind;
    cmds::VertexAttribPointer set_pointer;
  };

  Cmds expected;
  expected.bind.Init(GL_ARRAY_BUFFER, kBufferId);
  expected.set_pointer.Init(kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE,
                            kStride2, kOffset2);

  // Set one client side buffer.
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kStride1, verts);
  // Set one VBO
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kStride2,
                           reinterpret_cast<const void*>(kOffset2));
  // now get them both.
  void* ptr1 = NULL;
  void* ptr2 = NULL;

  gl_->GetVertexAttribPointerv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr1);
  gl_->GetVertexAttribPointerv(
      kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr2);

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(static_cast<const void*>(&verts) == ptr1);
  EXPECT_TRUE(ptr2 == reinterpret_cast<void*>(kOffset2));
}

TEST_F(GLES2ImplementationTest, GetVertexAttrib) {
  static const float verts[1] = { 0.0f, };
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kStride1 = 12;
  const GLsizei kStride2 = 0;
  const GLuint kBufferId = 0x123;
  const GLint kOffset2 = 0x456;

  // Only one set and one get because the client side buffer's info is stored
  // on the client side.
  struct Cmds {
    cmds::EnableVertexAttribArray enable;
    cmds::BindBuffer bind;
    cmds::VertexAttribPointer set_pointer;
    cmds::GetVertexAttribfv get2;  // for getting the value from attrib1
  };

  ExpectedMemoryInfo mem2 = GetExpectedResultMemory(16);

  Cmds expected;
  expected.enable.Init(kAttribIndex1);
  expected.bind.Init(GL_ARRAY_BUFFER, kBufferId);
  expected.set_pointer.Init(kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE,
                            kStride2, kOffset2);
  expected.get2.Init(kAttribIndex1,
                     GL_CURRENT_VERTEX_ATTRIB,
                     mem2.id, mem2.offset);

  FourFloats current_attrib(1.2f, 3.4f, 5.6f, 7.8f);

  // One call to flush to wait for last call to GetVertexAttribiv
  // as others are all cached.
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(
          mem2.ptr, SizedResultHelper<FourFloats>(current_attrib)))
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  // Set one client side buffer.
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kStride1, verts);
  // Set one VBO
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kStride2,
                           reinterpret_cast<const void*>(kOffset2));
  // first get the service side once to see that we make a command
  GLint buffer_id = 0;
  GLint enabled = 0;
  GLint size = 0;
  GLint stride = 0;
  GLint type = 0;
  GLint normalized = 1;
  float current[4] = { 0.0f, };

  gl_->GetVertexAttribiv(
      kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer_id);
  EXPECT_EQ(kBufferId, static_cast<GLuint>(buffer_id));
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer_id);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
  gl_->GetVertexAttribfv(
      kAttribIndex1, GL_CURRENT_VERTEX_ATTRIB, &current[0]);

  EXPECT_EQ(0, buffer_id);
  EXPECT_EQ(GL_TRUE, enabled);
  EXPECT_EQ(kNumComponents1, size);
  EXPECT_EQ(kStride1, stride);
  EXPECT_EQ(GL_FLOAT, type);
  EXPECT_EQ(GL_FALSE, normalized);
  EXPECT_EQ(0, memcmp(&current_attrib, &current, sizeof(current_attrib)));

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReservedIds) {
  // Only the get error command should be issued.
  struct Cmds {
    cmds::GetError get;
  };
  Cmds expected;

  ExpectedMemoryInfo mem1 = GetExpectedResultMemory(
      sizeof(cmds::GetError::Result));

  expected.get.Init(mem1.id, mem1.offset);

  // One call to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(mem1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gl_->BindBuffer(
      GL_ARRAY_BUFFER,
      GLES2Implementation::kClientSideArrayId);
  gl_->BindBuffer(
      GL_ARRAY_BUFFER,
      GLES2Implementation::kClientSideElementArrayId);
  GLenum err = gl_->GetError();
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), err);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

TEST_F(GLES2ImplementationTest, ReadPixels2Reads) {
  struct Cmds {
    cmds::ReadPixels read1;
    cmd::SetToken set_token1;
    cmds::ReadPixels read2;
    cmd::SetToken set_token2;
  };
  const GLint kBytesPerPixel = 4;
  const GLint kWidth =
      (kTransferBufferSize - GLES2Implementation::kStartingOffset) /
      kBytesPerPixel;
  const GLint kHeight = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(kWidth * kHeight / 2 * kBytesPerPixel);
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::ReadPixels::Result));
  ExpectedMemoryInfo mem2 =
      GetExpectedMemory(kWidth * kHeight / 2 * kBytesPerPixel);
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::ReadPixels::Result));

  Cmds expected;
  expected.read1.Init(
      0, 0, kWidth, kHeight / 2, kFormat, kType,
      mem1.id, mem1.offset, result1.id, result1.offset,
      false);
  expected.set_token1.Init(GetNextToken());
  expected.read2.Init(
      0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      mem2.id, mem2.offset, result2.id, result2.offset, false);
  expected.set_token2.Init(GetNextToken());
  scoped_ptr<int8[]> buffer(new int8[kWidth * kHeight * kBytesPerPixel]);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, static_cast<uint32>(1)))
      .WillOnce(SetMemory(result2.ptr, static_cast<uint32>(1)))
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.get());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReadPixelsBadFormatType) {
  struct Cmds {
    cmds::ReadPixels read;
    cmd::SetToken set_token;
  };
  const GLint kBytesPerPixel = 4;
  const GLint kWidth = 2;
  const GLint kHeight = 2;
  const GLenum kFormat = 0;
  const GLenum kType = 0;

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(kWidth * kHeight * kBytesPerPixel);
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::ReadPixels::Result));

  Cmds expected;
  expected.read.Init(
      0, 0, kWidth, kHeight, kFormat, kType,
      mem1.id, mem1.offset, result1.id, result1.offset, false);
  expected.set_token.Init(GetNextToken());
  scoped_ptr<int8[]> buffer(new int8[kWidth * kHeight * kBytesPerPixel]);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .Times(1)
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.get());
}

TEST_F(GLES2ImplementationTest, FreeUnusedSharedMemory) {
  struct Cmds {
    cmds::BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kSize);

  Cmds expected;
  expected.buf.Init(
    kTarget, kOffset, kSize, mem1.id, mem1.offset);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapBufferSubDataCHROMIUM(
      kTarget, kOffset, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem != NULL);
  gl_->UnmapBufferSubDataCHROMIUM(mem);
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  gl_->FreeUnusedSharedMemory();
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUM) {
  struct Cmds {
    cmds::BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  uint32 offset = 0;
  Cmds expected;
  expected.buf.Init(
      kTarget, kOffset, kSize,
      command_buffer()->GetNextFreeTransferBufferId(), offset);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapBufferSubDataCHROMIUM(
      kTarget, kOffset, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem != NULL);
  gl_->UnmapBufferSubDataCHROMIUM(mem);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUMBadArgs) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  void* mem;
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, -1, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, kOffset, -1, GL_WRITE_ONLY);
  ASSERT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, kOffset, kSize, GL_READ_ONLY);
  ASSERT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  const char* kPtr = "something";
  gl_->UnmapBufferSubDataCHROMIUM(kPtr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, MapUnmapTexSubImage2DCHROMIUM) {
  struct Cmds {
    cmds::TexSubImage2D tex;
    cmd::SetToken set_token;
  };
  const GLint kLevel = 1;
  const GLint kXOffset = 2;
  const GLint kYOffset = 3;
  const GLint kWidth = 4;
  const GLint kHeight = 5;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  uint32 offset = 0;
  Cmds expected;
  expected.tex.Init(
      GL_TEXTURE_2D, kLevel, kXOffset, kYOffset, kWidth, kHeight, kFormat,
      kType,
      command_buffer()->GetNextFreeTransferBufferId(), offset, GL_FALSE);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapTexSubImage2DCHROMIUM(
      GL_TEXTURE_2D,
      kLevel,
      kXOffset,
      kYOffset,
      kWidth,
      kHeight,
      kFormat,
      kType,
      GL_WRITE_ONLY);
  ASSERT_TRUE(mem != NULL);
  gl_->UnmapTexSubImage2DCHROMIUM(mem);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapUnmapTexSubImage2DCHROMIUMBadArgs) {
  const GLint kLevel = 1;
  const GLint kXOffset = 2;
  const GLint kYOffset = 3;
  const GLint kWidth = 4;
  const GLint kHeight = 5;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result5 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result6 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result7 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result5.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result6.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result7.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  void* mem;
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    -1,
    kXOffset,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    -1,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    -1,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    -1,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    kWidth,
    -1,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_READ_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  const char* kPtr = "something";
  gl_->UnmapTexSubImage2DCHROMIUM(kPtr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetMultipleIntegervCHROMIUMValidArgs) {
  const GLenum pnames[] = {
    GL_DEPTH_WRITEMASK,
    GL_COLOR_WRITEMASK,
    GL_STENCIL_WRITEMASK,
  };
  const GLint num_results = 6;
  GLint results[num_results + 1];
  struct Cmds {
    cmds::GetMultipleIntegervCHROMIUM get_multiple;
    cmd::SetToken set_token;
  };
  const GLsizei kNumPnames = arraysize(pnames);
  const GLsizeiptr kResultsSize = num_results * sizeof(results[0]);
  const size_t kPNamesSize = kNumPnames * sizeof(pnames[0]);

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPNamesSize + kResultsSize);
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(
      sizeof(cmds::GetError::Result));

  const uint32 kPnamesOffset = mem1.offset;
  const uint32 kResultsOffset = mem1.offset + kPNamesSize;
  Cmds expected;
  expected.get_multiple.Init(
      mem1.id, kPnamesOffset, kNumPnames,
      mem1.id, kResultsOffset, kResultsSize);
  expected.set_token.Init(GetNextToken());

  const GLint kSentinel = 0x12345678;
  memset(results, 0, sizeof(results));
  results[num_results] = kSentinel;
  const GLint returned_results[] = {
    1, 0, 1, 0, 1, -1,
  };
  // One call to flush to wait for results
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemoryFromArray(mem1.ptr + kPNamesSize,
                                   returned_results, sizeof(returned_results)))
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(0, memcmp(&returned_results, results, sizeof(returned_results)));
  EXPECT_EQ(kSentinel, results[num_results]);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetMultipleIntegervCHROMIUMBadArgs) {
  GLenum pnames[] = {
    GL_DEPTH_WRITEMASK,
    GL_COLOR_WRITEMASK,
    GL_STENCIL_WRITEMASK,
  };
  const GLint num_results = 6;
  GLint results[num_results + 1];
  const GLsizei kNumPnames = arraysize(pnames);
  const GLsizeiptr kResultsSize = num_results * sizeof(results[0]);

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  const GLint kSentinel = 0x12345678;
  memset(results, 0, sizeof(results));
  results[num_results] = kSentinel;
  // try bad size.
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize + 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
  // try bad size.
  ClearCommands();
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize - 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
  // try uncleared results.
  ClearCommands();
  results[2] = 1;
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
  // try bad enum results.
  ClearCommands();
  results[2] = 0;
  pnames[1] = GL_TRUE;
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
}

TEST_F(GLES2ImplementationTest, GetProgramInfoCHROMIUMGoodArgs) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kProgramId = 123;
  const char kBad = 0x12;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  memset(buf, kBad, sizeof(buf));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetProgramInfoCHROMIUM get_program_info;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_program_info.Init(kProgramId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetProgramInfoCHROMIUM(kProgramId, sizeof(buf), &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
  EXPECT_EQ(sizeof(kString), static_cast<size_t>(size));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

TEST_F(GLES2ImplementationTest, GetProgramInfoCHROMIUMBadArgs) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kProgramId = 123;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32(sizeof(kString))),
                      SetMemory(mem1.ptr,  kString)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  // try bufsize not big enough.
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetProgramInfoCHROMIUM get_program_info;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_program_info.Init(kProgramId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetProgramInfoCHROMIUM(kProgramId, 6, &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), gl_->GetError());
  ClearCommands();

  // try bad bufsize
  gl_->GetProgramInfoCHROMIUM(kProgramId, -1, &size, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  ClearCommands();
  // try no size ptr.
  gl_->GetProgramInfoCHROMIUM(kProgramId, sizeof(buf), NULL, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

// Test that things are cached
TEST_F(GLES2ImplementationTest, GetIntegerCacheRead) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  const PNameValue pairs[] = {
    { GL_ACTIVE_TEXTURE, GL_TEXTURE0, },
    { GL_TEXTURE_BINDING_2D, 0, },
    { GL_TEXTURE_BINDING_CUBE_MAP, 0, },
    { GL_FRAMEBUFFER_BINDING, 0, },
    { GL_RENDERBUFFER_BINDING, 0, },
    { GL_ARRAY_BUFFER_BINDING, 0, },
    { GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, },
    { GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, kMaxCombinedTextureImageUnits, },
    { GL_MAX_CUBE_MAP_TEXTURE_SIZE, kMaxCubeMapTextureSize, },
    { GL_MAX_FRAGMENT_UNIFORM_VECTORS, kMaxFragmentUniformVectors, },
    { GL_MAX_RENDERBUFFER_SIZE, kMaxRenderbufferSize, },
    { GL_MAX_TEXTURE_IMAGE_UNITS, kMaxTextureImageUnits, },
    { GL_MAX_TEXTURE_SIZE, kMaxTextureSize, },
    { GL_MAX_VARYING_VECTORS, kMaxVaryingVectors, },
    { GL_MAX_VERTEX_ATTRIBS, kMaxVertexAttribs, },
    { GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, kMaxVertexTextureImageUnits, },
    { GL_MAX_VERTEX_UNIFORM_VECTORS, kMaxVertexUniformVectors, },
    { GL_NUM_COMPRESSED_TEXTURE_FORMATS, kNumCompressedTextureFormats, },
    { GL_NUM_SHADER_BINARY_FORMATS, kNumShaderBinaryFormats, },
  };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_TRUE(NoCommandsWritten());
    EXPECT_EQ(pv.expected, v);
  }

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetIntegerCacheWrite) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  gl_->ActiveTexture(GL_TEXTURE4);
  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 3);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 4);
  gl_->BindRenderbuffer(GL_RENDERBUFFER, 5);
  gl_->BindTexture(GL_TEXTURE_2D, 6);
  gl_->BindTexture(GL_TEXTURE_CUBE_MAP, 7);

  const PNameValue pairs[] = {
    { GL_ACTIVE_TEXTURE, GL_TEXTURE4, },
    { GL_ARRAY_BUFFER_BINDING, 2, },
    { GL_ELEMENT_ARRAY_BUFFER_BINDING, 3, },
    { GL_FRAMEBUFFER_BINDING, 4, },
    { GL_RENDERBUFFER_BINDING, 5, },
    { GL_TEXTURE_BINDING_2D, 6, },
    { GL_TEXTURE_BINDING_CUBE_MAP, 7, },
  };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_EQ(pv.expected, v);
  }

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

static bool CheckRect(
    int width, int height, GLenum format, GLenum type, int alignment,
    bool flip_y, const uint8* r1, const uint8* r2) {
  uint32 size = 0;
  uint32 unpadded_row_size = 0;
  uint32 padded_row_size = 0;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, alignment, &size, &unpadded_row_size,
      &padded_row_size)) {
    return false;
  }

  int r2_stride = flip_y ?
      -static_cast<int>(padded_row_size) :
      static_cast<int>(padded_row_size);
  r2 = flip_y ? (r2 + (height - 1) * padded_row_size) : r2;

  for (int y = 0; y < height; ++y) {
    if (memcmp(r1, r2, unpadded_row_size) != 0) {
      return false;
    }
    r1 += padded_row_size;
    r2 += r2_stride;
  }
  return true;
}

ACTION_P8(CheckRectAction, width, height, format, type, alignment, flip_y,
          r1, r2) {
  EXPECT_TRUE(CheckRect(
      width, height, format, type, alignment, flip_y, r1, r2));
}

// Test TexImage2D with and without flip_y
TEST_F(GLES2ImplementationTest, TexImage2D) {
  struct Cmds {
    cmds::TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  struct Cmds2 {
    cmds::TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLsizei kWidth = 3;
  const GLsizei kHeight = 4;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  static uint8 pixels[] = {
    11, 12, 13, 13, 14, 15, 15, 16, 17, 101, 102, 103,
    21, 22, 23, 23, 24, 25, 25, 26, 27, 201, 202, 203,
    31, 32, 33, 33, 34, 35, 35, 36, 37, 123, 124, 125,
    41, 42, 43, 43, 44, 45, 45, 46, 47,
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(sizeof(pixels));

  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      mem1.id, mem1.offset);
  expected.set_token.Init(GetNextToken());
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment, false,
      pixels, mem1.ptr));

  ClearCommands();
  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);

  ExpectedMemoryInfo mem2 = GetExpectedMemory(sizeof(pixels));
  Cmds2 expected2;
  expected2.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      mem2.id, mem2.offset);
  expected2.set_token.Init(GetNextToken());
  const void* commands2 = GetPut();
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels);
  EXPECT_EQ(0, memcmp(&expected2, commands2, sizeof(expected2)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment, true,
      pixels, mem2.ptr));
}

// Test TexImage2D with 2 writes
TEST_F(GLES2ImplementationTest, TexImage2D2Writes) {
  struct Cmds {
    cmds::TexImage2D tex_image_2d;
    cmds::TexSubImage2D tex_sub_image_2d1;
    cmd::SetToken set_token1;
    cmds::TexSubImage2D tex_sub_image_2d2;
    cmd::SetToken set_token2;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;

  uint32 size = 0;
  uint32 unpadded_row_size = 0;
  uint32 padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  const GLsizei kHeight = (MaxTransferBufferSize() / padded_row_size) * 2;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, NULL, NULL));
  uint32 half_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
      &half_size, NULL, NULL));

  scoped_ptr<uint8[]> pixels(new uint8[size]);
  for (uint32 ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8>(ii);
  }

  ExpectedMemoryInfo mem1 = GetExpectedMemory(half_size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(half_size);

  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, 0, kWidth, kHeight / 2, kFormat, kType,
      mem1.id, mem1.offset, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, 0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      mem2.id, mem2.offset, true);
  expected.set_token2.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer(), OnFlush())
  //     .WillOnce(CheckRectAction(
  //         kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
  //         false, pixels.get(),
  //         GetExpectedTransferAddressFromOffsetAs<uint8>(offset1, half_size)))
  //     .RetiresOnSaturation();

  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels.get());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment, false,
      pixels.get() + kHeight / 2 * padded_row_size, mem2.ptr));

  ClearCommands();
  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  const void* commands2 = GetPut();
  ExpectedMemoryInfo mem3 = GetExpectedMemory(half_size);
  ExpectedMemoryInfo mem4 = GetExpectedMemory(half_size);
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      mem3.id, mem3.offset, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, 0, 0, kWidth, kHeight / 2, kFormat, kType,
      mem4.id, mem4.offset, true);
  expected.set_token2.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer(), OnFlush())
  //     .WillOnce(CheckRectAction(
  //         kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
  //         true, pixels.get(),
  //         GetExpectedTransferAddressFromOffsetAs<uint8>(offset3, half_size)))
  //     .RetiresOnSaturation();

  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels.get());
  EXPECT_EQ(0, memcmp(&expected, commands2, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment, true,
      pixels.get() + kHeight / 2 * padded_row_size, mem4.ptr));
}

// Test TexSubImage2D with GL_PACK_FLIP_Y set and partial multirow transfers
TEST_F(GLES2ImplementationTest, TexSubImage2DFlipY) {
  const GLsizei kTextureWidth = MaxTransferBufferSize() / 4;
  const GLsizei kTextureHeight = 7;
  const GLsizei kSubImageWidth = MaxTransferBufferSize() / 8;
  const GLsizei kSubImageHeight = 4;
  const GLint kSubImageXOffset = 1;
  const GLint kSubImageYOffset = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLint kBorder = 0;
  const GLint kPixelStoreUnpackAlignment = 4;

  struct Cmds {
    cmds::PixelStorei pixel_store_i1;
    cmds::TexImage2D tex_image_2d;
    cmds::PixelStorei pixel_store_i2;
    cmds::TexSubImage2D tex_sub_image_2d1;
    cmd::SetToken set_token1;
    cmds::TexSubImage2D tex_sub_image_2d2;
    cmd::SetToken set_token2;
  };

  uint32 sub_2_high_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kSubImageWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment,
      &sub_2_high_size, NULL, NULL));

  ExpectedMemoryInfo mem1 = GetExpectedMemory(sub_2_high_size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(sub_2_high_size);

  Cmds expected;
  expected.pixel_store_i1.Init(GL_UNPACK_ALIGNMENT, kPixelStoreUnpackAlignment);
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kTextureWidth, kTextureHeight, kBorder, kFormat,
      kType, 0, 0);
  expected.pixel_store_i2.Init(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  expected.tex_sub_image_2d1.Init(kTarget, kLevel, kSubImageXOffset,
      kSubImageYOffset + 2, kSubImageWidth, 2, kFormat, kType,
      mem1.id, mem1.offset, false);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(kTarget, kLevel, kSubImageXOffset,
      kSubImageYOffset, kSubImageWidth , 2, kFormat, kType,
      mem2.id, mem2.offset, false);
  expected.set_token2.Init(GetNextToken());

  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, kPixelStoreUnpackAlignment);
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kTextureWidth, kTextureHeight, kBorder, kFormat,
      kType, NULL);
  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  scoped_ptr<uint32[]> pixels(new uint32[kSubImageWidth * kSubImageHeight]);
  for (int y = 0; y < kSubImageHeight; ++y) {
    for (int x = 0; x < kSubImageWidth; ++x) {
      pixels.get()[kSubImageWidth * y + x] = x | (y << 16);
    }
  }
  gl_->TexSubImage2D(
      GL_TEXTURE_2D, 0, kSubImageXOffset, kSubImageYOffset, kSubImageWidth,
      kSubImageHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kSubImageWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment, true,
      reinterpret_cast<uint8*>(pixels.get() + 2 * kSubImageWidth),
      mem2.ptr));
}

TEST_F(GLES2ImplementationTest, SubImageUnpack) {
  static const GLint unpack_alignments[] = { 1, 2, 4, 8 };

  static const GLenum kFormat = GL_RGB;
  static const GLenum kType = GL_UNSIGNED_BYTE;
  static const GLint kLevel = 0;
  static const GLint kBorder = 0;
  // We're testing using the unpack params to pull a subimage out of a larger
  // source of pixels. Here we specify the subimage by its border rows /
  // columns.
  static const GLint kSrcWidth = 33;
  static const GLint kSrcSubImageX0 = 11;
  static const GLint kSrcSubImageX1 = 20;
  static const GLint kSrcSubImageY0 = 18;
  static const GLint kSrcSubImageY1 = 23;
  static const GLint kSrcSubImageWidth = kSrcSubImageX1 - kSrcSubImageX0;
  static const GLint kSrcSubImageHeight = kSrcSubImageY1 - kSrcSubImageY0;

  // these are only used in the texsubimage tests
  static const GLint kTexWidth = 1023;
  static const GLint kTexHeight = 511;
  static const GLint kTexSubXOffset = 419;
  static const GLint kTexSubYOffset = 103;

  struct {
    cmds::PixelStorei pixel_store_i;
    cmds::PixelStorei pixel_store_i2;
    cmds::TexImage2D tex_image_2d;
  } texImageExpected;

  struct  {
    cmds::PixelStorei pixel_store_i;
    cmds::PixelStorei pixel_store_i2;
    cmds::TexImage2D tex_image_2d;
    cmds::TexSubImage2D tex_sub_image_2d;
  } texSubImageExpected;

  uint32 src_size;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kSrcWidth, kSrcSubImageY1, kFormat, kType, 8, &src_size, NULL, NULL));
  scoped_ptr<uint8[]> src_pixels;
  src_pixels.reset(new uint8[src_size]);
  for (size_t i = 0; i < src_size; ++i) {
    src_pixels[i] = static_cast<int8>(i);
  }

  for (int sub = 0; sub < 2; ++sub) {
    for (int flip_y = 0; flip_y < 2; ++flip_y) {
      for (size_t a = 0; a < arraysize(unpack_alignments); ++a) {
        GLint alignment = unpack_alignments[a];
        uint32 size;
        uint32 unpadded_row_size;
        uint32 padded_row_size;
        ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
            kSrcSubImageWidth, kSrcSubImageHeight, kFormat, kType, alignment,
            &size, &unpadded_row_size, &padded_row_size));
        ASSERT_TRUE(size <= MaxTransferBufferSize());
        ExpectedMemoryInfo mem = GetExpectedMemory(size);

        const void* commands = GetPut();
        gl_->PixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        gl_->PixelStorei(GL_UNPACK_ROW_LENGTH, kSrcWidth);
        gl_->PixelStorei(GL_UNPACK_SKIP_PIXELS, kSrcSubImageX0);
        gl_->PixelStorei(GL_UNPACK_SKIP_ROWS, kSrcSubImageY0);
        gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
        if (sub) {
          gl_->TexImage2D(
              GL_TEXTURE_2D, kLevel, kFormat, kTexWidth, kTexHeight, kBorder,
              kFormat, kType, NULL);
          gl_->TexSubImage2D(
              GL_TEXTURE_2D, kLevel, kTexSubXOffset, kTexSubYOffset,
              kSrcSubImageWidth, kSrcSubImageHeight, kFormat, kType,
              src_pixels.get());
          texSubImageExpected.pixel_store_i.Init(
              GL_UNPACK_ALIGNMENT, alignment);
          texSubImageExpected.pixel_store_i2.Init(
              GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
          texSubImageExpected.tex_image_2d.Init(
              GL_TEXTURE_2D, kLevel, kFormat, kTexWidth, kTexHeight, kBorder,
              kFormat, kType, 0, 0);
          texSubImageExpected.tex_sub_image_2d.Init(
              GL_TEXTURE_2D, kLevel, kTexSubXOffset, kTexSubYOffset,
              kSrcSubImageWidth, kSrcSubImageHeight, kFormat, kType, mem.id,
              mem.offset, GL_FALSE);
          EXPECT_EQ(0, memcmp(
              &texSubImageExpected, commands, sizeof(texSubImageExpected)));
        } else {
          gl_->TexImage2D(
              GL_TEXTURE_2D, kLevel, kFormat,
              kSrcSubImageWidth, kSrcSubImageHeight, kBorder, kFormat, kType,
              src_pixels.get());
          texImageExpected.pixel_store_i.Init(GL_UNPACK_ALIGNMENT, alignment);
          texImageExpected.pixel_store_i2.Init(
              GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
          texImageExpected.tex_image_2d.Init(
              GL_TEXTURE_2D, kLevel, kFormat, kSrcSubImageWidth,
              kSrcSubImageHeight, kBorder, kFormat, kType, mem.id, mem.offset);
          EXPECT_EQ(0, memcmp(
              &texImageExpected, commands, sizeof(texImageExpected)));
        }
        uint32 src_padded_row_size;
        ASSERT_TRUE(GLES2Util::ComputeImagePaddedRowSize(
            kSrcWidth, kFormat, kType, alignment, &src_padded_row_size));
        uint32 bytes_per_group = GLES2Util::ComputeImageGroupSize(
            kFormat, kType);
        for (int y = 0; y < kSrcSubImageHeight; ++y) {
          GLint src_sub_y = flip_y ? kSrcSubImageHeight - y - 1 : y;
          const uint8* src_row = src_pixels.get() +
              (kSrcSubImageY0 + src_sub_y) * src_padded_row_size +
              bytes_per_group * kSrcSubImageX0;
          const uint8* dst_row = mem.ptr + y * padded_row_size;
          EXPECT_EQ(0, memcmp(src_row, dst_row, unpadded_row_size));
        }
        ClearCommands();
      }
    }
  }
}

// Binds can not be cached with bind_generates_resource = false because
// our id might not be valid. More specifically if you bind on contextA then
// delete on contextB the resource is still bound on contextA but GetInterger
// won't return an id.
TEST_F(GLES2ImplementationStrictSharedTest, BindsNotCached) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  const PNameValue pairs[] = {
    { GL_TEXTURE_BINDING_2D, 1, },
    { GL_TEXTURE_BINDING_CUBE_MAP, 2, },
    { GL_FRAMEBUFFER_BINDING, 3, },
    { GL_RENDERBUFFER_BINDING, 4, },
    { GL_ARRAY_BUFFER_BINDING, 5, },
    { GL_ELEMENT_ARRAY_BUFFER_BINDING, 6, },
  };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    ExpectedMemoryInfo result1 =
        GetExpectedResultMemory(sizeof(cmds::GetIntegerv::Result));
    EXPECT_CALL(*command_buffer(), OnFlush())
        .WillOnce(SetMemory(result1.ptr,
                            SizedResultHelper<GLuint>(pv.expected)))
        .RetiresOnSaturation();
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_EQ(pv.expected, v);
  }
}

TEST_F(GLES2ImplementationTest, CreateStreamTextureCHROMIUM) {
  const GLuint kTextureId = 123;
  const GLuint kResult = 456;

  struct Cmds {
    cmds::CreateStreamTextureCHROMIUM create_stream;
  };

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(
          sizeof(cmds::CreateStreamTextureCHROMIUM::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  Cmds expected;
  expected.create_stream.Init(kTextureId, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, kResult))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  GLuint handle = gl_->CreateStreamTextureCHROMIUM(kTextureId);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(handle, kResult);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetString) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const Str7 kString = {"foobar"};
  // GL_CHROMIUM_map_sub GL_CHROMIUM_flipy are hard coded into
  // GLES2Implementation.
  const char* expected_str =
      "foobar "
      "GL_CHROMIUM_flipy "
      "GL_CHROMIUM_map_sub "
      "GL_CHROMIUM_shallow_flush "
      "GL_EXT_unpack_subimage "
      "GL_CHROMIUM_map_image";
  const char kBad = 0x12;
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetString get_string;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_string.Init(GL_EXTENSIONS, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  char buf[sizeof(kString) + 1];
  memset(buf, kBad, sizeof(buf));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .RetiresOnSaturation();

  const GLubyte* result = gl_->GetString(GL_EXTENSIONS);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_STREQ(expected_str, reinterpret_cast<const char*>(result));
}

TEST_F(GLES2ImplementationTest, PixelStoreiGLPackReverseRowOrderANGLE) {
  const uint32 kBucketId = GLES2Implementation::kResultBucketId;
  const Str7 kString = {"foobar"};
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetString get_string;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
    cmds::PixelStorei pixel_store;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));

  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_string.Init(GL_EXTENSIONS, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  expected.pixel_store.Init(GL_PACK_REVERSE_ROW_ORDER_ANGLE, 1);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .RetiresOnSaturation();

  gl_->PixelStorei(GL_PACK_REVERSE_ROW_ORDER_ANGLE, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CreateProgram) {
  struct Cmds {
    cmds::CreateProgram cmd;
  };

  Cmds expected;
  expected.cmd.Init(kProgramsAndShadersStartId);
  GLuint id = gl_->CreateProgram();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kProgramsAndShadersStartId, id);
}

TEST_F(GLES2ImplementationTest, BufferDataLargerThanTransferBuffer) {
  struct Cmds {
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
  };
  const unsigned kUsableSize =
      kTransferBufferSize - GLES2Implementation::kStartingOffset;
  uint8 buf[kUsableSize * 2] = { 0, };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kUsableSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kUsableSize);

  Cmds expected;
  expected.set_size.Init(
      GL_ARRAY_BUFFER, arraysize(buf), 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, 0, kUsableSize, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kUsableSize, kUsableSize, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  gl_->BufferData(GL_ARRAY_BUFFER, arraysize(buf), buf, GL_DYNAMIC_DRAW);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CapabilitiesAreCached) {
  static const GLenum kStates[] = {
    GL_DITHER,
    GL_BLEND,
    GL_CULL_FACE,
    GL_DEPTH_TEST,
    GL_POLYGON_OFFSET_FILL,
    GL_SAMPLE_ALPHA_TO_COVERAGE,
    GL_SAMPLE_COVERAGE,
    GL_SCISSOR_TEST,
    GL_STENCIL_TEST,
  };
  struct Cmds {
    cmds::Enable enable_cmd;
  };
  Cmds expected;

  for (size_t ii = 0; ii < arraysize(kStates); ++ii) {
    GLenum state = kStates[ii];
    expected.enable_cmd.Init(state);
    GLboolean result = gl_->IsEnabled(state);
    EXPECT_EQ(static_cast<GLboolean>(ii == 0), result);
    EXPECT_TRUE(NoCommandsWritten());
    const void* commands = GetPut();
    if (!result) {
      gl_->Enable(state);
      EXPECT_EQ(0, memcmp(&expected, commands, sizeof(expected)));
    }
    ClearCommands();
    result = gl_->IsEnabled(state);
    EXPECT_TRUE(result);
    EXPECT_TRUE(NoCommandsWritten());
  }
}

TEST_F(GLES2ImplementationTest, BindVertexArrayOES) {
  GLuint id = 0;
  gl_->GenVertexArraysOES(1, &id);
  ClearCommands();

  struct Cmds {
    cmds::BindVertexArrayOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(id);

  const void* commands = GetPut();
  gl_->BindVertexArrayOES(id);
  EXPECT_EQ(0, memcmp(&expected, commands, sizeof(expected)));
  ClearCommands();
  gl_->BindVertexArrayOES(id);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BeginEndQueryEXT) {
  // Test GetQueryivEXT returns 0 if no current query.
  GLint param = -1;
  gl_->GetQueryivEXT(GL_ANY_SAMPLES_PASSED_EXT, GL_CURRENT_QUERY_EXT, &param);
  EXPECT_EQ(0, param);

  GLuint expected_ids[2] = { 1, 2 }; // These must match what's actually genned.
  struct GenCmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  GenCmds expected_gen_cmds;
  expected_gen_cmds.gen.Init(arraysize(expected_ids), &expected_ids[0]);
  GLuint ids[arraysize(expected_ids)] = { 0, };
  gl_->GenQueriesEXT(arraysize(expected_ids), &ids[0]);
  EXPECT_EQ(0, memcmp(
      &expected_gen_cmds, commands_, sizeof(expected_gen_cmds)));
  GLuint id1 = ids[0];
  GLuint id2 = ids[1];
  ClearCommands();

  // Test BeginQueryEXT fails if id = 0.
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, 0);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test BeginQueryEXT fails if id not GENed.
  // TODO(gman):

  // Test BeginQueryEXT inserts command.
  struct BeginCmds {
    cmds::BeginQueryEXT begin_query;
  };
  BeginCmds expected_begin_cmds;
  const void* commands = GetPut();
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, id1);
  QueryTracker::Query* query = GetQuery(id1);
  ASSERT_TRUE(query != NULL);
  expected_begin_cmds.begin_query.Init(
      GL_ANY_SAMPLES_PASSED_EXT, id1, query->shm_id(), query->shm_offset());
  EXPECT_EQ(0, memcmp(
      &expected_begin_cmds, commands, sizeof(expected_begin_cmds)));
  ClearCommands();

  // Test GetQueryivEXT returns id.
  param = -1;
  gl_->GetQueryivEXT(GL_ANY_SAMPLES_PASSED_EXT, GL_CURRENT_QUERY_EXT, &param);
  EXPECT_EQ(id1, static_cast<GLuint>(param));
  gl_->GetQueryivEXT(
      GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, GL_CURRENT_QUERY_EXT, &param);
  EXPECT_EQ(0, param);

  // Test BeginQueryEXT fails if between Begin/End.
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, id2);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test EndQueryEXT fails if target not same as current query.
  ClearCommands();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test EndQueryEXT sends command
  struct EndCmds {
    cmds::EndQueryEXT end_query;
  };
  EndCmds expected_end_cmds;
  expected_end_cmds.end_query.Init(
      GL_ANY_SAMPLES_PASSED_EXT, query->submit_count());
  commands = GetPut();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  EXPECT_EQ(0, memcmp(
      &expected_end_cmds, commands, sizeof(expected_end_cmds)));

  // Test EndQueryEXT fails if no current query.
  ClearCommands();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test 2nd Begin/End increments count.
  uint32 old_submit_count = query->submit_count();
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, id1);
  EXPECT_NE(old_submit_count, query->submit_count());
  expected_end_cmds.end_query.Init(
      GL_ANY_SAMPLES_PASSED_EXT, query->submit_count());
  commands = GetPut();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  EXPECT_EQ(0, memcmp(
      &expected_end_cmds, commands, sizeof(expected_end_cmds)));

  // Test BeginQueryEXT fails if target changed.
  ClearCommands();
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, id1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT fails if unused id
  GLuint available = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id2, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT fails if bad id
  ClearCommands();
  gl_->GetQueryObjectuivEXT(4567, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT CheckResultsAvailable
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id1, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0u, available);
}

TEST_F(GLES2ImplementationTest, ErrorQuery) {
  GLuint id = 0;
  gl_->GenQueriesEXT(1, &id);
  ClearCommands();

  // Test BeginQueryEXT does NOT insert commands.
  gl_->BeginQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM, id);
  EXPECT_TRUE(NoCommandsWritten());
  QueryTracker::Query* query = GetQuery(id);
  ASSERT_TRUE(query != NULL);

  // Test EndQueryEXT sends both begin and end command
  struct EndCmds {
    cmds::BeginQueryEXT begin_query;
    cmds::EndQueryEXT end_query;
  };
  EndCmds expected_end_cmds;
  expected_end_cmds.begin_query.Init(
      GL_GET_ERROR_QUERY_CHROMIUM, id, query->shm_id(), query->shm_offset());
  expected_end_cmds.end_query.Init(
      GL_GET_ERROR_QUERY_CHROMIUM, query->submit_count());
  const void* commands = GetPut();
  gl_->EndQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM);
  EXPECT_EQ(0, memcmp(
      &expected_end_cmds, commands, sizeof(expected_end_cmds)));
  ClearCommands();

  // Check result is not yet available.
  GLuint available = 0xBDu;
  gl_->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0u, available);

  // Test no commands are sent if there is a client side error.

  // Generate a client side error
  gl_->ActiveTexture(GL_TEXTURE0 - 1);

  gl_->BeginQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM, id);
  gl_->EndQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM);
  EXPECT_TRUE(NoCommandsWritten());

  // Check result is available.
  gl_->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_NE(0u, available);

  // Check result.
  GLuint result = 0xBDu;
  gl_->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_EXT, &result);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLuint>(GL_INVALID_ENUM), result);
}

#if !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
TEST_F(GLES2ImplementationTest, VertexArrays) {
  const GLuint kAttribIndex1 = 1;
  const GLint kNumComponents1 = 3;
  const GLsizei kClientStride = 12;

  GLuint id = 0;
  gl_->GenVertexArraysOES(1, &id);
  ClearCommands();

  gl_->BindVertexArrayOES(id);

  // Test that VertexAttribPointer cannot be called with a bound buffer of 0
  // unless the offset is NULL
  gl_->BindBuffer(GL_ARRAY_BUFFER, 0);

  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride,
      reinterpret_cast<const void*>(4));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride, NULL);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
}
#endif

TEST_F(GLES2ImplementationTest, Disable) {
  struct Cmds {
    cmds::Disable cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_DITHER);  // Note: DITHER defaults to enabled.

  gl_->Disable(GL_DITHER);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  // Check it's cached and not called again.
  ClearCommands();
  gl_->Disable(GL_DITHER);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, Enable) {
  struct Cmds {
    cmds::Enable cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_BLEND);  // Note: BLEND defaults to disabled.

  gl_->Enable(GL_BLEND);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  // Check it's cached and not called again.
  ClearCommands();
  gl_->Enable(GL_BLEND);
  EXPECT_TRUE(NoCommandsWritten());
}


#include "gpu/command_buffer/client/gles2_implementation_unittest_autogen.h"

}  // namespace gles2
}  // namespace gpu
