// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/texture_mailbox.h"

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

TextureMailbox::TextureMailbox()
    : target_(GL_TEXTURE_2D),
      sync_point_(0),
      shared_memory_(NULL) {
}

TextureMailbox::TextureMailbox(const std::string& mailbox_name)
    : target_(GL_TEXTURE_2D),
      sync_point_(0),
      shared_memory_(NULL) {
  if (!mailbox_name.empty()) {
    CHECK(mailbox_name.size() == sizeof(name_.name));
    name_.SetName(reinterpret_cast<const int8*>(mailbox_name.data()));
  }
}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox_name)
    : target_(GL_TEXTURE_2D),
      sync_point_(0),
      shared_memory_(NULL) {
  name_.SetName(mailbox_name.name);
}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox_name,
                               unsigned sync_point)
    : target_(GL_TEXTURE_2D),
      sync_point_(sync_point),
      shared_memory_(NULL) {
  name_.SetName(mailbox_name.name);
}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox_name,
                               unsigned texture_target,
                               unsigned sync_point)
    : target_(texture_target),
      sync_point_(sync_point),
      shared_memory_(NULL) {
  name_.SetName(mailbox_name.name);
}

TextureMailbox::TextureMailbox(base::SharedMemory* shared_memory,
                               gfx::Size size)
    : target_(GL_TEXTURE_2D),
      sync_point_(0),
      shared_memory_(shared_memory),
      shared_memory_size_(size) {}

TextureMailbox::~TextureMailbox() {}

bool TextureMailbox::Equals(const TextureMailbox& other) const {
  if (other.IsTexture())
    return ContainsMailbox(other.name());
  else if (other.IsSharedMemory())
    return ContainsHandle(other.shared_memory_->handle());

  DCHECK(!other.IsValid());
  return !IsValid();
}

bool TextureMailbox::ContainsMailbox(const gpu::Mailbox& other) const {
  return IsTexture() && !memcmp(data(), other.name, sizeof(name_.name));
}

bool TextureMailbox::ContainsHandle(base::SharedMemoryHandle handle) const {
  return shared_memory_ && shared_memory_->handle() == handle;
}

void TextureMailbox::SetName(const gpu::Mailbox& name) {
  DCHECK(shared_memory_ == NULL);
  name_ = name;
}

size_t TextureMailbox::shared_memory_size_in_bytes() const {
  return 4 * shared_memory_size_.GetArea();
}

}  // namespace cc
