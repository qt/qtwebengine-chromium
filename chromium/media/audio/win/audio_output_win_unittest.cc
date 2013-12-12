// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/base_paths.h"
#include "base/memory/aligned_memory.h"
#include "base/path_service.h"
#include "base/sync_socket.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "media/base/limits.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_util.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

using base::win::ScopedCOMInitializer;

namespace media {

static const wchar_t kAudioFile1_16b_m_16K[]
    = L"media\\test\\data\\sweep02_16b_mono_16KHz.raw";

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestSourceBasic : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit TestSourceBasic()
      : callback_count_(0),
        had_error_(0) {
  }
  // AudioSourceCallback::OnMoreData implementation:
  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState buffers_state) {
    ++callback_count_;
    // Touch the channel memory value to make sure memory is good.
    audio_bus->Zero();
    return audio_bus->frames();
  }
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) {
    NOTREACHED();
    return 0;
  }
  // AudioSourceCallback::OnError implementation:
  virtual void OnError(AudioOutputStream* stream) {
    ++had_error_;
  }
  // Returns how many times OnMoreData() has been called.
  int callback_count() const {
    return callback_count_;
  }
  // Returns how many times the OnError callback was called.
  int had_error() const {
    return had_error_;
  }

  void set_error(bool error) {
    had_error_ += error ? 1 : 0;
  }

 private:
  int callback_count_;
  int had_error_;
};

const int kMaxNumBuffers = 3;
// Specializes TestSourceBasic to simulate a source that blocks for some time
// in the OnMoreData callback.
class TestSourceLaggy : public TestSourceBasic {
 public:
  TestSourceLaggy(int laggy_after_buffer, int lag_in_ms)
      : laggy_after_buffer_(laggy_after_buffer), lag_in_ms_(lag_in_ms) {
  }
  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState buffers_state) {
    // Call the base, which increments the callback_count_.
    TestSourceBasic::OnMoreData(audio_bus, buffers_state);
    if (callback_count() > kMaxNumBuffers) {
      ::Sleep(lag_in_ms_);
    }
    return audio_bus->frames();
  }
 private:
  int laggy_after_buffer_;
  int lag_in_ms_;
};

class MockAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD2(OnMoreData, int(AudioBus* audio_bus,
                               AudioBuffersState buffers_state));
  MOCK_METHOD3(OnMoreIOData, int(AudioBus* source,
                                 AudioBus* dest,
                                 AudioBuffersState buffers_state));
  MOCK_METHOD1(OnError, void(AudioOutputStream* stream));

  static int ClearData(AudioBus* audio_bus, AudioBuffersState buffers_state) {
    audio_bus->Zero();
    return audio_bus->frames();
  }
};

// Helper class to memory map an entire file. The mapping is read-only. Don't
// use for gigabyte-sized files. Attempts to write to this memory generate
// memory access violations.
class ReadOnlyMappedFile {
 public:
  explicit ReadOnlyMappedFile(const wchar_t* file_name)
      : fmap_(NULL), start_(NULL), size_(0) {
    HANDLE file = ::CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == file)
      return;
    fmap_ = ::CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    ::CloseHandle(file);
    if (!fmap_)
      return;
    start_ = reinterpret_cast<char*>(::MapViewOfFile(fmap_, FILE_MAP_READ,
                                                     0, 0, 0));
    if (!start_)
      return;
    MEMORY_BASIC_INFORMATION mbi = {0};
    ::VirtualQuery(start_, &mbi, sizeof(mbi));
    size_ = mbi.RegionSize;
  }
  ~ReadOnlyMappedFile() {
    if (start_) {
      ::UnmapViewOfFile(start_);
      ::CloseHandle(fmap_);
    }
  }
  // Returns true if the file was successfully mapped.
  bool is_valid() const {
    return ((start_ > 0) && (size_ > 0));
  }
  // Returns the size in bytes of the mapped memory.
  uint32 size() const {
    return size_;
  }
  // Returns the memory backing the file.
  const void* GetChunkAt(uint32 offset) {
    return &start_[offset];
  }

 private:
  HANDLE fmap_;
  char* start_;
  uint32 size_;
};

// ===========================================================================
// Validation of AudioManager::AUDIO_PCM_LINEAR
//
// NOTE:
// The tests can fail on the build bots when somebody connects to them via
// remote-desktop and the rdp client installs an audio device that fails to open
// at some point, possibly when the connection goes idle.

