// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_source_delegate.h"

#include <limits>
#include <string>
#include <vector>

#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/media/webmediasource_impl.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

using media::DemuxerStream;
using media::DemuxerConfigs;
using media::DemuxerData;
using blink::WebMediaPlayer;
using blink::WebString;

namespace {

// The size of the access unit to transfer in an IPC in case of MediaSource.
// 16: approximately 250ms of content in 60 fps movies.
const size_t kAccessUnitSizeForMediaSource = 16;

const uint8 kVorbisPadding[] = { 0xff, 0xff, 0xff, 0xff };

}  // namespace

namespace content {

static void LogMediaSourceError(const scoped_refptr<media::MediaLog>& media_log,
                                const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}

MediaSourceDelegate::MediaSourceDelegate(
    RendererDemuxerAndroid* demuxer_client,
    int demuxer_client_id,
    const scoped_refptr<base::MessageLoopProxy>& media_loop,
    media::MediaLog* media_log)
    : main_loop_(base::MessageLoopProxy::current()),
      main_weak_factory_(this),
      main_weak_this_(main_weak_factory_.GetWeakPtr()),
      media_loop_(media_loop),
      media_weak_factory_(this),
      demuxer_client_(demuxer_client),
      demuxer_client_id_(demuxer_client_id),
      media_log_(media_log),
      demuxer_(NULL),
      is_demuxer_ready_(false),
      audio_stream_(NULL),
      video_stream_(NULL),
      seeking_(false),
      doing_browser_seek_(false),
      browser_seek_time_(media::kNoTimestamp()),
      expecting_regular_seek_(false),
#if defined(GOOGLE_TV)
      key_added_(false),
#endif
      access_unit_size_(0) {
  DCHECK(main_loop_->BelongsToCurrentThread());
}

MediaSourceDelegate::~MediaSourceDelegate() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(!chunk_demuxer_);
  DCHECK(!demuxer_);
  DCHECK(!demuxer_client_);
  DCHECK(!audio_decrypting_demuxer_stream_);
  DCHECK(!video_decrypting_demuxer_stream_);
  DCHECK(!audio_stream_);
  DCHECK(!video_stream_);
}

void MediaSourceDelegate::Destroy() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;

  if (!demuxer_) {
    DCHECK(!demuxer_client_);
    delete this;
    return;
  }

  duration_change_cb_.Reset();
  update_network_state_cb_.Reset();
  media_source_opened_cb_.Reset();

  main_weak_factory_.InvalidateWeakPtrs();
  DCHECK(!main_weak_factory_.HasWeakPtrs());

  if (chunk_demuxer_)
    chunk_demuxer_->Shutdown();

  // |this| will be transferred to the callback StopDemuxer() and
  // OnDemuxerStopDone(). They own |this| and OnDemuxerStopDone() will delete
  // it when called, hence using base::Unretained(this) is safe here.
  media_loop_->PostTask(FROM_HERE,
                        base::Bind(&MediaSourceDelegate::StopDemuxer,
                        base::Unretained(this)));
}

void MediaSourceDelegate::StopDemuxer() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DCHECK(demuxer_);

  demuxer_client_->RemoveDelegate(demuxer_client_id_);
  demuxer_client_ = NULL;

  audio_stream_ = NULL;
  video_stream_ = NULL;
  // TODO(xhwang): Figure out if we need to Reset the DDSs after Seeking or
  // before destroying them.
  audio_decrypting_demuxer_stream_.reset();
  video_decrypting_demuxer_stream_.reset();

  media_weak_factory_.InvalidateWeakPtrs();
  DCHECK(!media_weak_factory_.HasWeakPtrs());

  // The callback OnDemuxerStopDone() owns |this| and will delete it when
  // called. Hence using base::Unretained(this) is safe here.
  demuxer_->Stop(media::BindToLoop(main_loop_,
      base::Bind(&MediaSourceDelegate::OnDemuxerStopDone,
                 base::Unretained(this))));
}

