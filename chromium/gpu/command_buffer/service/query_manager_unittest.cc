// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace gpu {
namespace gles2 {

class QueryManagerTest : public testing::Test {
 public:
  static const int32 kSharedMemoryId = 401;
  static const size_t kSharedBufferSize = 2048;
  static const uint32 kSharedMemoryOffset = 132;
  static const int32 kInvalidSharedMemoryId = 402;
  static const uint32 kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const uint32 kInitialResult = 0xBDBDBDBDu;
  static const uint8 kInitialMemoryValue = 0xBDu;

  QueryManagerTest() {
  }
  virtual ~QueryManagerTest() {
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    engine_.reset(new MockCommandBufferEngine());
    decoder_.reset(new MockGLES2Decoder());
    decoder_->set_engine(engine_.get());
    TestHelper::SetupFeatureInfoInitExpectations(
        gl_.get(),
        "GL_EXT_occlusion_query_boolean");
    scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
    feature_info->Initialize();
    manager_.reset(new QueryManager(decoder_.get(), feature_info.get()));
  }

  virtual void TearDown() {
    decoder_.reset();
    manager_->Destroy(false);
    manager_.reset();
    engine_.reset();
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  QueryManager::Query* CreateQuery(
      GLenum target, GLuint client_id, int32 shm_id, uint32 shm_offset,
      GLuint service_id) {
    EXPECT_CALL(*gl_, GenQueriesARB(1, _))
       .WillOnce(SetArgumentPointee<1>(service_id))
       .RetiresOnSaturation();
    return manager_->CreateQuery(target, client_id, shm_id, shm_offset);
  }

  void QueueQuery(
    QueryManager::Query* query, GLuint service_id, uint32 submit_count) {
    EXPECT_CALL(*gl_, BeginQueryARB(query->target(), service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, EndQueryARB(query->target()))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_TRUE(manager_->BeginQuery(query));
    EXPECT_TRUE(manager_->EndQuery(query, submit_count));
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_ptr<MockGLES2Decoder> decoder_;
  scoped_ptr<QueryManager> manager_;

 private:
  class MockCommandBufferEngine : public CommandBufferEngine {
   public:
    MockCommandBufferEngine() {
      data_.reset(new int8[kSharedBufferSize]);
      ClearSharedMemory();
      valid_buffer_.ptr = data_.get();
      valid_buffer_.size = kSharedBufferSize;
    }

    virtual ~MockCommandBufferEngine() {
    }

    virtual gpu::Buffer GetSharedMemoryBuffer(int32 shm_id) OVERRIDE {
      return shm_id == kSharedMemoryId ? valid_buffer_ : invalid_buffer_;
    }

    void ClearSharedMemory() {
      memset(data_.get(), kInitialMemoryValue, kSharedBufferSize);
    }

    virtual void set_token(int32 token) OVERRIDE {
      DCHECK(false);
    }

    virtual bool SetGetBuffer(int32 /* transfer_buffer_id */) OVERRIDE {
      DCHECK(false);
      return false;
    }

    // Overridden from CommandBufferEngine.
    virtual bool SetGetOffset(int32 offset) OVERRIDE {
      DCHECK(false);
      return false;
    }

    // Overridden from CommandBufferEngine.
    virtual int32 GetGetOffset() OVERRIDE {
      DCHECK(false);
      return 0;
    }

   private:
    scoped_ptr<int8[]> data_;
    gpu::Buffer valid_buffer_;
    gpu::Buffer invalid_buffer_;
  };

  scoped_ptr<MockCommandBufferEngine> engine_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const int32 QueryManagerTest::kSharedMemoryId;
const size_t QueryManagerTest::kSharedBufferSize;
const uint32 QueryManagerTest::kSharedMemoryOffset;
const int32 QueryManagerTest::kInvalidSharedMemoryId;
const uint32 QueryManagerTest::kInvalidSharedMemoryOffset;
const uint32 QueryManagerTest::kInitialResult;
const uint8 QueryManagerTest::kInitialMemoryValue;
#endif

TEST_F(QueryManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;

  EXPECT_FALSE(manager_->HavePendingQueries());
  // Check we can create a Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(GL_ANY_SAMPLES_PASSED_EXT, kClient1Id,
                  kSharedMemoryId, kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  // Check we can get the same Query.
  EXPECT_EQ(query.get(), manager_->GetQuery(kClient1Id));
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient2Id) == NULL);
  // Check we can delete the query.
  manager_->RemoveQuery(kClient1Id);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient1Id) == NULL);
  // Check query is deleted
  EXPECT_TRUE(query->IsDeleted());
  EXPECT_FALSE(manager_->HavePendingQueries());
}

TEST_F(QueryManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(GL_ANY_SAMPLES_PASSED_EXT, kClient1Id,
                  kSharedMemoryId, kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  EXPECT_CALL(*gl_, DeleteQueriesARB(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_->Destroy(true);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient1Id) == NULL);
  // Check query is deleted
  EXPECT_TRUE(query->IsDeleted());
}

TEST_F(QueryManagerTest, QueryBasic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id,
                  kSharedMemoryId, kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  EXPECT_TRUE(query->IsValid());
  EXPECT_FALSE(query->IsDeleted());
  EXPECT_FALSE(query->pending());
  EXPECT_EQ(kTarget, query->target());
  EXPECT_EQ(kSharedMemoryId, query->shm_id());
  EXPECT_EQ(kSharedMemoryOffset, query->shm_offset());
}

TEST_F(QueryManagerTest, ProcessPendingQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;
  const GLuint kResult = 1;

  // Check nothing happens if there are no pending queries.
  EXPECT_TRUE(manager_->ProcessPendingQueries());

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id,
                  kSharedMemoryId, kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  // Setup shared memory like client would.
  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      kSharedMemoryId, kSharedMemoryOffset, sizeof(*sync));
  ASSERT_TRUE(sync != NULL);
  sync->Reset();

  // Queue it
  QueueQuery(query.get(), kService1Id, kSubmitCount);
  EXPECT_TRUE(query->pending());
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with return not available.
  // Expect 1 GL command.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries());
  EXPECT_TRUE(query->pending());
  EXPECT_EQ(0u, sync->process_count);
  EXPECT_EQ(0u, sync->result);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries());
  EXPECT_FALSE(query->pending());
  EXPECT_EQ(kSubmitCount, sync->process_count);
  EXPECT_EQ(kResult, sync->result);
  EXPECT_FALSE(manager_->HavePendingQueries());

