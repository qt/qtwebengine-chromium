// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/appcache/appcache_response.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/browser/appcache/appcache_storage.h"

namespace appcache {

namespace {

// Disk cache entry data indices.
enum {
  kResponseInfoIndex,
  kResponseContentIndex
};

// An IOBuffer that wraps a pickle's data. Ownership of the
// pickle is transfered to the WrappedPickleIOBuffer object.
class WrappedPickleIOBuffer : public net::WrappedIOBuffer {
 public:
  explicit WrappedPickleIOBuffer(const Pickle* pickle) :
      net::WrappedIOBuffer(reinterpret_cast<const char*>(pickle->data())),
      pickle_(pickle) {
    DCHECK(pickle->data());
  }

 private:
  virtual ~WrappedPickleIOBuffer() {}

  scoped_ptr<const Pickle> pickle_;
};

}  // anon namespace


// AppCacheResponseInfo ----------------------------------------------

AppCacheResponseInfo::AppCacheResponseInfo(
    AppCacheStorage* storage, const GURL& manifest_url,
    int64 response_id,  net::HttpResponseInfo* http_info,
    int64 response_data_size)
    : manifest_url_(manifest_url), response_id_(response_id),
      http_response_info_(http_info), response_data_size_(response_data_size),
      storage_(storage) {
  DCHECK(http_info);
  DCHECK(response_id != kNoResponseId);
  storage_->working_set()->AddResponseInfo(this);
}

AppCacheResponseInfo::~AppCacheResponseInfo() {
  storage_->working_set()->RemoveResponseInfo(this);
}

// HttpResponseInfoIOBuffer ------------------------------------------

HttpResponseInfoIOBuffer::HttpResponseInfoIOBuffer()
    : response_data_size(kUnkownResponseDataSize) {}

HttpResponseInfoIOBuffer::HttpResponseInfoIOBuffer(net::HttpResponseInfo* info)
    : http_info(info), response_data_size(kUnkownResponseDataSize) {}

HttpResponseInfoIOBuffer::~HttpResponseInfoIOBuffer() {}

// AppCacheResponseIO ----------------------------------------------

AppCacheResponseIO::AppCacheResponseIO(
    int64 response_id, int64 group_id, AppCacheDiskCacheInterface* disk_cache)
    : response_id_(response_id),
      group_id_(group_id),
      disk_cache_(disk_cache),
      entry_(NULL),
      buffer_len_(0),
      weak_factory_(this) {
}

AppCacheResponseIO::~AppCacheResponseIO() {
  if (entry_)
    entry_->Close();
}

void AppCacheResponseIO::ScheduleIOCompletionCallback(int result) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&AppCacheResponseIO::OnIOComplete,
                            weak_factory_.GetWeakPtr(), result));
}

void AppCacheResponseIO::InvokeUserCompletionCallback(int result) {
  // Clear the user callback and buffers prior to invoking the callback
  // so the caller can schedule additional operations in the callback.
  buffer_ = NULL;
  info_buffer_ = NULL;
  net::CompletionCallback cb = callback_;
  callback_.Reset();
  cb.Run(result);
}

void AppCacheResponseIO::ReadRaw(int index, int offset,
                                 net::IOBuffer* buf, int buf_len) {
  DCHECK(entry_);
  int rv = entry_->Read(
      index, offset, buf, buf_len,
      base::Bind(&AppCacheResponseIO::OnRawIOComplete,
                 weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    ScheduleIOCompletionCallback(rv);
}

void AppCacheResponseIO::WriteRaw(int index, int offset,
                                 net::IOBuffer* buf, int buf_len) {
  DCHECK(entry_);
  int rv = entry_->Write(
      index, offset, buf, buf_len,
      base::Bind(&AppCacheResponseIO::OnRawIOComplete,
                 weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    ScheduleIOCompletionCallback(rv);
}

void AppCacheResponseIO::OnRawIOComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  OnIOComplete(result);
}


// AppCacheResponseReader ----------------------------------------------

AppCacheResponseReader::AppCacheResponseReader(
    int64 response_id, int64 group_id, AppCacheDiskCacheInterface* disk_cache)
    : AppCacheResponseIO(response_id, group_id, disk_cache),
      range_offset_(0),
      range_length_(kint32max),
      read_position_(0),
      weak_factory_(this) {
}

AppCacheResponseReader::~AppCacheResponseReader() {
}

void AppCacheResponseReader::ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                                      const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsReadPending());
  DCHECK(info_buf);
  DCHECK(!info_buf->http_info.get());
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());

  info_buffer_ = info_buf;
  callback_ = callback;  // cleared on completion
  OpenEntryIfNeededAndContinue();
}