// Test that can it be created and closed.
TEST(WinAudioTest, PCMWaveStreamGetAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      8000, 16, 256),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that can it be cannot be created with invalid parameters.
TEST(WinAudioTest, SanityOnMakeParams) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  AudioParameters::Format fmt = AudioParameters::AUDIO_PCM_LINEAR;
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_UNSUPPORTED, 8000, 16, 256),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_MONO, 1024 * 1024, 16, 256),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, 8000, 80, 256),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_UNSUPPORTED, 8000, 16, 256),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, -8000, 16, 256),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_MONO, 8000, 16, -100),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_MONO, 8000, 16, 0),
      std::string(), std::string()));
  EXPECT_TRUE(NULL == audio_man->MakeAudioOutputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_MONO, 8000, 16,
                      media::limits::kMaxSamplesPerPacket + 1),
      std::string(), std::string()));
}

// Test that it can be opened and closed.
TEST(WinAudioTest, PCMWaveStreamOpenAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      8000, 16, 256),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open());
  oas->Close();
}

// Test that it has a maximum packet size.
TEST(WinAudioTest, PCMWaveStreamOpenLimit) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      8000, 16, 1024 * 1024 * 1024),
      std::string(), std::string());
  EXPECT_TRUE(NULL == oas);
  if (oas)
    oas->Close();
}

// Test potential deadlock situation if the source is slow or blocks for some
// time. The actual EXPECT_GT are mostly meaningless and the real test is that
// the test completes in reasonable time.
TEST(WinAudioTest, PCMWaveSlowSource) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      16000, 16, 256),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);
  TestSourceLaggy test_laggy(2, 90);
  EXPECT_TRUE(oas->Open());
  // The test parameters cause a callback every 32 ms and the source is
  // sleeping for 90 ms, so it is guaranteed that we run out of ready buffers.
  oas->Start(&test_laggy);
  ::Sleep(500);
  EXPECT_GT(test_laggy.callback_count(), 2);
  EXPECT_FALSE(test_laggy.had_error());
  oas->Stop();
  ::Sleep(500);
  oas->Close();
}

// Test another potential deadlock situation if the thread that calls Start()
// gets paused. This test is best when run over RDP with audio enabled. See
// bug 19276 for more details.
TEST(WinAudioTest, PCMWaveStreamPlaySlowLoop) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32 samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate, 16, samples_100_ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);

  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);

  for (int ix = 0; ix != 5; ++ix) {
    oas->Start(&source);
    ::Sleep(10);
    oas->Stop();
  }
  oas->Close();
}


// This test produces actual audio for .5 seconds on the default wave
// device at 44.1K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing.
TEST(WinAudioTest, PCMWaveStreamPlay200HzTone44Kss) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32 samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate, 16, samples_100_ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);

  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);
  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();
  oas->Close();
}

// This test produces actual audio for for .5 seconds on the default wave
// device at 22K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing. The audio also should
// sound with a lower volume than PCMWaveStreamPlay200HzTone44Kss.
TEST(WinAudioTest, PCMWaveStreamPlay200HzTone22Kss) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32 samples_100_ms = AudioParameters::kAudioCDSampleRate / 20;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate / 2, 16,
                      samples_100_ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate/2);

  EXPECT_TRUE(oas->Open());

  oas->SetVolume(0.5);
  oas->Start(&source);
  ::Sleep(500);

  // Test that the volume is within the set limits.
  double volume = 0.0;
  oas->GetVolume(&volume);
  EXPECT_LT(volume, 0.51);
  EXPECT_GT(volume, 0.49);
  oas->Stop();
  oas->Close();
}

// Uses a restricted source to play ~2 seconds of audio for about 5 seconds. We
// try hard to generate situation where the two threads are accessing the
// object roughly at the same time.
TEST(WinAudioTest, PushSourceFile16KHz)  {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  static const int kSampleRate = 16000;
  SineWaveAudioSource source(1, 200.0, kSampleRate);
  // Compute buffer size for 100ms of audio.
  const uint32 kSamples100ms = (kSampleRate / 1000) * 100;
  // Restrict SineWaveAudioSource to 100ms of samples.
  source.CapSamples(kSamples100ms);

  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      kSampleRate, 16, kSamples100ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  EXPECT_TRUE(oas->Open());

  oas->SetVolume(1.0);
  oas->Start(&source);

  // We buffer and play at the same time, buffering happens every ~10ms and the
  // consuming of the buffer happens every ~100ms. We do 100 buffers which
  // effectively wrap around the file more than once.
  for (uint32 ix = 0; ix != 100; ++ix) {
    ::Sleep(10);
    source.Reset();
  }

  // Play a little bit more of the file.
  ::Sleep(500);

  oas->Stop();
  oas->Close();
}

