// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCODEC_CODEC_MEMORY_SK_STREAM_H_
#define CORE_FXCODEC_CODEC_MEMORY_SK_STREAM_H_

#include <stddef.h>

#include "core/fxcodec/cfx_codec_memory.h"
#include "core/fxcrt/retain_ptr.h"
#include "third_party/skia/include/core/SkStream.h"

// Implements/exposes `SkStream` API on top of `CFX_CodecMemory`.
class CodecMemorySkStream final : public SkStream {
 public:
  explicit CodecMemorySkStream(RetainPtr<CFX_CodecMemory> codec_memory);
  ~CodecMemorySkStream() override;

  size_t read(void* buffer, size_t size) override;
  bool isAtEnd() const override;
  bool rewind() override;
  bool seek(size_t position) override;
  size_t getPosition() const override;
  size_t getLength() const override;
  bool hasPosition() const override;
  bool hasLength() const override;

 private:
  RetainPtr<CFX_CodecMemory> const codec_memory_;
};

#endif  // CORE_FXCODEC_CODEC_MEMORY_SK_STREAM_H_