void AppCacheResponseReader::ContinueReadInfo() {
  if (!entry_)  {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  int size = entry_->GetSize(kResponseInfoIndex);
  if (size <= 0) {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  buffer_ = new net::IOBuffer(size);
  ReadRaw(kResponseInfoIndex, 0, buffer_.get(), size);
}

void AppCacheResponseReader::ReadData(net::IOBuffer* buf, int buf_len,
                                      const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsReadPending());
  DCHECK(buf);
  DCHECK(buf_len >= 0);
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());

  buffer_ = buf;
  buffer_len_ = buf_len;
  callback_ = callback;  // cleared on completion
  OpenEntryIfNeededAndContinue();
}

void AppCacheResponseReader::ContinueReadData() {
  if (!entry_)  {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  if (read_position_ + buffer_len_ > range_length_) {
    // TODO(michaeln): What about integer overflows?
    DCHECK(range_length_ >= read_position_);
    buffer_len_ = range_length_ - read_position_;
  }
  ReadRaw(kResponseContentIndex,
          range_offset_ + read_position_,
          buffer_.get(),
          buffer_len_);
}

void AppCacheResponseReader::SetReadRange(int offset, int length) {
  DCHECK(!IsReadPending() && !read_position_);
  range_offset_ = offset;
  range_length_ = length;
}

void AppCacheResponseReader::OnIOComplete(int result) {
  if (result >= 0) {
    if (info_buffer_.get()) {
      // Deserialize the http info structure, ensuring we got headers.
      Pickle pickle(buffer_->data(), result);
      scoped_ptr<net::HttpResponseInfo> info(new net::HttpResponseInfo);
      bool response_truncated = false;
      if (!info->InitFromPickle(pickle, &response_truncated) ||
          !info->headers.get()) {
        InvokeUserCompletionCallback(net::ERR_FAILED);
        return;
      }
      DCHECK(!response_truncated);
      info_buffer_->http_info.reset(info.release());

      // Also return the size of the response body
      DCHECK(entry_);
      info_buffer_->response_data_size =
          entry_->GetSize(kResponseContentIndex);
    } else {
      read_position_ += result;
    }
  }
  InvokeUserCompletionCallback(result);
}

void AppCacheResponseReader::OpenEntryIfNeededAndContinue() {
  int rv;
  AppCacheDiskCacheInterface::Entry** entry_ptr = NULL;
  if (entry_) {
    rv = net::OK;
  } else if (!disk_cache_) {
    rv = net::ERR_FAILED;
  } else {
    entry_ptr = new AppCacheDiskCacheInterface::Entry*;
    open_callback_ =
        base::Bind(&AppCacheResponseReader::OnOpenEntryComplete,
                   weak_factory_.GetWeakPtr(), base::Owned(entry_ptr));
    rv = disk_cache_->OpenEntry(response_id_, entry_ptr, open_callback_);
  }

  if (rv != net::ERR_IO_PENDING)
    OnOpenEntryComplete(entry_ptr, rv);
}

void AppCacheResponseReader::OnOpenEntryComplete(
    AppCacheDiskCacheInterface::Entry** entry, int rv) {
  DCHECK(info_buffer_.get() || buffer_.get());

  if (!open_callback_.is_null()) {
    if (rv == net::OK) {
      DCHECK(entry);
      entry_ = *entry;
    }
    open_callback_.Reset();
  }

  if (info_buffer_.get())
    ContinueReadInfo();
  else
    ContinueReadData();
}

// AppCacheResponseWriter ----------------------------------------------

AppCacheResponseWriter::AppCacheResponseWriter(
    int64 response_id, int64 group_id, AppCacheDiskCacheInterface* disk_cache)
    : AppCacheResponseIO(response_id, group_id, disk_cache),
      info_size_(0),
      write_position_(0),
      write_amount_(0),
      creation_phase_(INITIAL_ATTEMPT),
      weak_factory_(this) {
}

AppCacheResponseWriter::~AppCacheResponseWriter() {
}

void AppCacheResponseWriter::WriteInfo(
    HttpResponseInfoIOBuffer* info_buf,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsWritePending());
  DCHECK(info_buf);
  DCHECK(info_buf->http_info.get());
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());
  DCHECK(info_buf->http_info->headers.get());

  info_buffer_ = info_buf;
  callback_ = callback;  // cleared on completion
  CreateEntryIfNeededAndContinue();
}