void MediaSourceDelegate::InitializeMediaSource(
    const MediaSourceOpenedCB& media_source_opened_cb,
    const media::Demuxer::NeedKeyCB& need_key_cb,
    const media::SetDecryptorReadyCB& set_decryptor_ready_cb,
    const UpdateNetworkStateCB& update_network_state_cb,
    const DurationChangeCB& duration_change_cb) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK(!media_source_opened_cb.is_null());
  media_source_opened_cb_ = media_source_opened_cb;
  need_key_cb_ = need_key_cb;
  set_decryptor_ready_cb_ = set_decryptor_ready_cb;
  update_network_state_cb_ = media::BindToCurrentLoop(update_network_state_cb);
  duration_change_cb_ = duration_change_cb;
  access_unit_size_ = kAccessUnitSizeForMediaSource;

  chunk_demuxer_.reset(new media::ChunkDemuxer(
      media::BindToCurrentLoop(base::Bind(
          &MediaSourceDelegate::OnDemuxerOpened, main_weak_this_)),
      media::BindToCurrentLoop(base::Bind(
          &MediaSourceDelegate::OnNeedKey, main_weak_this_)),
      base::Bind(&LogMediaSourceError, media_log_)));
  demuxer_ = chunk_demuxer_.get();

  // |this| will be retained until StopDemuxer() is posted, so Unretained() is
  // safe here.
  media_loop_->PostTask(FROM_HERE,
                        base::Bind(&MediaSourceDelegate::InitializeDemuxer,
                        base::Unretained(this)));
}

void MediaSourceDelegate::InitializeDemuxer() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  demuxer_client_->AddDelegate(demuxer_client_id_, this);
  demuxer_->Initialize(this, base::Bind(&MediaSourceDelegate::OnDemuxerInitDone,
                                        media_weak_factory_.GetWeakPtr()),
                       false);
}

#if defined(GOOGLE_TV)
void MediaSourceDelegate::InitializeMediaStream(
    media::Demuxer* demuxer,
    const UpdateNetworkStateCB& update_network_state_cb) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK(demuxer);
  demuxer_ = demuxer;
  update_network_state_cb_ = media::BindToCurrentLoop(update_network_state_cb);
  // When playing Media Stream, don't wait to accumulate multiple packets per
  // IPC communication.
  access_unit_size_ = 1;

  // |this| will be retained until StopDemuxer() is posted, so Unretained() is
  // safe here.
  media_loop_->PostTask(FROM_HERE,
                        base::Bind(&MediaSourceDelegate::InitializeDemuxer,
                        base::Unretained(this)));
}
#endif

const blink::WebTimeRanges& MediaSourceDelegate::Buffered() {
  buffered_web_time_ranges_ =
      ConvertToWebTimeRanges(buffered_time_ranges_);
  return buffered_web_time_ranges_;
}

size_t MediaSourceDelegate::DecodedFrameCount() const {
  return statistics_.video_frames_decoded;
}

size_t MediaSourceDelegate::DroppedFrameCount() const {
  return statistics_.video_frames_dropped;
}

size_t MediaSourceDelegate::AudioDecodedByteCount() const {
  return statistics_.audio_bytes_decoded;
}

size_t MediaSourceDelegate::VideoDecodedByteCount() const {
  return statistics_.video_bytes_decoded;
}

void MediaSourceDelegate::CancelPendingSeek(const base::TimeDelta& seek_time) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ") : "
           << demuxer_client_id_;

  if (!chunk_demuxer_)
    return;

  {
    // Remember to trivially finish any newly arriving browser seek requests
    // that may arrive prior to the next regular seek request.
    base::AutoLock auto_lock(seeking_lock_);
    expecting_regular_seek_ = true;
  }

  // Cancel any previously expected or in-progress regular or browser seek.
  // It is possible that we have just finished the seek, but caller does
  // not know this yet. It is still safe to cancel in this case because the
  // caller will always call StartWaitingForSeek() when it is notified of
  // the finished seek.
  chunk_demuxer_->CancelPendingSeek(seek_time);
}

