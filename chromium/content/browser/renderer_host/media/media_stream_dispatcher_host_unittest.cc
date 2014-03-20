// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <queue>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/mock_audio_manager.h"
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
      const ResourceContext::SaltCallback salt_callback,
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      MediaStreamManager* manager)
      : MediaStreamDispatcherHost(kProcessId, salt_callback, manager),
        message_loop_(message_loop) {}

  // A list of mock methods.
  MOCK_METHOD4(OnStreamGenerated,
               void(int routing_id, int request_id, int audio_array_size,
                    int video_array_size));
  MOCK_METHOD2(OnStreamGenerationFailed, void(int routing_id, int request_id));
  MOCK_METHOD1(OnDeviceStopped, void(int routing_id));
  MOCK_METHOD2(OnDeviceOpened, void(int routing_id, int request_id));

  // Accessor to private functions.
  void OnGenerateStream(int render_view_id,
                        int page_request_id,
                        const StreamOptions& components,
                        const GURL& security_origin,
                        const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnGenerateStream(
        render_view_id, page_request_id, components, security_origin);
  }

  void OnStopStreamDevice(int render_view_id,
                          const std::string& device_id) {
    MediaStreamDispatcherHost::OnStopStreamDevice(render_view_id, device_id);
  }

  void OnOpenDevice(int render_view_id,
                    int page_request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    const GURL& security_origin,
                    const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnOpenDevice(
        render_view_id, page_request_id, device_id, type, security_origin);
  }

  void OnEnumerateDevices(int render_view_id,
                          int page_request_id,
                          MediaStreamType type,
                          const GURL& security_origin,
                          const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnEnumerateDevices(
        render_view_id, page_request_id, type, security_origin);
  }

  std::string label_;
  StreamDeviceInfoArray audio_devices_;
  StreamDeviceInfoArray video_devices_;
  StreamDeviceInfo opened_device_;
  StreamDeviceInfoArray enumerated_devices_;

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
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceStopped, OnDeviceStopped)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceOpened, OnDeviceOpened)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesEnumerated,
                          OnDevicesEnumerated)
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
    // Notify that the event have occurred.
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));

    label_ = label;
    audio_devices_ = audio_device_list;
    video_devices_ = video_device_list;
  }

  void OnStreamGenerationFailed(const IPC::Message& msg, int request_id) {
    OnStreamGenerationFailed(msg.routing_id(), request_id);
    if (!quit_closures_.empty()) {
      base::Closure quit_closure = quit_closures_.front();
      quit_closures_.pop();
      message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    }

    label_= "";
  }

  void OnDeviceStopped(const IPC::Message& msg,
                       const std::string& label,
                       const content::StreamDeviceInfo& device) {
    if (IsVideoMediaType(device.device.type))
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(device, video_devices_[0]));
    if (IsAudioMediaType(device.device.type))
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(device, audio_devices_[0]));

    OnDeviceStopped(msg.routing_id());
  }

  void OnDeviceOpened(const IPC::Message& msg,
                      int request_id,
                      const std::string& label,
                      const StreamDeviceInfo& device) {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    label_ = label;
    opened_device_ = device;
  }

  void OnDevicesEnumerated(const IPC::Message& msg,
                           int request_id,
                           const StreamDeviceInfoArray& devices) {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    enumerated_devices_ = devices;
  }

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  std::queue<base::Closure> quit_closures_;
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MOCK_METHOD1(OnStarted, void(const base::Closure& stop));
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest()
      : old_browser_client_(NULL),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        origin_("https://test.com") {
    audio_manager_.reset(
        new media::MockAudioManager(base::MessageLoopProxy::current()));
    // Create our own MediaStreamManager.
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    // Make sure we use fake devices to avoid long delays.
    media_stream_manager_->UseFakeDevice();

    host_ = new MockMediaStreamDispatcherHost(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        base::MessageLoopProxy::current(),
        media_stream_manager_.get());

    // Use the fake content client and browser.
    content_client_.reset(new TestContentClient());
    SetContentClient(content_client_.get());
    old_browser_client_ = SetBrowserClientForTesting(host_.get());
  }

  virtual ~MediaStreamDispatcherHostTest() {
  }

  virtual void SetUp() OVERRIDE {
    media::FakeVideoCaptureDevice::GetDeviceNames(&physical_video_devices_);
    ASSERT_GT(physical_video_devices_.size(), 0u);

    audio_manager_->GetAudioInputDeviceNames(&physical_audio_devices_);
    ASSERT_GT(physical_audio_devices_.size(), 0u);
  }

  virtual void TearDown() OVERRIDE {
    host_->OnChannelClosing();
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

  void GenerateStreamAndWaitForResult(int render_view_id,
                                      int page_request_id,
                                      const StreamOptions& options) {
    base::RunLoop run_loop;
    int expected_audio_array_size =
        (options.audio_requested &&
         physical_audio_devices_.size() > 0) ? 1 : 0;
    int expected_video_array_size =
        (options.video_requested &&
         physical_video_devices_.size() > 0) ? 1 : 0;
    EXPECT_CALL(*host_.get(), OnStreamGenerated(render_view_id, page_request_id,
                                                expected_audio_array_size,
                                                expected_video_array_size));
    host_->OnGenerateStream(render_view_id, page_request_id, options, origin_,
                            run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->audio_devices_));
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->audio_devices_, origin_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void GenerateStreamAndWaitForFailure(int render_view_id,
                                       int page_request_id,
                                       const StreamOptions& options) {
      base::RunLoop run_loop;
      EXPECT_CALL(*host_.get(),
                  OnStreamGenerationFailed(render_view_id, page_request_id));
      host_->OnGenerateStream(render_view_id, page_request_id, options, origin_,
                              run_loop.QuitClosure());
      run_loop.Run();
  }

  void OpenVideoDeviceAndWaitForResult(int render_view_id,
                                       int page_request_id,
                                       const std::string& device_id) {
    base::RunLoop run_loop;
    host_->OnOpenDevice(render_view_id, page_request_id, device_id,
                        MEDIA_DEVICE_VIDEO_CAPTURE, origin_,
                        run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void EnumerateDevicesAndWaitForResult(int render_view_id,
                                        int page_request_id,
                                        MediaStreamType type) {
    base::RunLoop run_loop;
    host_->OnEnumerateDevices(render_view_id, page_request_id, type, origin_,
                              run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_FALSE(host_->enumerated_devices_.empty());
    EXPECT_FALSE(DoesContainRawIds(host_->enumerated_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->enumerated_devices_, origin_));
  }

  bool DoesContainRawIds(const StreamDeviceInfoArray& devices) {
    for (size_t i = 0; i < devices.size(); ++i) {
      media::AudioDeviceNames::const_iterator audio_it =
          physical_audio_devices_.begin();
      for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
        if (audio_it->unique_id == devices[i].device.id)
          return true;
      }
      media::VideoCaptureDevice::Names::const_iterator video_it =
          physical_video_devices_.begin();
      for (; video_it != physical_video_devices_.end(); ++video_it) {
        if (video_it->id() == devices[i].device.id)
          return true;
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(const StreamDeviceInfoArray& devices,
                                 const GURL& origin) {
    for (size_t i = 0; i < devices.size(); ++i) {
      bool found_match = false;
      media::AudioDeviceNames::const_iterator audio_it =
          physical_audio_devices_.begin();
      for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
        if (content::DoesMediaDeviceIDMatchHMAC(
                browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
                origin,
                devices[i].device.id,
                audio_it->unique_id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      media::VideoCaptureDevice::Names::const_iterator video_it =
          physical_video_devices_.begin();
      for (; video_it != physical_video_devices_.end(); ++video_it) {
        if (content::DoesMediaDeviceIDMatchHMAC(
                browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
                origin,
                devices[i].device.id,
                video_it->id())) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      if (!found_match)
        return false;
    }
    return true;
  }

  void AddSourceIdConstraint(const std::string& source_id,
                             StreamOptions::Constraints* constraints) {
    constraints->push_back(StreamOptions::Constraint(kMediaStreamSourceInfoId,
                                                     source_id));
  }

  scoped_refptr<MockMediaStreamDispatcherHost> host_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  ContentBrowserClient* old_browser_client_;
  scoped_ptr<ContentClient> content_client_;
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestBrowserContext browser_context_;
  media::AudioDeviceNames physical_audio_devices_;
  media::VideoCaptureDevice::Names physical_video_devices_;
  GURL origin_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithVideoOnly) {
  StreamOptions options(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioOnly) {
  StreamOptions options(true, false);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithNothing) {
  StreamOptions options(false, false);

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, options);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioAndVideo) {
  StreamOptions options(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

// This test generates two streams with video only using the same render view
// id. The same capture device  with the same device and session id is expected
// to be used.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsFromSameRenderId) {
  StreamOptions options(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().device.id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId + 1, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().device.id;
  int session_id2 = host_->video_devices_.front().session_id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamAndOpenDeviceFromSameRenderId) {
  StreamOptions options(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().device.id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId, device_id1);

  const std::string device_id2 = host_->opened_device_.device.id;
  const int session_id2 = host_->opened_device_.session_id;
  const std::string label2 = host_->label_;

  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}


// This test generates two streams with video only using two separate render
// view ids. The same device id but different session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsDifferentRenderId) {
  StreamOptions options(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().device.id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream from another render view.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId+1, kPageRequestId + 1, options);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().device.id;
  const int session_id2 = host_->video_devices_.front().session_id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test request two streams with video only without waiting for the first
// stream to be generated before requesting the second.
// The same device id and session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithoutWaiting) {
  StreamOptions options(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  EXPECT_CALL(*host_.get(), OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));

  // Generate second stream.
  EXPECT_CALL(*host_.get(),
              OnStreamGenerated(kRenderId, kPageRequestId + 1, 0, 1));

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  host_->OnGenerateStream(kRenderId, kPageRequestId, options, origin_,
                          run_loop1.QuitClosure());
  host_->OnGenerateStream(kRenderId, kPageRequestId + 1, options, origin_,
                          run_loop2.QuitClosure());

  run_loop1.Run();
  run_loop2.Run();
}

// Test that we can generate streams where a mandatory sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithMandatorySourceId) {
  ASSERT_GE(physical_audio_devices_.size(), 1u);
  ASSERT_GE(physical_video_devices_.size(), 1u);

  media::AudioDeviceNames::const_iterator audio_it =
      physical_audio_devices_.begin();
  for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    StreamOptions options(true, true);
    AddSourceIdConstraint(source_id, &options.mandatory_audio);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
    EXPECT_EQ(host_->audio_devices_[0].device.id, source_id);
  }

  media::VideoCaptureDevice::Names::const_iterator video_it =
      physical_video_devices_.begin();
  for (; video_it != physical_video_devices_.end(); ++video_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        video_it->id());
    ASSERT_FALSE(source_id.empty());
    StreamOptions options(true, true);
    AddSourceIdConstraint(source_id, &options.mandatory_video);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
    EXPECT_EQ(host_->video_devices_[0].device.id, source_id);
  }
}

// Test that we can generate streams where a optional sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithOptionalSourceId) {
  ASSERT_GE(physical_audio_devices_.size(), 1u);
  ASSERT_GE(physical_video_devices_.size(), 1u);

  media::AudioDeviceNames::const_iterator audio_it =
      physical_audio_devices_.begin();
  for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    StreamOptions options(true, true);
    AddSourceIdConstraint(source_id, &options.optional_audio);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
    EXPECT_EQ(host_->audio_devices_[0].device.id, source_id);
  }

  media::VideoCaptureDevice::Names::const_iterator video_it =
      physical_video_devices_.begin();
  for (; video_it != physical_video_devices_.end(); ++video_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        video_it->id());
    ASSERT_FALSE(source_id.empty());
    StreamOptions options(true, true);
    AddSourceIdConstraint(source_id, &options.optional_video);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
    EXPECT_EQ(host_->video_devices_[0].device.id, source_id);
  }
}

// Test that generating a stream with an invalid mandatory video source id fail.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidMandatoryVideoSourceId) {
  StreamOptions options(true, true);
  AddSourceIdConstraint("invalid source id", &options.mandatory_video);

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, options);
}

// Test that generating a stream with an invalid mandatory audio source id fail.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidMandatoryAudioSourceId) {
  StreamOptions options(true, true);
  AddSourceIdConstraint("invalid source id", &options.mandatory_audio);

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, options);
}

