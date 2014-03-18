// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/common/clipboard_messages.h"
#include "content/public/browser/browser_context.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace content {


namespace {

// On Windows, the write must be performed on the UI thread because the
// clipboard object from the IO thread cannot create windows so it cannot be
// the "owner" of the clipboard's contents. See http://crbug.com/5823.
void WriteObjectsOnUIThread(ui::Clipboard::ObjectMap* objects) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->WriteObjects(ui::CLIPBOARD_TYPE_COPY_PASTE, *objects);
}

}  // namespace


ClipboardMessageFilter::ClipboardMessageFilter() {}

void ClipboardMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  // Clipboard writes should always occur on the UI thread due the restrictions
  // of various platform APIs. In general, the clipboard is not thread-safe, so
  // all clipboard calls should be serviced from the UI thread.
  //
  // Windows needs clipboard reads to be serviced from the IO thread because
  // these are sync IPCs which can result in deadlocks with NPAPI plugins if
  // serviced from the UI thread. Note that Windows clipboard calls ARE
  // thread-safe so it is ok for reads and writes to be serviced from different
  // threads.
#if !defined(OS_WIN)
  if (IPC_MESSAGE_CLASS(message) == ClipboardMsgStart)
    *thread = BrowserThread::UI;
#endif

#if defined(OS_WIN)
  if (message.type() == ClipboardHostMsg_ReadImage::ID)
    *thread = BrowserThread::FILE;
#endif
}

bool ClipboardMessageFilter::OnMessageReceived(const IPC::Message& message,
                                               bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ClipboardMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteObjectsAsync, OnWriteObjectsAsync)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteObjectsSync, OnWriteObjectsSync)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_GetSequenceNumber, OnGetSequenceNumber)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_IsFormatAvailable, OnIsFormatAvailable)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_Clear, OnClear)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadAvailableTypes,
                        OnReadAvailableTypes)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadText, OnReadText)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadAsciiText, OnReadAsciiText)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadHTML, OnReadHTML)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadRTF, OnReadRTF)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ClipboardHostMsg_ReadImage, OnReadImage)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadCustomData, OnReadCustomData)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadData, OnReadData)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_FindPboardWriteStringAsync,
                        OnFindPboardWriteString)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ClipboardMessageFilter::~ClipboardMessageFilter() {
}

void ClipboardMessageFilter::OnWriteObjectsSync(
    const ui::Clipboard::ObjectMap& objects,
    base::SharedMemoryHandle bitmap_handle) {
  DCHECK(base::SharedMemory::IsHandleValid(bitmap_handle))
      << "Bad bitmap handle";

  // On Windows, we can't write directly from the IO thread, so we copy the data
  // into a heap allocated map and post a task to the UI thread. On other
  // platforms, to lower the amount of time the renderer has to wait for the
  // sync IPC to complete, we also take a copy and post a task to flush the data
  // to the clipboard later.
  scoped_ptr<ui::Clipboard::ObjectMap> long_living_objects(
      new ui::Clipboard::ObjectMap(objects));
  // Splice the shared memory handle into the data. |long_living_objects| now
  // contains a heap-allocated SharedMemory object that references
  // |bitmap_handle|. This reference will keep the shared memory section alive
  // when this IPC returns, and the SharedMemory object will eventually be
  // freed by ui::Clipboard::WriteObjects().
  if (!ui::Clipboard::ReplaceSharedMemHandle(
           long_living_objects.get(), bitmap_handle, PeerHandle()))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WriteObjectsOnUIThread,
                 base::Owned(long_living_objects.release())));
}

