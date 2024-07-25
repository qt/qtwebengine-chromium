// Copyright 2024 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FDRM_FX_CRYPT_SHA_H_
#define CORE_FDRM_FX_CRYPT_SHA_H_

#include <stdint.h>

struct CRYPT_sha1_context {
  uint64_t total_bytes;
  uint32_t blkused;  // Constrained to [0, 64).
  uint32_t h[5];
  uint8_t block[64];
};

struct CRYPT_sha2_context {
  uint64_t total_bytes;
  uint64_t state[8];
  uint8_t buffer[128];
};

void CRYPT_SHA1Start(CRYPT_sha1_context* context);
void CRYPT_SHA1Update(CRYPT_sha1_context* context,
                      const uint8_t* data,
                      uint32_t size);
void CRYPT_SHA1Finish(CRYPT_sha1_context* context, uint8_t digest[20]);
void CRYPT_SHA1Generate(const uint8_t* data, uint32_t size, uint8_t digest[20]);

void CRYPT_SHA256Start(CRYPT_sha2_context* context);
void CRYPT_SHA256Update(CRYPT_sha2_context* context,
                        const uint8_t* data,
                        uint32_t size);
void CRYPT_SHA256Finish(CRYPT_sha2_context* context, uint8_t digest[32]);
void CRYPT_SHA256Generate(const uint8_t* data,
                          uint32_t size,
                          uint8_t digest[32]);

void CRYPT_SHA384Start(CRYPT_sha2_context* context);
void CRYPT_SHA384Update(CRYPT_sha2_context* context,
                        const uint8_t* data,
                        uint32_t size);
void CRYPT_SHA384Finish(CRYPT_sha2_context* context, uint8_t digest[48]);
void CRYPT_SHA384Generate(const uint8_t* data,
                          uint32_t size,
                          uint8_t digest[48]);

void CRYPT_SHA512Start(CRYPT_sha2_context* context);
void CRYPT_SHA512Update(CRYPT_sha2_context* context,
                        const uint8_t* data,
                        uint32_t size);
void CRYPT_SHA512Finish(CRYPT_sha2_context* context, uint8_t digest[64]);
void CRYPT_SHA512Generate(const uint8_t* data,
                          uint32_t size,
                          uint8_t digest[64]);

#endif  // CORE_FDRM_FX_CRYPT_SHA_H_