void MediaSourceDelegate::StartWaitingForSeek(
    const base::TimeDelta& seek_time) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ") : "
           << demuxer_client_id_;

  if (!chunk_demuxer_)
    return;

  bool cancel_browser_seek = false;
  {
    // Remember to trivially finish any newly arriving browser seek requests
    // that may arrive prior to the next regular seek request.
    base::AutoLock auto_lock(seeking_lock_);
    expecting_regular_seek_ = true;

    // Remember to cancel any in-progress browser seek.
    if (seeking_) {
      DCHECK(doing_browser_seek_);
      cancel_browser_seek = true;
    }
  }

  if (cancel_browser_seek)
    chunk_demuxer_->CancelPendingSeek(seek_time);
  chunk_demuxer_->StartWaitingForSeek(seek_time);
}

void MediaSourceDelegate::Seek(
    const base::TimeDelta& seek_time, bool is_browser_seek) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ", "
           << (is_browser_seek ? "browser seek" : "regular seek") << ") : "
           << demuxer_client_id_;

  base::TimeDelta internal_seek_time = seek_time;
  {
    base::AutoLock auto_lock(seeking_lock_);
    DCHECK(!seeking_);
    seeking_ = true;
    doing_browser_seek_ = is_browser_seek;

    if (doing_browser_seek_ && (!chunk_demuxer_ || expecting_regular_seek_)) {
      // Trivially finish the browser seek without actually doing it. Reads will
      // continue to be |kAborted| until the next regular seek is done. Browser
      // seeking is not supported unless using a ChunkDemuxer; browser seeks are
      // trivially finished if |chunk_demuxer_| is NULL.
      seeking_ = false;
      doing_browser_seek_ = false;
      demuxer_client_->DemuxerSeekDone(demuxer_client_id_, seek_time);
      return;
    }

    if (doing_browser_seek_) {
      internal_seek_time = FindBufferedBrowserSeekTime_Locked(seek_time);
      browser_seek_time_ = internal_seek_time;
    } else {
      expecting_regular_seek_ = false;
      browser_seek_time_ = media::kNoTimestamp();
    }
  }

  // Prepare |chunk_demuxer_| for browser seek.
  if (is_browser_seek) {
    chunk_demuxer_->CancelPendingSeek(internal_seek_time);
    chunk_demuxer_->StartWaitingForSeek(internal_seek_time);
  }

  SeekInternal(internal_seek_time);
}

void MediaSourceDelegate::SeekInternal(const base::TimeDelta& seek_time) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DCHECK(IsSeeking());
  demuxer_->Seek(seek_time, base::Bind(
      &MediaSourceDelegate::OnDemuxerSeekDone,
      media_weak_factory_.GetWeakPtr()));
}

void MediaSourceDelegate::SetTotalBytes(int64 total_bytes) {
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::AddBufferedByteRange(int64 start, int64 end) {
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::AddBufferedTimeRange(base::TimeDelta start,
                                               base::TimeDelta end) {
  buffered_time_ranges_.Add(start, end);
}

void MediaSourceDelegate::SetDuration(base::TimeDelta duration) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << duration.InSecondsF() << ") : "
           << demuxer_client_id_;

  // Force duration change notification to be async to avoid reentrancy into
  // ChunkDemxuer.
  main_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaSourceDelegate::OnDurationChanged, main_weak_this_, duration));
}

void MediaSourceDelegate::OnDurationChanged(const base::TimeDelta& duration) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  if (demuxer_client_)
    demuxer_client_->DurationChanged(demuxer_client_id_, duration);
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run(duration);
}

void MediaSourceDelegate::OnReadFromDemuxer(media::DemuxerStream::Type type) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << type << ") : " << demuxer_client_id_;
  if (IsSeeking())
    return;  // Drop the request during seeking.

  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO);
  // The access unit size should have been initialized properly at this stage.
  DCHECK_GT(access_unit_size_, 0u);
  scoped_ptr<DemuxerData> data(new DemuxerData());
  data->type = type;
  data->access_units.resize(access_unit_size_);
  ReadFromDemuxerStream(type, data.Pass(), 0);
}

