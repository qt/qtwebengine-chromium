// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_request.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// static
scoped_ptr<CopyOutputRequest> CopyOutputRequest::CreateRelayRequest(
    const CopyOutputRequest& original_request,
    const CopyOutputRequestCallback& result_callback) {
  scoped_ptr<CopyOutputRequest> relay = CreateRequest(result_callback);
  relay->force_bitmap_result_ = original_request.force_bitmap_result_;
  relay->has_area_ = original_request.has_area_;
  relay->area_ = original_request.area_;
  relay->has_texture_mailbox_ = original_request.has_texture_mailbox_;
  relay->texture_mailbox_ = original_request.texture_mailbox_;
  return relay.Pass();
}

CopyOutputRequest::CopyOutputRequest() {}

CopyOutputRequest::CopyOutputRequest(
    bool force_bitmap_result,
    const CopyOutputRequestCallback& result_callback)
    : force_bitmap_result_(force_bitmap_result),
      has_area_(false),
      has_texture_mailbox_(false),
      result_callback_(result_callback) {}

CopyOutputRequest::~CopyOutputRequest() {
  if (!result_callback_.is_null())
    SendResult(CopyOutputResult::CreateEmptyResult().Pass());
}

void CopyOutputRequest::SendResult(scoped_ptr<CopyOutputResult> result) {
  base::ResetAndReturn(&result_callback_).Run(result.Pass());
}

void CopyOutputRequest::SendEmptyResult() {
  SendResult(CopyOutputResult::CreateEmptyResult().Pass());
}

void CopyOutputRequest::SendBitmapResult(scoped_ptr<SkBitmap> bitmap) {
  SendResult(CopyOutputResult::CreateBitmapResult(bitmap.Pass()).Pass());
}

void CopyOutputRequest::SendTextureResult(
    gfx::Size size,
    const TextureMailbox& texture_mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback) {
  DCHECK(texture_mailbox.IsTexture());
  SendResult(CopyOutputResult::CreateTextureResult(
      size, texture_mailbox, release_callback.Pass()));
}

void CopyOutputRequest::SetTextureMailbox(
    const TextureMailbox& texture_mailbox) {
  DCHECK(!force_bitmap_result_);
  DCHECK(texture_mailbox.IsTexture());
  has_texture_mailbox_ = true;
  texture_mailbox_ = texture_mailbox;
}

}  // namespace cc