void ClipboardMessageFilter::OnWriteObjectsAsync(
    const ui::Clipboard::ObjectMap& objects) {
  // This async message doesn't support shared-memory based bitmaps; they must
  // be removed otherwise we might dereference a rubbish pointer.
  scoped_ptr<ui::Clipboard::ObjectMap> sanitized_objects(
      new ui::Clipboard::ObjectMap(objects));
  sanitized_objects->erase(ui::Clipboard::CBF_SMBITMAP);

#if defined(OS_WIN)
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and post a task to preform
  // the write on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &WriteObjectsOnUIThread, base::Owned(sanitized_objects.release())));
#else
  GetClipboard()->WriteObjects(
      ui::CLIPBOARD_TYPE_COPY_PASTE, *sanitized_objects.get());
#endif
}

void ClipboardMessageFilter::OnGetSequenceNumber(ui::ClipboardType type,
                                                 uint64* sequence_number) {
  *sequence_number = GetClipboard()->GetSequenceNumber(type);
}

void ClipboardMessageFilter::OnReadAvailableTypes(
    ui::ClipboardType type,
    std::vector<base::string16>* types,
    bool* contains_filenames) {
  GetClipboard()->ReadAvailableTypes(type, types, contains_filenames);
}

void ClipboardMessageFilter::OnIsFormatAvailable(
    const ui::Clipboard::FormatType& format,
    ui::ClipboardType type,
    bool* result) {
  *result = GetClipboard()->IsFormatAvailable(format, type);
}

void ClipboardMessageFilter::OnClear(ui::ClipboardType type) {
  GetClipboard()->Clear(type);
}

void ClipboardMessageFilter::OnReadText(ui::ClipboardType type,
                                        base::string16* result) {
  GetClipboard()->ReadText(type, result);
}

void ClipboardMessageFilter::OnReadAsciiText(ui::ClipboardType type,
                                             std::string* result) {
  GetClipboard()->ReadAsciiText(type, result);
}

void ClipboardMessageFilter::OnReadHTML(ui::ClipboardType type,
                                        base::string16* markup,
                                        GURL* url,
                                        uint32* fragment_start,
                                        uint32* fragment_end) {
  std::string src_url_str;
  GetClipboard()->ReadHTML(type, markup, &src_url_str, fragment_start,
                           fragment_end);
  *url = GURL(src_url_str);
}

void ClipboardMessageFilter::OnReadRTF(ui::ClipboardType type,
                                       std::string* result) {
  GetClipboard()->ReadRTF(type, result);
}

void ClipboardMessageFilter::OnReadImage(ui::ClipboardType type,
                                         IPC::Message* reply_msg) {
  SkBitmap bitmap = GetClipboard()->ReadImage(type);

#if defined(USE_X11)
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ClipboardMessageFilter::OnReadImageReply, this, bitmap, reply_msg));
#else
  OnReadImageReply(bitmap, reply_msg);
#endif
}

void ClipboardMessageFilter::OnReadImageReply(
    const SkBitmap& bitmap, IPC::Message* reply_msg) {
  base::SharedMemoryHandle image_handle = base::SharedMemory::NULLHandle();
  uint32 image_size = 0;
  if (!bitmap.isNull()) {
    std::vector<unsigned char> png_data;
    if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &png_data)) {
      base::SharedMemory buffer;
      if (buffer.CreateAndMapAnonymous(png_data.size())) {
        memcpy(buffer.memory(), vector_as_array(&png_data), png_data.size());
        if (buffer.GiveToProcess(PeerHandle(), &image_handle)) {
          image_size = png_data.size();
        }
      }
    }
  }
  ClipboardHostMsg_ReadImage::WriteReplyParams(reply_msg, image_handle,
                                               image_size);
  Send(reply_msg);
}

void ClipboardMessageFilter::OnReadCustomData(ui::ClipboardType clipboard_type,
                                              const base::string16& type,
                                              base::string16* result) {
  GetClipboard()->ReadCustomData(clipboard_type, type, result);
}

void ClipboardMessageFilter::OnReadData(const ui::Clipboard::FormatType& format,
                                        std::string* data) {
  GetClipboard()->ReadData(format, data);
}

// static
ui::Clipboard* ClipboardMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  return clipboard;
}

}  // namespace content