void MediaSourceDelegate::ReadFromDemuxerStream(media::DemuxerStream::Type type,
                                                scoped_ptr<DemuxerData> data,
                                                size_t index) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  // DemuxerStream::Read() always returns the read callback asynchronously.
  DemuxerStream* stream =
      (type == DemuxerStream::AUDIO) ? audio_stream_ : video_stream_;
  stream->Read(base::Bind(
      &MediaSourceDelegate::OnBufferReady,
      media_weak_factory_.GetWeakPtr(), type, base::Passed(&data), index));
}

void MediaSourceDelegate::OnBufferReady(
    media::DemuxerStream::Type type,
    scoped_ptr<DemuxerData> data,
    size_t index,
    DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << index << ", " << status << ", "
           << ((!buffer || buffer->end_of_stream()) ?
               -1 : buffer->timestamp().InMilliseconds())
           << ") : " << demuxer_client_id_;
  DCHECK(demuxer_);

  // No new OnReadFromDemuxer() will be called during seeking. So this callback
  // must be from previous OnReadFromDemuxer() call and should be ignored.
  if (IsSeeking()) {
    DVLOG(1) << __FUNCTION__ << ": Ignore previous read during seeking.";
    return;
  }

  bool is_audio = (type == DemuxerStream::AUDIO);
  if (status != DemuxerStream::kAborted &&
      index >= data->access_units.size()) {
    LOG(ERROR) << "The internal state inconsistency onBufferReady: "
               << (is_audio ? "Audio" : "Video") << ", index " << index
               << ", size " << data->access_units.size()
               << ", status " << static_cast<int>(status);
    NOTREACHED();
    return;
  }

  switch (status) {
    case DemuxerStream::kAborted:
      DVLOG(1) << __FUNCTION__ << " : Aborted";
      data->access_units[index].status = status;
      data->access_units.resize(index + 1);
      break;

    case DemuxerStream::kConfigChanged:
      // In case of kConfigChanged, need to read decoder_config once
      // for the next reads.
      // TODO(kjyoun): Investigate if we need to use this new config. See
      // http://crbug.com/255783
      if (is_audio) {
        audio_stream_->audio_decoder_config();
      } else {
        gfx::Size size = video_stream_->video_decoder_config().coded_size();
        DVLOG(1) << "Video config is changed: " << size.width() << "x"
                 << size.height();
      }
      data->access_units[index].status = status;
      data->access_units.resize(index + 1);
      break;

    case DemuxerStream::kOk:
      data->access_units[index].status = status;
      if (buffer->end_of_stream()) {
        data->access_units[index].end_of_stream = true;
        data->access_units.resize(index + 1);
        break;
      }
      // TODO(ycheo): We assume that the inputed stream will be decoded
      // right away.
      // Need to implement this properly using MediaPlayer.OnInfoListener.
      if (is_audio) {
        statistics_.audio_bytes_decoded += buffer->data_size();
      } else {
        statistics_.video_bytes_decoded += buffer->data_size();
        statistics_.video_frames_decoded++;
      }
      data->access_units[index].timestamp = buffer->timestamp();

      {  // No local variable in switch-case scope.
        int data_offset = buffer->decrypt_config() ?
            buffer->decrypt_config()->data_offset() : 0;
        DCHECK_LT(data_offset, buffer->data_size());
        data->access_units[index].data = std::vector<uint8>(
            buffer->data() + data_offset,
            buffer->data() + buffer->data_size() - data_offset);
      }
#if !defined(GOOGLE_TV)
      // Vorbis needs 4 extra bytes padding on Android. Check
      // NuMediaExtractor.cpp in Android source code.
      if (is_audio && media::kCodecVorbis ==
          audio_stream_->audio_decoder_config().codec()) {
        data->access_units[index].data.insert(
            data->access_units[index].data.end(), kVorbisPadding,
            kVorbisPadding + 4);
      }
#endif
      if (buffer->decrypt_config()) {
        data->access_units[index].key_id = std::vector<char>(
            buffer->decrypt_config()->key_id().begin(),
            buffer->decrypt_config()->key_id().end());
        data->access_units[index].iv = std::vector<char>(
            buffer->decrypt_config()->iv().begin(),
            buffer->decrypt_config()->iv().end());
        data->access_units[index].subsamples =
            buffer->decrypt_config()->subsamples();
      }
      if (++index < data->access_units.size()) {
        ReadFromDemuxerStream(type, data.Pass(), index);
        return;
      }
      break;

    default:
      NOTREACHED();
  }

  if (!IsSeeking() && demuxer_client_)
    demuxer_client_->ReadFromDemuxerAck(demuxer_client_id_, *data);
}