void AppCacheResponseWriter::ContinueWriteInfo() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }

  const bool kSkipTransientHeaders = true;
  const bool kTruncated = false;
  Pickle* pickle = new Pickle;
  info_buffer_->http_info->Persist(pickle, kSkipTransientHeaders, kTruncated);
  write_amount_ = static_cast<int>(pickle->size());
  buffer_ = new WrappedPickleIOBuffer(pickle);  // takes ownership of pickle
  WriteRaw(kResponseInfoIndex, 0, buffer_.get(), write_amount_);
}

void AppCacheResponseWriter::WriteData(
    net::IOBuffer* buf, int buf_len, const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsWritePending());
  DCHECK(buf);
  DCHECK(buf_len >= 0);
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());

  buffer_ = buf;
  write_amount_ = buf_len;
  callback_ = callback;  // cleared on completion
  CreateEntryIfNeededAndContinue();
}

void AppCacheResponseWriter::ContinueWriteData() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }
  WriteRaw(
      kResponseContentIndex, write_position_, buffer_.get(), write_amount_);
}

void AppCacheResponseWriter::OnIOComplete(int result) {
  if (result >= 0) {
    DCHECK(write_amount_ == result);
    if (!info_buffer_.get())
      write_position_ += result;
    else
      info_size_ = result;
  }
  InvokeUserCompletionCallback(result);
}

void AppCacheResponseWriter::CreateEntryIfNeededAndContinue() {
  int rv;
  AppCacheDiskCacheInterface::Entry** entry_ptr = NULL;
  if (entry_) {
    creation_phase_ = NO_ATTEMPT;
    rv = net::OK;
  } else if (!disk_cache_) {
    creation_phase_ = NO_ATTEMPT;
    rv = net::ERR_FAILED;
  } else {
    creation_phase_ = INITIAL_ATTEMPT;
    entry_ptr = new AppCacheDiskCacheInterface::Entry*;
    create_callback_ =
        base::Bind(&AppCacheResponseWriter::OnCreateEntryComplete,
                   weak_factory_.GetWeakPtr(), base::Owned(entry_ptr));
    rv = disk_cache_->CreateEntry(response_id_, entry_ptr, create_callback_);
  }
  if (rv != net::ERR_IO_PENDING)
    OnCreateEntryComplete(entry_ptr, rv);
}

void AppCacheResponseWriter::OnCreateEntryComplete(
    AppCacheDiskCacheInterface::Entry** entry, int rv) {
  DCHECK(info_buffer_.get() || buffer_.get());

  if (creation_phase_ == INITIAL_ATTEMPT) {
    if (rv != net::OK) {
      // We may try to overwrite existing entries.
      creation_phase_ = DOOM_EXISTING;
      rv = disk_cache_->DoomEntry(response_id_, create_callback_);
      if (rv != net::ERR_IO_PENDING)
        OnCreateEntryComplete(NULL, rv);
      return;
    }
  } else if (creation_phase_ == DOOM_EXISTING) {
    creation_phase_ = SECOND_ATTEMPT;
    AppCacheDiskCacheInterface::Entry** entry_ptr =
        new AppCacheDiskCacheInterface::Entry*;
    create_callback_ =
        base::Bind(&AppCacheResponseWriter::OnCreateEntryComplete,
                   weak_factory_.GetWeakPtr(), base::Owned(entry_ptr));
    rv = disk_cache_->CreateEntry(response_id_, entry_ptr, create_callback_);
    if (rv != net::ERR_IO_PENDING)
      OnCreateEntryComplete(entry_ptr, rv);
    return;
  }

  if (!create_callback_.is_null()) {
    if (rv == net::OK)
      entry_ = *entry;

    create_callback_.Reset();
  }

  if (info_buffer_.get())
    ContinueWriteInfo();
  else
    ContinueWriteData();
}

}  // namespace appcache