  // Process with no queries.
  // Expect no GL commands/
  EXPECT_TRUE(manager_->ProcessPendingQueries());
}

TEST_F(QueryManagerTest, ProcessPendingQueries) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  const GLuint kService2Id = 12;
  const GLuint kClient3Id = 3;
  const GLuint kService3Id = 13;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount1 = 123;
  const uint32 kSubmitCount2 = 123;
  const uint32 kSubmitCount3 = 123;
  const GLuint kResult1 = 1;
  const GLuint kResult2 = 1;
  const GLuint kResult3 = 1;

  // Setup shared memory like client would.
  QuerySync* sync1 = decoder_->GetSharedMemoryAs<QuerySync*>(
      kSharedMemoryId, kSharedMemoryOffset, sizeof(*sync1) * 3);
  ASSERT_TRUE(sync1 != NULL);
  QuerySync* sync2 = sync1 + 1;
  QuerySync* sync3 = sync2 + 1;

  // Create Queries.
  scoped_refptr<QueryManager::Query> query1(
      CreateQuery(kTarget, kClient1Id,
                  kSharedMemoryId, kSharedMemoryOffset + sizeof(*sync1) * 0,
                  kService1Id));
  scoped_refptr<QueryManager::Query> query2(
      CreateQuery(kTarget, kClient2Id,
                  kSharedMemoryId, kSharedMemoryOffset + sizeof(*sync1) * 1,
                  kService2Id));
  scoped_refptr<QueryManager::Query> query3(
      CreateQuery(kTarget, kClient3Id,
                  kSharedMemoryId, kSharedMemoryOffset + sizeof(*sync1) * 2,
                  kService3Id));
  ASSERT_TRUE(query1.get() != NULL);
  ASSERT_TRUE(query2.get() != NULL);
  ASSERT_TRUE(query3.get() != NULL);
  EXPECT_FALSE(manager_->HavePendingQueries());

  sync1->Reset();
  sync2->Reset();
  sync3->Reset();

  // Queue them
  QueueQuery(query1.get(), kService1Id, kSubmitCount1);
  QueueQuery(query2.get(), kService2Id, kSubmitCount2);
  QueueQuery(query3.get(), kService3Id, kSubmitCount3);
  EXPECT_TRUE(query1->pending());
  EXPECT_TRUE(query2->pending());
  EXPECT_TRUE(query3->pending());
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with return available for first 2 queries.
  // Expect 4 GL commands.
  {
    InSequence s;
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgumentPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgumentPointee<2>(kResult1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService2Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgumentPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService2Id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgumentPointee<2>(kResult2))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgumentPointee<2>(0))
        .RetiresOnSaturation();
    EXPECT_TRUE(manager_->ProcessPendingQueries());
  }
  EXPECT_FALSE(query1->pending());
  EXPECT_FALSE(query2->pending());
  EXPECT_TRUE(query3->pending());
  EXPECT_EQ(kSubmitCount1, sync1->process_count);
  EXPECT_EQ(kSubmitCount2, sync2->process_count);
  EXPECT_EQ(kResult1, sync1->result);
  EXPECT_EQ(kResult2, sync2->result);
  EXPECT_EQ(0u, sync3->process_count);
  EXPECT_EQ(0u, sync3->result);
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with renaming query. No result.
  // Expect 1 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries());
  EXPECT_TRUE(query3->pending());
  EXPECT_EQ(0u, sync3->process_count);
  EXPECT_EQ(0u, sync3->result);
  EXPECT_TRUE(manager_->HavePendingQueries());

  // Process with renaming query. With result.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult3))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries());
  EXPECT_FALSE(query3->pending());
  EXPECT_EQ(kSubmitCount3, sync3->process_count);
  EXPECT_EQ(kResult3, sync3->result);
  EXPECT_FALSE(manager_->HavePendingQueries());
}