// Test that generating a stream with an invalid optional video source id
// succeed.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidOptionalVideoSourceId) {
  StreamOptions options(true, true);
  AddSourceIdConstraint("invalid source id", &options.optional_video);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
}

// Test that generating a stream with an invalid optional audio source id
// succeed.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidOptionalAudioSourceId) {
  StreamOptions options(true, true);
  AddSourceIdConstraint("invalid source id", &options.optional_audio);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsNoAvailableVideoDevice) {
  size_t number_of_fake_devices = physical_video_devices_.size();
  media::FakeVideoCaptureDevice::SetNumberOfFakeDevices(0);
  media::FakeVideoCaptureDevice::GetDeviceNames(&physical_video_devices_);
  StreamOptions options(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
  EXPECT_TRUE(host_->video_devices_.empty());

  // Reset the number of fake devices for next test.
  media::FakeVideoCaptureDevice::SetNumberOfFakeDevices(number_of_fake_devices);
}

// Test that if a OnStopStreamDevice message is received for a device that has
// been opened in a MediaStream and by pepper, the device is only stopped for
// the MediaStream.
TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStream) {
  StreamOptions options(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  std::string stream_request_label = host_->label_;
  StreamDeviceInfo video_device_info = host_->video_devices_.front();
  ASSERT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      stream_request_label).size());

  // Open the same device by Pepper.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId,
                                  video_device_info.device.id);
  std::string open_device_request_label = host_->label_;

  // Stop the device in the MediaStream.
  host_->OnStopStreamDevice(kRenderId, video_device_info.device.id);

  EXPECT_EQ(0u, media_stream_manager_->GetDevicesOpenedByRequest(
      stream_request_label).size());
  EXPECT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      open_device_request_label).size());
}

TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStreamAndRestart) {
  StreamOptions options(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  std::string request_label1 = host_->label_;
  StreamDeviceInfo video_device_info = host_->video_devices_.front();
  // Expect that 1 audio and 1 video device has been opened.
  EXPECT_EQ(2u, media_stream_manager_->GetDevicesOpenedByRequest(
      request_label1).size());

  host_->OnStopStreamDevice(kRenderId, video_device_info.device.id);
  EXPECT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      request_label1).size());

  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
  std::string request_label2 = host_->label_;

  StreamDeviceInfoArray request1_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1);
  StreamDeviceInfoArray request2_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label2);

  ASSERT_EQ(1u, request1_devices.size());
  ASSERT_EQ(2u, request2_devices.size());

  // Test that the same audio device has been opened in both streams.
  EXPECT_TRUE(StreamDeviceInfo::IsEqual(request1_devices[0],
                                        request2_devices[0]) ||
              StreamDeviceInfo::IsEqual(request1_devices[0],
                                        request2_devices[1]));
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreamsOnChannelClosing) {
  StreamOptions options(false, true);

  base::RunLoop run_loop;

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStream(kRenderId, kPageRequestId + i, options, origin_,
                            run_loop.QuitClosure());
  }

  // Calling OnChannelClosing() to cancel all the pending requests.
  host_->OnChannelClosing();
  run_loop.RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreamsOnChannelClosing) {
  StreamOptions options(false, true);

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i) {
    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId + i, options);
  }

  // Calling OnChannelClosing() to cancel all the pending/generated streams.
  host_->OnChannelClosing();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, CloseFromUI) {
  StreamOptions options(false, true);

  base::Closure close_callback;
  scoped_ptr<MockMediaStreamUIProxy> stream_ui(new MockMediaStreamUIProxy());
  EXPECT_CALL(*stream_ui, OnStarted(_))
      .WillOnce(SaveArg<0>(&close_callback));
  media_stream_manager_->UseFakeUI(stream_ui.PassAs<FakeMediaStreamUIProxy>());

  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  ASSERT_FALSE(close_callback.is_null());
  EXPECT_CALL(*host_.get(), OnDeviceStopped(kRenderId));
  close_callback.Run();
  base::RunLoop().RunUntilIdle();
}

// Test that the dispatcher is notified if a video device that is in use is
// being unplugged.
TEST_F(MediaStreamDispatcherHostTest, VideoDeviceUnplugged) {
  size_t number_of_fake_devices = physical_video_devices_.size();
  StreamOptions options(true, true);
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, options);
  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  media::FakeVideoCaptureDevice::SetNumberOfFakeDevices(0);

  base::RunLoop run_loop;
  EXPECT_CALL(*host_.get(), OnDeviceStopped(kRenderId))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  media_stream_manager_->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  run_loop.Run();

  media::FakeVideoCaptureDevice::SetNumberOfFakeDevices(number_of_fake_devices);
}

TEST_F(MediaStreamDispatcherHostTest, EnumerateAudioDevices) {
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_AUDIO_CAPTURE);
}

TEST_F(MediaStreamDispatcherHostTest, EnumerateVideoDevices) {
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_VIDEO_CAPTURE);
}

};  // namespace content
