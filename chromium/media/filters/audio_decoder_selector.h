// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_DECODER_SELECTOR_H_
#define MEDIA_FILTERS_AUDIO_DECODER_SELECTOR_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class AudioDecoder;
class DecoderBuffer;
class DecryptingDemuxerStream;
class DemuxerStream;

// AudioDecoderSelector (creates if necessary and) initializes the proper
// AudioDecoder for a given DemuxerStream. If the given DemuxerStream is
// encrypted, a DecryptingDemuxerStream may also be created.
class MEDIA_EXPORT AudioDecoderSelector {
 public:
  // Indicates completion of AudioDecoder selection.
  // - First parameter: The initialized AudioDecoder. If it's set to NULL, then
  // AudioDecoder initialization failed.
  // - Second parameter: The initialized DecryptingDemuxerStream. If it's not
  // NULL, then a DecryptingDemuxerStream is created and initialized to do
  // decryption for the initialized AudioDecoder.
  // Note: The caller owns selected AudioDecoder and DecryptingDemuxerStream.
  // The caller should call DecryptingDemuxerStream::Reset() before
  // calling AudioDecoder::Reset() to release any pending decryption or read.
  typedef base::Callback<
      void(scoped_ptr<AudioDecoder>,
           scoped_ptr<DecryptingDemuxerStream>)> SelectDecoderCB;

  // |decoders| contains the AudioDecoders to use when initializing.
  //
  // |set_decryptor_ready_cb| is optional. If |set_decryptor_ready_cb| is null,
  // no decryptor will be available to perform decryption.
  AudioDecoderSelector(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      ScopedVector<AudioDecoder> decoders,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  ~AudioDecoderSelector();

  // Initializes and selects an AudioDecoder that can decode the |stream|.
  // Selected AudioDecoder (and DecryptingDemuxerStream) is returned via
  // the |select_decoder_cb|.
  void SelectAudioDecoder(DemuxerStream* stream,
                          const StatisticsCB& statistics_cb,
                          const SelectDecoderCB& select_decoder_cb);

  // Aborts pending AudioDecoder selection and fires |select_decoder_cb| with
  // NULL and NULL immediately if it's pending.
  void Abort();

 private:
  void DecryptingAudioDecoderInitDone(PipelineStatus status);
  void DecryptingDemuxerStreamInitDone(PipelineStatus status);
  void InitializeDecoder();
  void DecoderInitDone(PipelineStatus status);
  void ReturnNullDecoder();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ScopedVector<AudioDecoder> decoders_;
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  DemuxerStream* input_stream_;
  StatisticsCB statistics_cb_;
  SelectDecoderCB select_decoder_cb_;

  scoped_ptr<AudioDecoder> audio_decoder_;
  scoped_ptr<DecryptingDemuxerStream> decrypted_stream_;

  base::WeakPtrFactory<AudioDecoderSelector> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioDecoderSelector);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_DECODER_SELECTOR_H_
