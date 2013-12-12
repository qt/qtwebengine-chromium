// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sync_socket.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/common/media/audio_messages.h"
#include "content/common/media/media_stream_options.h"
#include "ipc/ipc_message_utils.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_output_stream.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::NotNull;

namespace content {

static const int kRenderProcessId = 1;
static const int kRenderViewId = 4;
static const int kStreamId = 50;

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MockAudioMirroringManager() {}
  virtual ~MockAudioMirroringManager() {}

  MOCK_METHOD3(AddDiverter,
               void(int render_process_id, int render_view_id,
                    Diverter* diverter));
  MOCK_METHOD3(RemoveDiverter,
               void(int render_process_id, int render_view_id,
                    Diverter* diverter));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioMirroringManager);
};

class MockAudioRendererHost : public AudioRendererHost {
 public:
  explicit MockAudioRendererHost(
      media::AudioManager* audio_manager,
      AudioMirroringManager* mirroring_manager,
      MediaInternals* media_internals,
      MediaStreamManager* media_stream_manager)
      : AudioRendererHost(kRenderProcessId,
                          audio_manager,
                          mirroring_manager,
                          media_internals,
                          media_stream_manager),
        shared_memory_length_(0) {
  }

  // A list of mock methods.
  MOCK_METHOD2(OnStreamCreated,
               void(int stream_id, int length));
  MOCK_METHOD1(OnStreamPlaying, void(int stream_id));
  MOCK_METHOD1(OnStreamPaused, void(int stream_id));
  MOCK_METHOD1(OnStreamError, void(int stream_id));

 private:
  virtual ~MockAudioRendererHost() {
    // Make sure all audio streams have been deleted.
    EXPECT_TRUE(audio_entries_.empty());
  }

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioRendererHost, *message)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated,
                          OnStreamCreated)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged,
                          OnStreamStateChanged)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  void OnStreamCreated(const IPC::Message& msg, int stream_id,
                       base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                       base::SyncSocket::Handle socket_handle,
#else
                       base::FileDescriptor socket_descriptor,
#endif
                       uint32 length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    CHECK(shared_memory_->Map(length));
    CHECK(shared_memory_->memory());
    shared_memory_length_ = length;

    // Create the SyncSocket using the handle.
    base::SyncSocket::Handle sync_socket_handle;
#if defined(OS_WIN)
    sync_socket_handle = socket_handle;
#else
    sync_socket_handle = socket_descriptor.fd;
#endif
    sync_socket_.reset(new base::SyncSocket(sync_socket_handle));

    // And then delegate the call to the mock method.
    OnStreamCreated(stream_id, length);
  }

  void OnStreamStateChanged(const IPC::Message& msg, int stream_id,
                            media::AudioOutputIPCDelegate::State state) {
    switch (state) {
      case media::AudioOutputIPCDelegate::kPlaying:
        OnStreamPlaying(stream_id);
        break;
      case media::AudioOutputIPCDelegate::kPaused:
        OnStreamPaused(stream_id);
        break;
      case media::AudioOutputIPCDelegate::kError:
        OnStreamError(stream_id);
        break;
      default:
        FAIL() << "Unknown stream state";
        break;
    }
  }

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> sync_socket_;
  uint32 shared_memory_length_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

class AudioRendererHostTest : public testing::Test {
 public:
  AudioRendererHostTest() : is_stream_active_(false) {}

