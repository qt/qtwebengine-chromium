// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_manager.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

const int kProcessId = 5;
const int kRenderId = 6;
const int kPageRequestId = 7;

namespace content {

class MockMediaStreamDispatcherHost : public MediaStreamDispatcherHost,
                                      public TestContentBrowserClient {
 public:
  MockMediaStreamDispatcherHost(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      MediaStreamManager* manager)
      : MediaStreamDispatcherHost(kProcessId, manager),
        message_loop_(message_loop) {}

  // A list of mock methods.
  MOCK_METHOD4(OnStreamGenerated,
               void(int routing_id, int request_id, int audio_array_size,
                    int video_array_size));
  MOCK_METHOD2(OnStreamGenerationFailed, void(int routing_id, int request_id));
  MOCK_METHOD1(OnStopGeneratedStreamFromBrowser,
               void(int routing_id));

  // Accessor to private functions.
  void OnGenerateStream(int page_request_id,
                        const StreamOptions& components,
                        const base::Closure& quit_closure) {
    quit_closure_ = quit_closure;
    MediaStreamDispatcherHost::OnGenerateStream(
        kRenderId, page_request_id, components, GURL());
  }

  void OnStopGeneratedStream(const std::string& label) {
    MediaStreamDispatcherHost::OnStopGeneratedStream(kRenderId, label);
  }

  // Return the number of streams that have been opened or is being open.
  size_t NumberOfStreams() {
    return streams_.size();
  }

  std::string label_;
  StreamDeviceInfoArray audio_devices_;
  StreamDeviceInfoArray video_devices_;

 private:
  virtual ~MockMediaStreamDispatcherHost() {}

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) OVERRIDE {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockMediaStreamDispatcherHost, *message)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerated, OnStreamGenerated)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerationFailed,
                          OnStreamGenerationFailed)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StopGeneratedStream,
                          OnStopGeneratedStreamFromBrowser)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnStreamGenerated(
      const IPC::Message& msg,
      int request_id,
      std::string label,
      StreamDeviceInfoArray audio_device_list,
      StreamDeviceInfoArray video_device_list) {
    OnStreamGenerated(msg.routing_id(), request_id, audio_device_list.size(),
        video_device_list.size());
    // Notify that the event have occured.
    message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure_));
    label_ = label;
    audio_devices_ = audio_device_list;
    video_devices_ = video_device_list;
  }

  void OnStreamGenerationFailed(const IPC::Message& msg, int request_id) {
    OnStreamGenerationFailed(msg.routing_id(), request_id);
    if (!quit_closure_.is_null())
      message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure_));
    label_= "";
  }

  void OnStopGeneratedStreamFromBrowser(const IPC::Message& msg,
                                        const std::string& label) {
    OnStopGeneratedStreamFromBrowser(msg.routing_id());
    // Notify that the event have occured.
    if (!quit_closure_.is_null())
      message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure_));
    label_ = "";
  }

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::Closure quit_closure_;
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MOCK_METHOD1(OnStarted, void(const base::Closure& stop));
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest()
      : old_browser_client_(NULL),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
    // Create our own MediaStreamManager.
    audio_manager_.reset(media::AudioManager::Create());
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    // Make sure we use fake devices to avoid long delays.
    media_stream_manager_->UseFakeDevice();

    host_ = new MockMediaStreamDispatcherHost(base::MessageLoopProxy::current(),
                                              media_stream_manager_.get());

    // Use the fake content client and browser.
    content_client_.reset(new TestContentClient());
    SetContentClient(content_client_.get());
    old_browser_client_ = SetBrowserClientForTesting(host_.get());
  }

  virtual ~MediaStreamDispatcherHostTest() {
    // Recover the old browser client and content client.
    SetBrowserClientForTesting(old_browser_client_);
    content_client_.reset();
    media_stream_manager_->WillDestroyCurrentMessageLoop();
  }

 protected:
  virtual void SetupFakeUI(bool expect_started) {
    scoped_ptr<MockMediaStreamUIProxy> stream_ui(new MockMediaStreamUIProxy());
    if (expect_started) {
      EXPECT_CALL(*stream_ui, OnStarted(_));
    }
    media_stream_manager_->UseFakeUI(
        stream_ui.PassAs<FakeMediaStreamUIProxy>());
  }

  void GenerateStreamAndWaitForResult(int page_request_id,
                                      const StreamOptions& options) {
    base::RunLoop run_loop;
    host_->OnGenerateStream(page_request_id, options, run_loop.QuitClosure());
    run_loop.Run();
  }

  scoped_refptr<MockMediaStreamDispatcherHost> host_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  ContentBrowserClient* old_browser_client_;
  scoped_ptr<ContentClient> content_client_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStream) {
  StreamOptions options(MEDIA_NO_SERVICE, MEDIA_DEVICE_VIDEO_CAPTURE);

  SetupFakeUI(true);
  EXPECT_CALL(*host_.get(), OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  GenerateStreamAndWaitForResult(kPageRequestId, options);

  std::string label =  host_->label_;

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  host_->OnStopGeneratedStream(label);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateThreeStreams) {
  // This test opens three video capture devices. Two fake devices exists and it
  // is expected the last call to |Open()| will open the first device again, but
  // with a different label.
  StreamOptions options(MEDIA_NO_SERVICE, MEDIA_DEVICE_VIDEO_CAPTURE);

  // Generate first stream.
  SetupFakeUI(true);
  EXPECT_CALL(*host_.get(), OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  GenerateStreamAndWaitForResult(kPageRequestId, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  std::string label1 = host_->label_;
  std::string device_id1 = host_->video_devices_.front().device.id;

  // Check that we now have one opened streams.
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  // Generate second stream.
  SetupFakeUI(true);
  EXPECT_CALL(*host_.get(),
              OnStreamGenerated(kRenderId, kPageRequestId + 1, 0, 1));
  GenerateStreamAndWaitForResult(kPageRequestId + 1, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  std::string label2 = host_->label_;
  std::string device_id2 = host_->video_devices_.front().device.id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(label1, label2);

  // Check that we now have two opened streams.
  EXPECT_EQ(2u, host_->NumberOfStreams());

  // Generate third stream.
  SetupFakeUI(true);
  EXPECT_CALL(*host_.get(),
              OnStreamGenerated(kRenderId, kPageRequestId + 2, 0, 1));
  GenerateStreamAndWaitForResult(kPageRequestId + 2, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  std::string label3 = host_->label_;
  std::string device_id3 = host_->video_devices_.front().device.id;
  EXPECT_EQ(device_id1, device_id3);
  EXPECT_NE(label1, label3);
  EXPECT_NE(label2, label3);

  // Check that we now have three opened streams.
  EXPECT_EQ(host_->NumberOfStreams(), 3u);

  host_->OnStopGeneratedStream(label1);
  host_->OnStopGeneratedStream(label2);
  host_->OnStopGeneratedStream(label3);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreamsOnChannelClosing) {
  StreamOptions options(MEDIA_NO_SERVICE, MEDIA_DEVICE_VIDEO_CAPTURE);

  base::RunLoop run_loop;

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStream(
        kPageRequestId + i, options, run_loop.QuitClosure());
    EXPECT_EQ(host_->NumberOfStreams(), i);
  }

  // Calling OnChannelClosing() to cancel all the pending requests.
  host_->OnChannelClosing();
  run_loop.RunUntilIdle();

  // Streams should have been cleaned up.
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreamsOnChannelClosing) {
  StreamOptions options(MEDIA_NO_SERVICE, MEDIA_DEVICE_VIDEO_CAPTURE);

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i) {
    SetupFakeUI(true);
    EXPECT_CALL(*host_.get(),
                OnStreamGenerated(kRenderId, kPageRequestId + i, 0, 1));
    GenerateStreamAndWaitForResult(kPageRequestId + i, options);
  }
  EXPECT_EQ(host_->NumberOfStreams(), generated_streams);

  // Calling OnChannelClosing() to cancel all the pending/generated streams.
  host_->OnChannelClosing();
  base::RunLoop().RunUntilIdle();

  // Streams should have been cleaned up.
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, CloseFromUI) {
  StreamOptions options(MEDIA_NO_SERVICE, MEDIA_DEVICE_VIDEO_CAPTURE);

  base::Closure close_callback;
  scoped_ptr<MockMediaStreamUIProxy> stream_ui(new MockMediaStreamUIProxy());
  EXPECT_CALL(*stream_ui, OnStarted(_))
      .WillOnce(SaveArg<0>(&close_callback));
  media_stream_manager_->UseFakeUI(stream_ui.PassAs<FakeMediaStreamUIProxy>());

  EXPECT_CALL(*host_.get(), OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  EXPECT_CALL(*host_.get(), OnStopGeneratedStreamFromBrowser(kRenderId));
  GenerateStreamAndWaitForResult(kPageRequestId, options);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  ASSERT_FALSE(close_callback.is_null());
  close_callback.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

};  // namespace content