// This test is to make sure an AudioOutputStream can be started after it was
// stopped. You will here two .5 seconds wave signal separated by 0.5 seconds
// of silence.
TEST(WinAudioTest, PCMWaveStreamPlayTwice200HzTone44Kss) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32 samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate, 16, samples_100_ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);
  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);

  // Play the wave for .5 seconds.
  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();

  // Sleep to give silence after stopping the AudioOutputStream.
  ::Sleep(250);

  // Start again and play for .5 seconds.
  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();

  oas->Close();
}

// With the low latency mode, WASAPI is utilized by default for Vista and
// higher and Wave is used for XP and lower. It is possible to utilize a
// smaller buffer size for WASAPI than for Wave.
TEST(WinAudioTest, PCMWaveStreamPlay200HzToneLowLatency) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  // The WASAPI API requires a correct COM environment.
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Use 10 ms buffer size for WASAPI and 50 ms buffer size for Wave.
  // Take the existing native sample rate into account.
  const AudioParameters params = audio_man->GetDefaultOutputStreamParameters();
  int sample_rate = params.sample_rate();
  uint32 samples_10_ms = sample_rate / 100;
  int n = 1;
  (base::win::GetVersion() <= base::win::VERSION_XP) ? n = 5 : n = 1;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_MONO, sample_rate,
                      16, n * samples_10_ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200, sample_rate);

  bool opened = oas->Open();
  if (!opened) {
    // It was not possible to open this audio device in mono.
    // No point in continuing the test so let's break here.
    LOG(WARNING) << "Mono is not supported. Skipping test.";
    oas->Close();
    return;
  }
  oas->SetVolume(1.0);

  // Play the wave for .8 seconds.
  oas->Start(&source);
  ::Sleep(800);
  oas->Stop();
  oas->Close();
}

// Check that the pending bytes value is correct what the stream starts.
TEST(WinAudioTest, PCMWaveStreamPendingBytes) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32 samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate, 16, samples_100_ms),
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  NiceMock<MockAudioSource> source;
  EXPECT_TRUE(oas->Open());

  uint32 bytes_100_ms = samples_100_ms * 2;

  // Audio output stream has either a double or triple buffer scheme.
  // We expect the amount of pending bytes will reaching up to 2 times of
  // |bytes_100_ms| depending on number of buffers used.
  // From that it would decrease as we are playing the data but not providing
  // new one. And then we will try to provide zero data so the amount of
  // pending bytes will go down and eventually read zero.
  InSequence s;

  EXPECT_CALL(source, OnMoreData(NotNull(),
                                 Field(&AudioBuffersState::pending_bytes, 0)))
      .WillOnce(Invoke(MockAudioSource::ClearData));
  switch (NumberOfWaveOutBuffers()) {
    case 2:
      break;  // Calls are the same as at end of 3-buffer scheme.
    case 3:
      EXPECT_CALL(source, OnMoreData(NotNull(),
                                     Field(&AudioBuffersState::pending_bytes,
                                           bytes_100_ms)))
          .WillOnce(Invoke(MockAudioSource::ClearData));
      EXPECT_CALL(source, OnMoreData(NotNull(),
                                     Field(&AudioBuffersState::pending_bytes,
                                           2 * bytes_100_ms)))
          .WillOnce(Invoke(MockAudioSource::ClearData));
      EXPECT_CALL(source, OnMoreData(NotNull(),
                                     Field(&AudioBuffersState::pending_bytes,
                                           2 * bytes_100_ms)))
          .Times(AnyNumber())
          .WillRepeatedly(Return(0));
      break;
    default:
      ASSERT_TRUE(false)
          << "Unexpected number of buffers: " << NumberOfWaveOutBuffers();
  }
  EXPECT_CALL(source, OnMoreData(NotNull(),
                                 Field(&AudioBuffersState::pending_bytes,
                                       bytes_100_ms)))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(source, OnMoreData(NotNull(),
                                 Field(&AudioBuffersState::pending_bytes, 0)))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));

  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();
  oas->Close();
}

// Simple source that uses a SyncSocket to retrieve the audio data
// from a potentially remote thread.
class SyncSocketSource : public AudioOutputStream::AudioSourceCallback {
 public:
  SyncSocketSource(base::SyncSocket* socket, const AudioParameters& params)
      : socket_(socket) {
    // Setup AudioBus wrapping data we'll receive over the sync socket.
    data_size_ = AudioBus::CalculateMemorySize(params);
    data_.reset(static_cast<float*>(
        base::AlignedAlloc(data_size_, AudioBus::kChannelAlignment)));
    audio_bus_ = AudioBus::WrapMemory(params, data_.get());
  }
  ~SyncSocketSource() {}