TEST_F(QueryManagerTest, ProcessPendingBadSharedMemoryId) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;
  const GLuint kResult = 1;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id,
                  kInvalidSharedMemoryId, kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  // Queue it
  QueueQuery(query.get(), kService1Id, kSubmitCount);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult))
      .RetiresOnSaturation();
  EXPECT_FALSE(manager_->ProcessPendingQueries());
}

TEST_F(QueryManagerTest, ProcessPendingBadSharedMemoryOffset) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;
  const GLuint kResult = 1;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id,
                  kSharedMemoryId, kInvalidSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  // Queue it
  QueueQuery(query.get(), kService1Id, kSubmitCount);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult))
      .RetiresOnSaturation();
  EXPECT_FALSE(manager_->ProcessPendingQueries());
}

TEST_F(QueryManagerTest, ExitWithPendingQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;

  // Create Query.
  scoped_refptr<QueryManager::Query> query(
      CreateQuery(kTarget, kClient1Id,
                  kSharedMemoryId, kSharedMemoryOffset, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  // Queue it
  QueueQuery(query.get(), kService1Id, kSubmitCount);
}

// Test that when based on ARB_occlusion_query2 we use GL_ANY_SAMPLES_PASSED_ARB
// for GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT
TEST_F(QueryManagerTest, ARBOcclusionQuery2) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT;
  const uint32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(),
      "GL_ARB_occlusion_query2");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize();
  scoped_ptr<QueryManager> manager(
      new QueryManager(decoder_.get(), feature_info.get()));

  EXPECT_CALL(*gl_, GenQueriesARB(1, _))
     .WillOnce(SetArgumentPointee<1>(kService1Id))
     .RetiresOnSaturation();
  QueryManager::Query* query = manager->CreateQuery(
      kTarget, kClient1Id, kSharedMemoryId, kSharedMemoryOffset);
  ASSERT_TRUE(query != NULL);

  EXPECT_CALL(*gl_, BeginQueryARB(GL_ANY_SAMPLES_PASSED_EXT, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EndQueryARB(GL_ANY_SAMPLES_PASSED_EXT))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(manager->BeginQuery(query));
  EXPECT_TRUE(manager->EndQuery(query, kSubmitCount));
  manager->Destroy(false);
}

// Test that when based on ARB_occlusion_query we use GL_SAMPLES_PASSED_ARB
// for GL_ANY_SAMPLES_PASSED_EXT
TEST_F(QueryManagerTest, ARBOcclusionQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(),
      "GL_ARB_occlusion_query");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize();
  scoped_ptr<QueryManager> manager(
      new QueryManager(decoder_.get(), feature_info.get()));

  EXPECT_CALL(*gl_, GenQueriesARB(1, _))
     .WillOnce(SetArgumentPointee<1>(kService1Id))
     .RetiresOnSaturation();
  QueryManager::Query* query = manager->CreateQuery(
      kTarget, kClient1Id, kSharedMemoryId, kSharedMemoryOffset);
  ASSERT_TRUE(query != NULL);

  EXPECT_CALL(*gl_, BeginQueryARB(GL_SAMPLES_PASSED_ARB, kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EndQueryARB(GL_SAMPLES_PASSED_ARB))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(manager->BeginQuery(query));
  EXPECT_TRUE(manager->EndQuery(query, kSubmitCount));
  manager->Destroy(false);
}

TEST_F(QueryManagerTest, GetErrorQuery) {
  const GLuint kClient1Id = 1;
  const GLenum kTarget = GL_GET_ERROR_QUERY_CHROMIUM;
  const uint32 kSubmitCount = 123;

  TestHelper::SetupFeatureInfoInitExpectations(gl_.get(), "");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize();
  scoped_ptr<QueryManager> manager(
      new QueryManager(decoder_.get(), feature_info.get()));

  QueryManager::Query* query = manager->CreateQuery(
      kTarget, kClient1Id, kSharedMemoryId, kSharedMemoryOffset);
  ASSERT_TRUE(query != NULL);

  // Setup shared memory like client would.
  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      kSharedMemoryId, kSharedMemoryOffset, sizeof(*sync));
  ASSERT_TRUE(sync != NULL);
  sync->Reset();

  EXPECT_TRUE(manager->BeginQuery(query));

  MockErrorState mock_error_state;
  EXPECT_CALL(*decoder_.get(), GetErrorState())
      .WillRepeatedly(Return(&mock_error_state));
  EXPECT_CALL(mock_error_state, GetGLError())
      .WillOnce(Return(GL_INVALID_ENUM))
      .RetiresOnSaturation();

  EXPECT_TRUE(manager->EndQuery(query, kSubmitCount));
  EXPECT_FALSE(query->pending());

  EXPECT_EQ(static_cast<GLuint>(GL_INVALID_ENUM), sync->result);

  manager->Destroy(false);
}

}  // namespace gles2
}  // namespace gpu