void MediaSourceDelegate::OnDemuxerError(media::PipelineStatus status) {
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  // |update_network_state_cb_| is bound to the main thread.
  if (status != media::PIPELINE_OK && !update_network_state_cb_.is_null())
    update_network_state_cb_.Run(PipelineErrorToNetworkState(status));
}

void MediaSourceDelegate::AddTextStream(
    media::DemuxerStream* /* text_stream */ ,
    const media::TextTrackConfig& /* config */ ) {
  // TODO(matthewjheaney): add text stream (http://crbug/322115).
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::RemoveTextStream(
    media::DemuxerStream* /* text_stream */ ) {
  // TODO(matthewjheaney): remove text stream (http://crbug/322115).
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::OnDemuxerInitDone(media::PipelineStatus status) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(demuxer_);

  if (status != media::PIPELINE_OK) {
    OnDemuxerError(status);
    return;
  }

  audio_stream_ = demuxer_->GetStream(DemuxerStream::AUDIO);
  video_stream_ = demuxer_->GetStream(DemuxerStream::VIDEO);

  if (audio_stream_ && audio_stream_->audio_decoder_config().is_encrypted() &&
      !set_decryptor_ready_cb_.is_null()) {
    InitAudioDecryptingDemuxerStream();
    // InitVideoDecryptingDemuxerStream() will be called in
    // OnAudioDecryptingDemuxerStreamInitDone().
    return;
  }

  if (video_stream_ && video_stream_->video_decoder_config().is_encrypted() &&
      !set_decryptor_ready_cb_.is_null()) {
    InitVideoDecryptingDemuxerStream();
    return;
  }

  // Notify demuxer ready when both streams are not encrypted.
  is_demuxer_ready_ = true;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::InitAudioDecryptingDemuxerStream() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(!set_decryptor_ready_cb_.is_null());

  audio_decrypting_demuxer_stream_.reset(new media::DecryptingDemuxerStream(
      media_loop_, set_decryptor_ready_cb_));
  audio_decrypting_demuxer_stream_->Initialize(
      audio_stream_,
      base::Bind(&MediaSourceDelegate::OnAudioDecryptingDemuxerStreamInitDone,
                 media_weak_factory_.GetWeakPtr()));
}

void MediaSourceDelegate::InitVideoDecryptingDemuxerStream() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(!set_decryptor_ready_cb_.is_null());

  video_decrypting_demuxer_stream_.reset(new media::DecryptingDemuxerStream(
      media_loop_, set_decryptor_ready_cb_));
  video_decrypting_demuxer_stream_->Initialize(
      video_stream_,
      base::Bind(&MediaSourceDelegate::OnVideoDecryptingDemuxerStreamInitDone,
                 media_weak_factory_.GetWeakPtr()));
}

void MediaSourceDelegate::OnAudioDecryptingDemuxerStreamInitDone(
    media::PipelineStatus status) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(demuxer_);

  if (status != media::PIPELINE_OK)
    audio_decrypting_demuxer_stream_.reset();
  else
    audio_stream_ = audio_decrypting_demuxer_stream_.get();

  if (video_stream_ && video_stream_->video_decoder_config().is_encrypted()) {
    InitVideoDecryptingDemuxerStream();
    return;
  }

  // Try to notify demuxer ready when audio DDS initialization finished and
  // video is not encrypted.
  is_demuxer_ready_ = true;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::OnVideoDecryptingDemuxerStreamInitDone(
    media::PipelineStatus status) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(demuxer_);

  if (status != media::PIPELINE_OK)
    video_decrypting_demuxer_stream_.reset();
  else
    video_stream_ = video_decrypting_demuxer_stream_.get();

  // Try to notify demuxer ready when video DDS initialization finished.
  is_demuxer_ready_ = true;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