  // AudioSourceCallback::OnMoreData implementation:
  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState buffers_state) {
    socket_->Send(&buffers_state, sizeof(buffers_state));
    uint32 size = socket_->Receive(data_.get(), data_size_);
    DCHECK_EQ(static_cast<size_t>(size) % sizeof(*audio_bus_->channel(0)), 0U);
    audio_bus_->CopyTo(audio_bus);
    return audio_bus_->frames();
  }
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) {
    NOTREACHED();
    return 0;
  }
  // AudioSourceCallback::OnError implementation:
  virtual void OnError(AudioOutputStream* stream) {
  }

 private:
  base::SyncSocket* socket_;
  int data_size_;
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> data_;
  scoped_ptr<AudioBus> audio_bus_;
};

struct SyncThreadContext {
  base::SyncSocket* socket;
  int sample_rate;
  int channels;
  int frames;
  double sine_freq;
  uint32 packet_size_bytes;
};

// This thread provides the data that the SyncSocketSource above needs
// using the other end of a SyncSocket. The protocol is as follows:
//
// SyncSocketSource ---send 4 bytes ------------> SyncSocketThread
//                  <--- audio packet ----------
//
DWORD __stdcall SyncSocketThread(void* context) {
  SyncThreadContext& ctx = *(reinterpret_cast<SyncThreadContext*>(context));

  // Setup AudioBus wrapping data we'll pass over the sync socket.
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> data(static_cast<float*>(
      base::AlignedAlloc(ctx.packet_size_bytes, AudioBus::kChannelAlignment)));
  scoped_ptr<AudioBus> audio_bus = AudioBus::WrapMemory(
      ctx.channels, ctx.frames, data.get());

  SineWaveAudioSource sine(1, ctx.sine_freq, ctx.sample_rate);
  const int kTwoSecFrames = ctx.sample_rate * 2;

  AudioBuffersState buffers_state;
  int times = 0;
  for (int ix = 0; ix < kTwoSecFrames; ix += ctx.frames) {
    if (ctx.socket->Receive(&buffers_state, sizeof(buffers_state)) == 0)
      break;
    if ((times > 0) && (buffers_state.pending_bytes < 1000)) __debugbreak();
    sine.OnMoreData(audio_bus.get(), buffers_state);
    ctx.socket->Send(data.get(), ctx.packet_size_bytes);
    ++times;
  }

  return 0;
}

// Test the basic operation of AudioOutputStream used with a SyncSocket.
// The emphasis is to verify that it is possible to feed data to the audio
// layer using a source based on SyncSocket. In a real situation we would
// go for the low-latency version in combination with SyncSocket, but to keep
// the test more simple, AUDIO_PCM_LINEAR is utilized instead. The main
// principle of the test still remains and we avoid the additional complexity
// related to the two different audio-layers for AUDIO_PCM_LOW_LATENCY.
// In this test you should hear a continuous 200Hz tone for 2 seconds.
TEST(WinAudioTest, SyncSocketBasic) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  static const int sample_rate = AudioParameters::kAudioCDSampleRate;
  static const uint32 kSamples20ms = sample_rate / 50;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         CHANNEL_LAYOUT_MONO, sample_rate, 16, kSamples20ms);


  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(params,
      std::string(), std::string());
  ASSERT_TRUE(NULL != oas);

  ASSERT_TRUE(oas->Open());

  base::SyncSocket sockets[2];
  ASSERT_TRUE(base::SyncSocket::CreatePair(&sockets[0], &sockets[1]));

  SyncSocketSource source(&sockets[0], params);

  SyncThreadContext thread_context;
  thread_context.sample_rate = params.sample_rate();
  thread_context.sine_freq = 200.0;
  thread_context.packet_size_bytes = AudioBus::CalculateMemorySize(params);
  thread_context.frames = params.frames_per_buffer();
  thread_context.channels = params.channels();
  thread_context.socket = &sockets[1];

  HANDLE thread = ::CreateThread(NULL, 0, SyncSocketThread,
                                 &thread_context, 0, NULL);

  oas->Start(&source);

  ::WaitForSingleObject(thread, INFINITE);
  ::CloseHandle(thread);

  oas->Stop();
  oas->Close();
}

}  // namespace media
