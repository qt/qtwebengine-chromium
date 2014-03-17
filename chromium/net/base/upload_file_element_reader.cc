// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// In tests, this value is used to override the return value of
// UploadFileElementReader::GetContentLength() when set to non-zero.
uint64 overriding_content_length = 0;

// This function is used to implement Init().
template<typename FileStreamDeleter>
int InitInternal(const base::FilePath& path,
                 uint64 range_offset,
                 uint64 range_length,
                 const base::Time& expected_modification_time,
                 scoped_ptr<FileStream, FileStreamDeleter>* out_file_stream,
                 uint64* out_content_length) {
  scoped_ptr<FileStream> file_stream(new FileStream(NULL));
  int64 rv = file_stream->OpenSync(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (rv != OK) {
    // If the file can't be opened, the upload should fail.
    DLOG(WARNING) << "Failed to open \"" << path.value()
                  << "\" for reading: " << rv;
    return rv;
  } else if (range_offset) {
    rv = file_stream->SeekSync(FROM_BEGIN, range_offset);
    if (rv < 0) {
      DLOG(WARNING) << "Failed to seek \"" << path.value()
                    << "\" to offset: " << range_offset << " (" << rv << ")";
      return rv;
    }
  }

  int64 length = 0;
  if (!base::GetFileSize(path, &length)) {
    DLOG(WARNING) << "Failed to get file size of \"" << path.value() << "\"";
    return ERR_FILE_NOT_FOUND;
  }

  if (range_offset < static_cast<uint64>(length)) {
    // Compensate for the offset.
    length = std::min(length - range_offset, range_length);
  }

  // If the underlying file has been changed and the expected file modification
  // time is set, treat it as error. Note that the expected modification time
  // from WebKit is based on time_t precision. So we have to convert both to
  // time_t to compare. This check is used for sliced files.
  if (!expected_modification_time.is_null()) {
    base::PlatformFileInfo info;
    if (!base::GetFileInfo(path, &info)) {
      DLOG(WARNING) << "Failed to get file info of \"" << path.value() << "\"";
      return ERR_FILE_NOT_FOUND;
    }

    if (expected_modification_time.ToTimeT() != info.last_modified.ToTimeT()) {
      return ERR_UPLOAD_FILE_CHANGED;
    }
  }

  *out_content_length = length;
  out_file_stream->reset(file_stream.release());

  return OK;
}

// This function is used to implement Read().
int ReadInternal(scoped_refptr<IOBuffer> buf,
                 int buf_length,
                 uint64 bytes_remaining,
                 FileStream* file_stream) {
  DCHECK_LT(0, buf_length);

  const uint64 num_bytes_to_read =
      std::min(bytes_remaining, static_cast<uint64>(buf_length));

  int result = 0;
  if (num_bytes_to_read > 0) {
    DCHECK(file_stream);  // file_stream is non-null if content_length_ > 0.
    result = file_stream->ReadSync(buf->data(), num_bytes_to_read);
    if (result == 0)  // Reached end-of-file earlier than expected.
      result = ERR_UPLOAD_FILE_CHANGED;
  }
  return result;
}

}  // namespace

UploadFileElementReader::FileStreamDeleter::FileStreamDeleter(
    base::TaskRunner* task_runner) : task_runner_(task_runner) {
  DCHECK(task_runner_.get());
}

UploadFileElementReader::FileStreamDeleter::~FileStreamDeleter() {}

void UploadFileElementReader::FileStreamDeleter::operator() (
    FileStream* file_stream) const {
  if (file_stream) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&base::DeletePointer<FileStream>,
                                      file_stream));
  }
}

UploadFileElementReader::UploadFileElementReader(
    base::TaskRunner* task_runner,
    const base::FilePath& path,
    uint64 range_offset,
    uint64 range_length,
    const base::Time& expected_modification_time)
    : task_runner_(task_runner),
      path_(path),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      file_stream_(NULL, FileStreamDeleter(task_runner_.get())),
      content_length_(0),
      bytes_remaining_(0),
      weak_ptr_factory_(this) {
  DCHECK(task_runner_.get());
}

UploadFileElementReader::~UploadFileElementReader() {
}