void MediaSourceDelegate::OnDemuxerSeekDone(media::PipelineStatus status) {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(IsSeeking());

  if (status != media::PIPELINE_OK) {
    OnDemuxerError(status);
    return;
  }

  ResetAudioDecryptingDemuxerStream();
}

void MediaSourceDelegate::ResetAudioDecryptingDemuxerStream() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  if (audio_decrypting_demuxer_stream_) {
    audio_decrypting_demuxer_stream_->Reset(
        base::Bind(&MediaSourceDelegate::ResetVideoDecryptingDemuxerStream,
                   media_weak_factory_.GetWeakPtr()));
    return;
  }

  ResetVideoDecryptingDemuxerStream();
}

void MediaSourceDelegate::ResetVideoDecryptingDemuxerStream() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  if (video_decrypting_demuxer_stream_) {
    video_decrypting_demuxer_stream_->Reset(base::Bind(
        &MediaSourceDelegate::FinishResettingDecryptingDemuxerStreams,
        media_weak_factory_.GetWeakPtr()));
    return;
  }

  FinishResettingDecryptingDemuxerStreams();
}

void MediaSourceDelegate::FinishResettingDecryptingDemuxerStreams() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;

  base::AutoLock auto_lock(seeking_lock_);
  DCHECK(seeking_);
  seeking_ = false;
  doing_browser_seek_ = false;
  demuxer_client_->DemuxerSeekDone(demuxer_client_id_, browser_seek_time_);
}

void MediaSourceDelegate::OnDemuxerStopDone() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  chunk_demuxer_.reset();
  demuxer_ = NULL;
  delete this;
}

void MediaSourceDelegate::OnMediaConfigRequest() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  if (CanNotifyDemuxerReady())
    NotifyDemuxerReady();
}

#if defined(GOOGLE_TV)
// TODO(kjyoun): Enhance logic to detect when to call NotifyDemuxerReady()
// For now, we call it when the first key is added. See http://crbug.com/255781
void MediaSourceDelegate::NotifyKeyAdded(const std::string& key_system) {
  if (!media_loop_->BelongsToCurrentThread()) {
    media_loop_->PostTask(FROM_HERE,
        base::Bind(&MediaSourceDelegate::NotifyKeyAdded,
                   base::Unretained(this), key_system));
    return;
  }
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  if (key_added_)
    return;
  key_added_ = true;
  key_system_ = key_system;
  if (!CanNotifyDemuxerReady())
    return;
  if (HasEncryptedStream())
    NotifyDemuxerReady();
}
#endif  // defined(GOOGLE_TV)

bool MediaSourceDelegate::CanNotifyDemuxerReady() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  // This can happen when a key is added before the demuxer is initialized.
  // See NotifyKeyAdded().
  // TODO(kjyoun): Remove NotifyDemxuerReady() call from NotifyKeyAdded() so
  // that we can remove all is_demuxer_ready_/key_added_/key_system_ madness.
  // See http://crbug.com/255781
  if (!is_demuxer_ready_)
    return false;
#if defined(GOOGLE_TV)
  if (HasEncryptedStream() && !key_added_)
    return false;
#endif  // defined(GOOGLE_TV)
  return true;
}

