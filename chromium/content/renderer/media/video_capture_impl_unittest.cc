// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace content {

class MockVideoCaptureMessageFilter : public VideoCaptureMessageFilter {
 public:
  MockVideoCaptureMessageFilter() : VideoCaptureMessageFilter() {}

  // Filter implementation.
  MOCK_METHOD1(Send, bool(IPC::Message* message));

 protected:
  virtual ~MockVideoCaptureMessageFilter() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureMessageFilter);
};

class MockVideoCaptureClient : public media::VideoCapture::EventHandler {
 public:
  MockVideoCaptureClient() {}
  virtual ~MockVideoCaptureClient() {}

  // EventHandler implementation.
  MOCK_METHOD1(OnStarted, void(media::VideoCapture* capture));
  MOCK_METHOD1(OnStopped, void(media::VideoCapture* capture));
  MOCK_METHOD1(OnPaused, void(media::VideoCapture* capture));
  MOCK_METHOD2(OnError, void(media::VideoCapture* capture, int error_code));
  MOCK_METHOD1(OnRemoved, void(media::VideoCapture* capture));
  MOCK_METHOD2(OnFrameReady,
               void(media::VideoCapture* capture,
                    const scoped_refptr<media::VideoFrame>& frame));
  MOCK_METHOD2(OnDeviceInfoReceived,
               void(media::VideoCapture* capture,
                    const media::VideoCaptureFormat& device_info));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureClient);
};

class VideoCaptureImplTest : public ::testing::Test {
 public:
  class MockVideoCaptureImpl : public VideoCaptureImpl {
   public:
    MockVideoCaptureImpl(const media::VideoCaptureSessionId id,
                         scoped_refptr<base::MessageLoopProxy> ml_proxy,
                         VideoCaptureMessageFilter* filter)
        : VideoCaptureImpl(id, ml_proxy.get(), filter) {}
    virtual ~MockVideoCaptureImpl() {}

    // Override Send() to mimic device to send events.
    virtual void Send(IPC::Message* message) OVERRIDE {
      CHECK(message);

      // In this method, messages are sent to the according handlers as if
      // we are the device.
      bool handled = true;
      IPC_BEGIN_MESSAGE_MAP(MockVideoCaptureImpl, *message)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, DeviceStartCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, DevicePauseCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, DeviceStopCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady,
                            DeviceReceiveEmptyBuffer)
        IPC_MESSAGE_UNHANDLED(handled = false)
      IPC_END_MESSAGE_MAP()
      EXPECT_TRUE(handled);
      delete message;
    }

    void DeviceStartCapture(int device_id,
                            media::VideoCaptureSessionId session_id,
                            const media::VideoCaptureParams& params) {
      OnStateChanged(VIDEO_CAPTURE_STATE_STARTED);
    }

    void DevicePauseCapture(int device_id) {}

    void DeviceStopCapture(int device_id) {
      OnStateChanged(VIDEO_CAPTURE_STATE_STOPPED);
    }

    void DeviceReceiveEmptyBuffer(int device_id, int buffer_id) {}
  };

  VideoCaptureImplTest() {
    params_small_.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);

    params_large_.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));
    message_loop_proxy_ = base::MessageLoopProxy::current().get();
    child_process_.reset(new ChildProcess());

    message_filter_ = new MockVideoCaptureMessageFilter;
    session_id_ = 1;

    video_capture_impl_ = new MockVideoCaptureImpl(
        session_id_, message_loop_proxy_, message_filter_.get());

    video_capture_impl_->device_id_ = 2;
  }

  virtual ~VideoCaptureImplTest() {
    delete video_capture_impl_;
  }

 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_ptr<ChildProcess> child_process_;
  scoped_refptr<MockVideoCaptureMessageFilter> message_filter_;
  media::VideoCaptureSessionId session_id_;
  MockVideoCaptureImpl* video_capture_impl_;
  media::VideoCaptureParams params_small_;
  media::VideoCaptureParams params_large_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplTest);
};

TEST_F(VideoCaptureImplTest, Simple) {
  // Execute SetCapture() and StopCapture() for one client.
  scoped_ptr<MockVideoCaptureClient> client(new MockVideoCaptureClient);

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), params_small_);
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunUntilIdle();
}

TEST_F(VideoCaptureImplTest, TwoClientsInSequence) {
  // Execute SetCapture() and StopCapture() for 2 clients in sequence.
  scoped_ptr<MockVideoCaptureClient> client(new MockVideoCaptureClient);

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), params_small_);
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client.get(), params_small_);
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client.get());
  message_loop_->RunUntilIdle();
}

TEST_F(VideoCaptureImplTest, LargeAndSmall) {
  // Execute SetCapture() and StopCapture() for 2 clients simultaneously.
  // The large client starts first and stops first.
  scoped_ptr<MockVideoCaptureClient> client_small(new MockVideoCaptureClient);
  scoped_ptr<MockVideoCaptureClient> client_large(new MockVideoCaptureClient);

  EXPECT_CALL(*client_large, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_small, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client_large.get(), params_large_);
  video_capture_impl_->StartCapture(client_small.get(), params_small_);
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client_large, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_large, OnRemoved(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_small, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_small, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client_large.get());
  video_capture_impl_->StopCapture(client_small.get());
  message_loop_->RunUntilIdle();
}

TEST_F(VideoCaptureImplTest, SmallAndLarge) {
  // Execute SetCapture() and StopCapture() for 2 clients simultaneously.
  // The small client starts first and stops first.
  scoped_ptr<MockVideoCaptureClient> client_small(new MockVideoCaptureClient);
  scoped_ptr<MockVideoCaptureClient> client_large(new MockVideoCaptureClient);

  EXPECT_CALL(*client_large, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_small, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client_small.get(), params_small_);
  video_capture_impl_->StartCapture(client_large.get(), params_large_);
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client_large, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_large, OnRemoved(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_small, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client_small, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client_small.get());
  video_capture_impl_->StopCapture(client_large.get());
  message_loop_->RunUntilIdle();
}

TEST_F(VideoCaptureImplTest, TwoClientsWithSameSize) {
  // Execute SetCapture() and StopCapture() for 2 clients simultaneously.
  // The client1 starts first and stops first.
  scoped_ptr<MockVideoCaptureClient> client1(new MockVideoCaptureClient);
  scoped_ptr<MockVideoCaptureClient> client2(new MockVideoCaptureClient);

  EXPECT_CALL(*client1, OnStarted(_))
      .WillOnce(Return());
  EXPECT_CALL(*client2, OnStarted(_))
      .WillOnce(Return());

  video_capture_impl_->StartCapture(client1.get(), params_small_);
  video_capture_impl_->StartCapture(client2.get(), params_small_);
  message_loop_->RunUntilIdle();

  EXPECT_CALL(*client1, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client1, OnRemoved(_))
      .WillOnce(Return());
  EXPECT_CALL(*client2, OnStopped(_))
      .WillOnce(Return());
  EXPECT_CALL(*client2, OnRemoved(_))
      .WillOnce(Return());

  video_capture_impl_->StopCapture(client1.get());
  video_capture_impl_->StopCapture(client2.get());
  message_loop_->RunUntilIdle();
}

}  // namespace content