const UploadFileElementReader* UploadFileElementReader::AsFileReader() const {
  return this;
}

int UploadFileElementReader::Init(const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  Reset();

  ScopedFileStreamPtr* file_stream =
      new ScopedFileStreamPtr(NULL, FileStreamDeleter(task_runner_.get()));
  uint64* content_length = new uint64;
  const bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&InitInternal<FileStreamDeleter>,
                 path_,
                 range_offset_,
                 range_length_,
                 expected_modification_time_,
                 file_stream,
                 content_length),
      base::Bind(&UploadFileElementReader::OnInitCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file_stream),
                 base::Owned(content_length),
                 callback));
  DCHECK(posted);
  return ERR_IO_PENDING;
}

uint64 UploadFileElementReader::GetContentLength() const {
  if (overriding_content_length)
    return overriding_content_length;
  return content_length_;
}

uint64 UploadFileElementReader::BytesRemaining() const {
  return bytes_remaining_;
}

int UploadFileElementReader::Read(IOBuffer* buf,
                                  int buf_length,
                                  const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (BytesRemaining() == 0)
    return 0;

  // Save the value of file_stream_.get() before base::Passed() invalidates it.
  FileStream* file_stream_ptr = file_stream_.get();
  // Pass the ownership of file_stream_ to the worker pool to safely perform
  // operation even when |this| is destructed before the read completes.
  const bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&ReadInternal,
                 scoped_refptr<IOBuffer>(buf),
                 buf_length,
                 BytesRemaining(),
                 file_stream_ptr),
      base::Bind(&UploadFileElementReader::OnReadCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&file_stream_),
                 callback));
  DCHECK(posted);
  return ERR_IO_PENDING;
}

void UploadFileElementReader::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  bytes_remaining_ = 0;
  content_length_ = 0;
  file_stream_.reset();
}

void UploadFileElementReader::OnInitCompleted(
    ScopedFileStreamPtr* file_stream,
    uint64* content_length,
    const CompletionCallback& callback,
    int result) {
  file_stream_.swap(*file_stream);
  content_length_ = *content_length;
  bytes_remaining_ = GetContentLength();
  if (!callback.is_null())
    callback.Run(result);
}

void UploadFileElementReader::OnReadCompleted(
    ScopedFileStreamPtr file_stream,
    const CompletionCallback& callback,
    int result) {
  file_stream_.swap(file_stream);
  if (result > 0) {
    DCHECK_GE(bytes_remaining_, static_cast<uint64>(result));
    bytes_remaining_ -= result;
  }
  if (!callback.is_null())
    callback.Run(result);
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
ScopedOverridingContentLengthForTests(uint64 value) {
  overriding_content_length = value;
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
~ScopedOverridingContentLengthForTests() {
  overriding_content_length = 0;
}

UploadFileElementReaderSync::UploadFileElementReaderSync(
    const base::FilePath& path,
    uint64 range_offset,
    uint64 range_length,
    const base::Time& expected_modification_time)
    : path_(path),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      content_length_(0),
      bytes_remaining_(0) {
}

UploadFileElementReaderSync::~UploadFileElementReaderSync() {
}

int UploadFileElementReaderSync::Init(const CompletionCallback& callback) {
  bytes_remaining_ = 0;
  content_length_ = 0;
  file_stream_.reset();

  const int result = InitInternal(path_, range_offset_, range_length_,
                                  expected_modification_time_,
                                  &file_stream_, &content_length_);
  bytes_remaining_ = GetContentLength();
  return result;
}

uint64 UploadFileElementReaderSync::GetContentLength() const {
  return content_length_;
}

uint64 UploadFileElementReaderSync::BytesRemaining() const {
  return bytes_remaining_;
}

int UploadFileElementReaderSync::Read(IOBuffer* buf,
                                      int buf_length,
                                      const CompletionCallback& callback) {
  const int result = ReadInternal(buf, buf_length, BytesRemaining(),
                                  file_stream_.get());
  if (result > 0) {
    DCHECK_GE(bytes_remaining_, static_cast<uint64>(result));
    bytes_remaining_ -= result;
  }
  return result;
}

}  // namespace net