void MediaSourceDelegate::NotifyDemuxerReady() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(CanNotifyDemuxerReady());

  scoped_ptr<DemuxerConfigs> configs(new DemuxerConfigs());
  if (audio_stream_) {
    media::AudioDecoderConfig config = audio_stream_->audio_decoder_config();
    configs->audio_codec = config.codec();
    configs->audio_channels =
        media::ChannelLayoutToChannelCount(config.channel_layout());
    configs->audio_sampling_rate = config.samples_per_second();
    configs->is_audio_encrypted = config.is_encrypted();
    configs->audio_extra_data = std::vector<uint8>(
        config.extra_data(), config.extra_data() + config.extra_data_size());
  }
  if (video_stream_) {
    media::VideoDecoderConfig config = video_stream_->video_decoder_config();
    configs->video_codec = config.codec();
    configs->video_size = config.natural_size();
    configs->is_video_encrypted = config.is_encrypted();
    configs->video_extra_data = std::vector<uint8>(
        config.extra_data(), config.extra_data() + config.extra_data_size());
  }
  configs->duration_ms = GetDurationMs();

#if defined(GOOGLE_TV)
  configs->key_system = HasEncryptedStream() ? key_system_ : "";
#endif

  if (demuxer_client_)
    demuxer_client_->DemuxerReady(demuxer_client_id_, *configs);
}

int MediaSourceDelegate::GetDurationMs() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  if (!chunk_demuxer_)
    return -1;

  double duration_ms = chunk_demuxer_->GetDuration() * 1000;
  if (duration_ms > std::numeric_limits<int32>::max()) {
    LOG(WARNING) << "Duration from ChunkDemuxer is too large; probably "
                    "something has gone wrong.";
    return std::numeric_limits<int32>::max();
  }
  return duration_ms;
}

void MediaSourceDelegate::OnDemuxerOpened() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  if (media_source_opened_cb_.is_null())
    return;

  media_source_opened_cb_.Run(new WebMediaSourceImpl(
      chunk_demuxer_.get(), base::Bind(&LogMediaSourceError, media_log_)));
}

void MediaSourceDelegate::OnNeedKey(const std::string& type,
                                    const std::vector<uint8>& init_data) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  if (need_key_cb_.is_null())
    return;

  need_key_cb_.Run(type, init_data);
}

bool MediaSourceDelegate::HasEncryptedStream() {
  DCHECK(media_loop_->BelongsToCurrentThread());
  return (audio_stream_ &&
          audio_stream_->audio_decoder_config().is_encrypted()) ||
         (video_stream_ &&
          video_stream_->video_decoder_config().is_encrypted());
}

bool MediaSourceDelegate::IsSeeking() const {
  base::AutoLock auto_lock(seeking_lock_);
  return seeking_;
}

base::TimeDelta MediaSourceDelegate::FindBufferedBrowserSeekTime_Locked(
    const base::TimeDelta& seek_time) const {
  seeking_lock_.AssertAcquired();
  DCHECK(seeking_);
  DCHECK(doing_browser_seek_);
  DCHECK(chunk_demuxer_) << "Browser seek requested, but no chunk demuxer";

  media::Ranges<base::TimeDelta> buffered =
      chunk_demuxer_->GetBufferedRanges();

  for (size_t i = 0; i < buffered.size(); ++i) {
    base::TimeDelta range_start = buffered.start(i);
    base::TimeDelta range_end = buffered.end(i);
    if (range_start <= seek_time) {
      if (range_end >= seek_time)
        return seek_time;
      continue;
    }

    // If the start of the next buffered range after |seek_time| is too far
    // into the future, do not jump forward.
    if ((range_start - seek_time) > base::TimeDelta::FromMilliseconds(100))
      break;

    // TODO(wolenetz): Remove possibility that this browser seek jumps
    // into future when the requested range is unbuffered but there is some
    // other buffered range after it. See http://crbug.com/304234.
    return range_start;
  }

  // We found no range containing |seek_time| or beginning shortly after
  // |seek_time|. While possible that such data at and beyond the player's
  // current time have been garbage collected or removed by the web app, this is
  // unlikely. This may cause unexpected playback stall due to seek pending an
  // append for a GOP prior to the last GOP demuxed.
  // TODO(wolenetz): Remove the possibility for this seek to cause unexpected
  // player stall by replaying cached data since last keyframe in browser player
  // rather than issuing browser seek. See http://crbug.com/304234.
  return seek_time;
}

}  // namespace content
