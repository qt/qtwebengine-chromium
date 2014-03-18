// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_sandbox_support_impl_linux.h"

#include <sys/stat.h>

#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/safe_numerics.h"
#include "base/sys_byteorder.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "third_party/WebKit/public/platform/linux/WebFontFamily.h"
#include "third_party/WebKit/public/platform/linux/WebFontRenderStyle.h"

namespace content {

void GetFontFamilyForCharacter(int32_t character,
                               const char* preferred_locale,
                               blink::WebFontFamily* family) {
  TRACE_EVENT0("sandbox_ipc", "GetFontFamilyForCharacter");

  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_FONT_FAMILY_FOR_CHAR);
  request.WriteInt(character);
  request.WriteString(preferred_locale);

  uint8_t buf[512];
  const ssize_t n = UnixDomainSocket::SendRecvMsg(GetSandboxFD(), buf,
                                                  sizeof(buf), NULL, request);

  std::string family_name;
  bool isBold = false;
  bool isItalic = false;
  if (n != -1) {
    Pickle reply(reinterpret_cast<char*>(buf), n);
    PickleIterator pickle_iter(reply);
    if (reply.ReadString(&pickle_iter, &family_name) &&
        reply.ReadBool(&pickle_iter, &isBold) &&
        reply.ReadBool(&pickle_iter, &isItalic)) {
      family->name = family_name;
      family->isBold = isBold;
      family->isItalic = isItalic;
    }
  }
}

void GetRenderStyleForStrike(const char* family, int sizeAndStyle,
                             blink::WebFontRenderStyle* out) {
  TRACE_EVENT0("sandbox_ipc", "GetRenderStyleForStrike");

  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_STYLE_FOR_STRIKE);
  request.WriteString(family);
  request.WriteInt(sizeAndStyle);

  uint8_t buf[512];
  const ssize_t n = UnixDomainSocket::SendRecvMsg(GetSandboxFD(), buf,
                                                  sizeof(buf), NULL, request);

  out->setDefaults();
  if (n == -1) {
    return;
  }

  Pickle reply(reinterpret_cast<char*>(buf), n);
  PickleIterator pickle_iter(reply);
  int useBitmaps, useAutoHint, useHinting, hintStyle, useAntiAlias;
  int useSubpixelRendering, useSubpixelPositioning;
  if (reply.ReadInt(&pickle_iter, &useBitmaps) &&
      reply.ReadInt(&pickle_iter, &useAutoHint) &&
      reply.ReadInt(&pickle_iter, &useHinting) &&
      reply.ReadInt(&pickle_iter, &hintStyle) &&
      reply.ReadInt(&pickle_iter, &useAntiAlias) &&
      reply.ReadInt(&pickle_iter, &useSubpixelRendering) &&
      reply.ReadInt(&pickle_iter, &useSubpixelPositioning)) {
    out->useBitmaps = useBitmaps;
    out->useAutoHint = useAutoHint;
    out->useHinting = useHinting;
    out->hintStyle = hintStyle;
    out->useAntiAlias = useAntiAlias;
    out->useSubpixelRendering = useSubpixelRendering;
    out->useSubpixelPositioning = useSubpixelPositioning;
  }
}

int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset) {
  TRACE_EVENT0("sandbox_ipc", "MatchFontWithFallback");

  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_MATCH_WITH_FALLBACK);
  request.WriteString(face);
  request.WriteBool(bold);
  request.WriteBool(italic);
  request.WriteUInt32(charset);
  uint8_t reply_buf[64];
  int fd = -1;
  UnixDomainSocket::SendRecvMsg(GetSandboxFD(), reply_buf, sizeof(reply_buf),
                                &fd, request);
  return fd;
}

bool GetFontTable(int fd, uint32_t table_tag, off_t offset,
                  uint8_t* output, size_t* output_length) {
  if (offset < 0)
    return false;

  size_t data_length = 0;  // the length of the file data.
  off_t data_offset = 0;   // the offset of the data in the file.
  if (table_tag == 0) {
    // Get the entire font file.
    struct stat st;
    if (fstat(fd, &st) < 0)
      return false;
    data_length = base::checked_numeric_cast<size_t>(st.st_size);
  } else {
    // Get a font table. Read the header to find its offset in the file.
    uint16_t num_tables;
    ssize_t n = HANDLE_EINTR(pread(fd, &num_tables, sizeof(num_tables),
                             4 /* skip the font type */));
    if (n != sizeof(num_tables))
      return false;
    // Font data is stored in net (big-endian) order.
    num_tables = base::NetToHost16(num_tables);

    // Read the table directory.
    static const size_t kTableEntrySize = 16;
    const size_t directory_size = num_tables * kTableEntrySize;
    scoped_ptr<uint8_t[]> table_entries(new uint8_t[directory_size]);
    n = HANDLE_EINTR(pread(fd, table_entries.get(), directory_size,
                           12 /* skip the SFNT header */));
    if (n != base::checked_numeric_cast<ssize_t>(directory_size))
      return false;

    for (uint16_t i = 0; i < num_tables; ++i) {
      uint8_t* entry = table_entries.get() + i * kTableEntrySize;
      uint32_t tag = *reinterpret_cast<uint32_t*>(entry);
      if (tag == table_tag) {
        // Font data is stored in net (big-endian) order.
        data_offset =
            base::NetToHost32(*reinterpret_cast<uint32_t*>(entry + 8));
        data_length =
            base::NetToHost32(*reinterpret_cast<uint32_t*>(entry + 12));
        break;
      }
    }
  }

  if (!data_length)
    return false;
  // Clamp |offset| inside the allowable range. This allows the read to succeed
  // but return 0 bytes.
  offset = std::min(offset, base::checked_numeric_cast<off_t>(data_length));
  // Make sure it's safe to add the data offset and the caller's logical offset.
  // Define the maximum positive offset on 32 bit systems.
  static const off_t kMaxPositiveOffset32 = 0x7FFFFFFF;  // 2 GB - 1.
  if ((offset > kMaxPositiveOffset32 / 2) ||
      (data_offset > kMaxPositiveOffset32 / 2))
    return false;
  data_offset += offset;
  data_length -= offset;

  if (output) {
    // 'output_length' holds the maximum amount of data the caller can accept.
    data_length = std::min(data_length, *output_length);
    ssize_t n = HANDLE_EINTR(pread(fd, output, data_length, data_offset));
    if (n != base::checked_numeric_cast<ssize_t>(data_length))
      return false;
  }
  *output_length = data_length;

  return true;
}

}  // namespace content
