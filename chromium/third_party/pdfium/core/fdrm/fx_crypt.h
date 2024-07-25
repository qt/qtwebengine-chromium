// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FDRM_FX_CRYPT_H_
#define CORE_FDRM_FX_CRYPT_H_

#include <stdint.h>

#include "core/fdrm/fx_crypt_aes.h"
#include "core/fdrm/fx_crypt_sha.h"
#include "core/fxcrt/span.h"

struct CRYPT_rc4_context {
  static constexpr int32_t kPermutationLength = 256;

  int32_t x;
  int32_t y;
  int32_t m[kPermutationLength];
};

struct CRYPT_md5_context {
  uint32_t total[2];
  uint32_t state[4];
  uint8_t buffer[64];
};


void CRYPT_ArcFourCryptBlock(pdfium::span<uint8_t> data,
                             pdfium::span<const uint8_t> key);
void CRYPT_ArcFourSetup(CRYPT_rc4_context* context,
                        pdfium::span<const uint8_t> key);
void CRYPT_ArcFourCrypt(CRYPT_rc4_context* context, pdfium::span<uint8_t> data);

CRYPT_md5_context CRYPT_MD5Start();
void CRYPT_MD5Update(CRYPT_md5_context* context,
                     pdfium::span<const uint8_t> data);
void CRYPT_MD5Finish(CRYPT_md5_context* context, uint8_t digest[16]);
void CRYPT_MD5Generate(pdfium::span<const uint8_t> data, uint8_t digest[16]);

#endif  // CORE_FDRM_FX_CRYPT_H_
