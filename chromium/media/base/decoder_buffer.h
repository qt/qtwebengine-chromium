// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include <string>

#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"

namespace media {

// A specialized buffer for interfacing with audio / video decoders.
//
// Specifically ensures that data is aligned and padded as necessary by the
// underlying decoding framework.  On desktop platforms this means memory is
// allocated using FFmpeg with particular alignment and padding requirements.
//
// Also includes decoder specific functionality for decryption.
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  enum {
    kPaddingSize = 16,
#if defined(ARCH_CPU_ARM_FAMILY)
    kAlignmentSize = 16
#else
    kAlignmentSize = 32
#endif
  };

  // Allocates buffer with |size| >= 0.  Buffer will be padded and aligned
  // as necessary.
  explicit DecoderBuffer(int size);

  // Create a DecoderBuffer whose |data_| is copied from |data|.  Buffer will be
  // padded and aligned as necessary.  |data| must not be NULL and |size| >= 0.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8* data, int size);

  // Create a DecoderBuffer whose |data_| is copied from |data| and |side_data_|
  // is copied from |side_data|. Buffers will be padded and aligned as necessary
  // Data pointers must not be NULL and sizes must be >= 0.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8* data, int size,
                                               const uint8* side_data,
                                               int side_data_size);

  // Create a DecoderBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  base::TimeDelta timestamp() const {
    DCHECK(!end_of_stream());
    return timestamp_;
  }

  void set_timestamp(const base::TimeDelta& timestamp) {
    DCHECK(!end_of_stream());
    timestamp_ = timestamp;
  }

  base::TimeDelta duration() const {
    DCHECK(!end_of_stream());
    return duration_;
  }

  void set_duration(const base::TimeDelta& duration) {
    DCHECK(!end_of_stream());
    duration_ = duration;
  }

  const uint8* data() const {
    DCHECK(!end_of_stream());
    return data_.get();
  }

  uint8* writable_data() const {
    DCHECK(!end_of_stream());
    return data_.get();
  }

  int data_size() const {
    DCHECK(!end_of_stream());
    return size_;
  }

  const uint8* side_data() const {
    DCHECK(!end_of_stream());
    return side_data_.get();
  }

  int side_data_size() const {
    DCHECK(!end_of_stream());
    return side_data_size_;
  }

  base::TimeDelta discard_padding() const {
    DCHECK(!end_of_stream());
    return discard_padding_;
  }

  void set_discard_padding(const base::TimeDelta discard_padding) {
    DCHECK(!end_of_stream());
    discard_padding_ = discard_padding;
  }

  const DecryptConfig* decrypt_config() const {
    DCHECK(!end_of_stream());
    return decrypt_config_.get();
  }

  void set_decrypt_config(scoped_ptr<DecryptConfig> decrypt_config) {
    DCHECK(!end_of_stream());
    decrypt_config_ = decrypt_config.Pass();
  }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const {
    return data_ == NULL;
  }

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString();

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;

  // Allocates a buffer of size |size| >= 0 and copies |data| into it.  Buffer
  // will be padded and aligned as necessary.  If |data| is NULL then |data_| is
  // set to NULL and |buffer_size_| to 0.
  DecoderBuffer(const uint8* data, int size,
                const uint8* side_data, int side_data_size);
  virtual ~DecoderBuffer();

 private:
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  int size_;
  scoped_ptr<uint8, base::ScopedPtrAlignedFree> data_;
  int side_data_size_;
  scoped_ptr<uint8, base::ScopedPtrAlignedFree> side_data_;
  scoped_ptr<DecryptConfig> decrypt_config_;
  base::TimeDelta discard_padding_;

  // Constructor helper method for memory allocations.
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(DecoderBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