 protected:
  virtual void SetUp() {
    // Create a message loop so AudioRendererHost can use it.
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));

    // Claim to be on both the UI and IO threads to pass all the DCHECKS.
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));
    audio_manager_.reset(media::AudioManager::Create());
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    media_stream_manager_->UseFakeDevice();
    observer_.reset(new MockMediaInternals());
    host_ = new MockAudioRendererHost(
        audio_manager_.get(), &mirroring_manager_, observer_.get(),
        media_stream_manager_.get());

    // Simulate IPC channel connected.
    host_->OnChannelConnected(base::GetCurrentProcId());
  }

  virtual void TearDown() {
    // Simulate closing the IPC channel.
    host_->OnChannelClosing();

    // Release the reference to the mock object.  The object will be destructed
    // on message_loop_.
    host_ = NULL;

    // We need to continue running message_loop_ to complete all destructions.
    SyncWithAudioThread();
    audio_manager_.reset();

    // Make sure the stream has been deleted before continuing.
    while (is_stream_active_)
      message_loop_->Run();

    io_thread_.reset();
    ui_thread_.reset();

    // Delete the IO message loop.  This will cause the MediaStreamManager to be
    // notified so it will stop its device thread and device managers.
    message_loop_.reset();
  }

  void Create(bool unified_stream) {
    EXPECT_CALL(*host_.get(), OnStreamCreated(kStreamId, _))
        .WillOnce(DoAll(Assign(&is_stream_active_, true),
                        QuitMessageLoop(message_loop_.get())));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderViewId, NotNull()))
        .RetiresOnSaturation();

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    int session_id;
    media::AudioParameters params;
    if (unified_stream) {
      // Use AudioInputDeviceManager::kFakeOpenSessionId as the session id to
      // pass the permission check.
      session_id = AudioInputDeviceManager::kFakeOpenSessionId;
      params = media::AudioParameters(
          media::AudioParameters::AUDIO_FAKE,
          media::CHANNEL_LAYOUT_STEREO,
          2,
          media::AudioParameters::kAudioCDSampleRate, 16,
          media::AudioParameters::kAudioCDSampleRate / 10);
    } else {
      session_id = 0;
      params = media::AudioParameters(
          media::AudioParameters::AUDIO_FAKE,
          media::CHANNEL_LAYOUT_STEREO,
          media::AudioParameters::kAudioCDSampleRate, 16,
          media::AudioParameters::kAudioCDSampleRate / 10);
    }
    host_->OnCreateStream(kStreamId, kRenderViewId, session_id, params);
    message_loop_->Run();

    // At some point in the future, a corresponding RemoveDiverter() call must
    // be made.
    EXPECT_CALL(mirroring_manager_,
                RemoveDiverter(kRenderProcessId, kRenderViewId, NotNull()))
        .RetiresOnSaturation();

    // All created streams should ultimately be closed.
    EXPECT_CALL(*observer_,
                OnSetAudioStreamStatus(_, kStreamId, "closed"));

    // Expect the audio stream will be deleted at some later point.
    EXPECT_CALL(*observer_, OnDeleteAudioStream(_, kStreamId))
        .WillOnce(DoAll(Assign(&is_stream_active_, false),
                        QuitMessageLoop(message_loop_.get())));
  }

  void Close() {
    // Send a message to AudioRendererHost to tell it we want to close the
    // stream.
    host_->OnCloseStream(kStreamId);
    if (is_stream_active_)
      message_loop_->Run();
    else
      message_loop_->RunUntilIdle();
  }

  void Play() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamPlaying(_, kStreamId, true));
    EXPECT_CALL(*host_.get(), OnStreamPlaying(kStreamId))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    host_->OnPlayStream(kStreamId);
    message_loop_->Run();
  }

  void Pause() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamPlaying(_, kStreamId, false));
    EXPECT_CALL(*host_.get(), OnStreamPaused(kStreamId))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    host_->OnPauseStream(kStreamId);
    message_loop_->Run();
  }

  void SetVolume(double volume) {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamVolume(_, kStreamId, volume));

    host_->OnSetVolume(kStreamId, volume);
    message_loop_->RunUntilIdle();
  }

  void SimulateError() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamStatus(_, kStreamId, "error"));
    EXPECT_EQ(1u, host_->audio_entries_.size())
        << "Calls Create() before calling this method";

    // Expect an error signal sent through IPC.
    EXPECT_CALL(*host_.get(), OnStreamError(kStreamId));

    // Simulate an error sent from the audio device.
    host_->ReportErrorAndClose(kStreamId);
    SyncWithAudioThread();

    // Expect the audio stream record is removed.
    EXPECT_EQ(0u, host_->audio_entries_.size());
  }

  // Called on the audio thread.
  static void PostQuitMessageLoop(base::MessageLoop* message_loop) {
    message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

  // Called on the main thread.
  static void PostQuitOnAudioThread(media::AudioManager* audio_manager,
                                    base::MessageLoop* message_loop) {
    audio_manager->GetMessageLoop()->PostTask(FROM_HERE,
        base::Bind(&PostQuitMessageLoop, message_loop));
  }

  // SyncWithAudioThread() waits until all pending tasks on the audio thread
  // are executed while also processing pending task in message_loop_ on the
  // current thread. It is used to synchronize with the audio thread when we are
  // closing an audio stream.
  void SyncWithAudioThread() {
    // Don't use scoped_refptr to addref the media::AudioManager when posting
    // to the thread that itself owns.
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&PostQuitOnAudioThread,
            base::Unretained(audio_manager_.get()),
            message_loop_.get()));
    message_loop_->Run();
  }

 private:
  scoped_ptr<MockMediaInternals> observer_;
  MockAudioMirroringManager mirroring_manager_;
  scoped_refptr<MockAudioRendererHost> host_;
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;

  bool is_stream_active_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHostTest);
};

TEST_F(AudioRendererHostTest, CreateAndClose) {
  Create(false);
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreateAndShutdown) {
  Create(false);
}

TEST_F(AudioRendererHostTest, CreatePlayAndClose) {
  Create(false);
  Play();
  Close();
}

TEST_F(AudioRendererHostTest, CreatePlayPauseAndClose) {
  Create(false);
  Play();
  Pause();
  Close();
}

TEST_F(AudioRendererHostTest, SetVolume) {
  Create(false);
  SetVolume(0.5);
  Play();
  Pause();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayAndShutdown) {
  Create(false);
  Play();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayPauseAndShutdown) {
  Create(false);
  Play();
  Pause();
}

TEST_F(AudioRendererHostTest, SimulateError) {
  Create(false);
  Play();
  SimulateError();
}

// Simulate the case when an error is generated on the browser process,
// the audio device is closed but the render process try to close the
// audio stream again.
TEST_F(AudioRendererHostTest, SimulateErrorAndClose) {
  Create(false);
  Play();
  SimulateError();
  Close();
}

TEST_F(AudioRendererHostTest, CreateUnifiedStreamAndClose) {
  Create(true);
  Close();
}

// TODO(hclam): Add tests for data conversation in low latency mode.

}  // namespace content
