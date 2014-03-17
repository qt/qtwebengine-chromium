// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_writer_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util_proxy.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

static const int kReadBufSize = 32768;

FileWriterDelegate::FileWriterDelegate(
    scoped_ptr<FileStreamWriter> file_stream_writer)
    : file_stream_writer_(file_stream_writer.Pass()),
      writing_started_(false),
      bytes_written_backlog_(0),
      bytes_written_(0),
      bytes_read_(0),
      io_buffer_(new net::IOBufferWithSize(kReadBufSize)),
      weak_factory_(this) {
}

FileWriterDelegate::~FileWriterDelegate() {
}

void FileWriterDelegate::Start(scoped_ptr<net::URLRequest> request,
                               const DelegateWriteCallback& write_callback) {
  write_callback_ = write_callback;
  request_ = request.Pass();
  request_->Start();
}

void FileWriterDelegate::Cancel() {
  if (request_) {
    // This halts any callbacks on this delegate.
    request_->set_delegate(NULL);
    request_->Cancel();
  }

  const int status = file_stream_writer_->Cancel(
      base::Bind(&FileWriterDelegate::OnWriteCancelled,
                 weak_factory_.GetWeakPtr()));
  // Return true to finish immediately if we have no pending writes.
  // Otherwise we'll do the final cleanup in the Cancel callback.
  if (status != net::ERR_IO_PENDING) {
    write_callback_.Run(base::PLATFORM_FILE_ERROR_ABORT, 0,
                        GetCompletionStatusOnError());
  }
}

void FileWriterDelegate::OnReceivedRedirect(net::URLRequest* request,
                                            const GURL& new_url,
                                            bool* defer_redirect) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnAuthRequired(net::URLRequest* request,
                                        net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnSSLCertificateError(net::URLRequest* request,
                                               const net::SSLInfo& ssl_info,
                                               bool fatal) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request_.get(), request);
  if (!request->status().is_success() || request->GetResponseCode() != 200) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  Read();
}

void FileWriterDelegate::OnReadCompleted(net::URLRequest* request,
                                         int bytes_read) {
  DCHECK_EQ(request_.get(), request);
  if (!request->status().is_success()) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  OnDataReceived(bytes_read);
}

void FileWriterDelegate::Read() {
  bytes_written_ = 0;
  bytes_read_ = 0;
  if (request_->Read(io_buffer_.get(), io_buffer_->size(), &bytes_read_)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FileWriterDelegate::OnDataReceived,
                   weak_factory_.GetWeakPtr(), bytes_read_));
  } else if (!request_->status().is_io_pending()) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
  }
}

void FileWriterDelegate::OnDataReceived(int bytes_read) {
  bytes_read_ = bytes_read;
  if (!bytes_read_) {  // We're done.
    OnProgress(0, true);
  } else {
    // This could easily be optimized to rotate between a pool of buffers, so
    // that we could read and write at the same time.  It's not yet clear that
    // it's necessary.
    cursor_ = new net::DrainableIOBuffer(io_buffer_.get(), bytes_read_);
    Write();
  }
}

void FileWriterDelegate::Write() {
  writing_started_ = true;
  int64 bytes_to_write = bytes_read_ - bytes_written_;
  int write_response =
      file_stream_writer_->Write(cursor_.get(),
                                 static_cast<int>(bytes_to_write),
                                 base::Bind(&FileWriterDelegate::OnDataWritten,
                                            weak_factory_.GetWeakPtr()));
  if (write_response > 0) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FileWriterDelegate::OnDataWritten,
                   weak_factory_.GetWeakPtr(), write_response));
  } else if (net::ERR_IO_PENDING != write_response) {
    OnError(NetErrorToPlatformFileError(write_response));
  }
}

void FileWriterDelegate::OnDataWritten(int write_response) {
  if (write_response > 0) {
    OnProgress(write_response, false);
    cursor_->DidConsume(write_response);
    bytes_written_ += write_response;
    if (bytes_written_ == bytes_read_)
      Read();
    else
      Write();
  } else {
    OnError(NetErrorToPlatformFileError(write_response));
  }
}

FileWriterDelegate::WriteProgressStatus
FileWriterDelegate::GetCompletionStatusOnError() const {
  return writing_started_ ? ERROR_WRITE_STARTED : ERROR_WRITE_NOT_STARTED;
}

void FileWriterDelegate::OnError(base::PlatformFileError error) {
  if (request_) {
    request_->set_delegate(NULL);
    request_->Cancel();
  }

  if (writing_started_)
    FlushForCompletion(error, 0, ERROR_WRITE_STARTED);
  else
    write_callback_.Run(error, 0, ERROR_WRITE_NOT_STARTED);
}

void FileWriterDelegate::OnProgress(int bytes_written, bool done) {
  DCHECK(bytes_written + bytes_written_backlog_ >= bytes_written_backlog_);
  static const int kMinProgressDelayMS = 200;
  base::Time currentTime = base::Time::Now();
  if (done || last_progress_event_time_.is_null() ||
      (currentTime - last_progress_event_time_).InMilliseconds() >
          kMinProgressDelayMS) {
    bytes_written += bytes_written_backlog_;
    last_progress_event_time_ = currentTime;
    bytes_written_backlog_ = 0;

    if (done) {
      FlushForCompletion(base::PLATFORM_FILE_OK, bytes_written,
                         SUCCESS_COMPLETED);
    } else {
      write_callback_.Run(base::PLATFORM_FILE_OK, bytes_written,
                          SUCCESS_IO_PENDING);
    }
    return;
  }
  bytes_written_backlog_ += bytes_written;
}

void FileWriterDelegate::OnWriteCancelled(int status) {
  write_callback_.Run(base::PLATFORM_FILE_ERROR_ABORT, 0,
                      GetCompletionStatusOnError());
}

void FileWriterDelegate::FlushForCompletion(
    base::PlatformFileError error,
    int bytes_written,
    WriteProgressStatus progress_status) {
  int flush_error = file_stream_writer_->Flush(
      base::Bind(&FileWriterDelegate::OnFlushed, weak_factory_.GetWeakPtr(),
                 error, bytes_written, progress_status));
  if (flush_error != net::ERR_IO_PENDING)
    OnFlushed(error, bytes_written, progress_status, flush_error);
}

void FileWriterDelegate::OnFlushed(base::PlatformFileError error,
                                   int bytes_written,
                                   WriteProgressStatus progress_status,
                                   int flush_error) {
  if (error == base::PLATFORM_FILE_OK && flush_error != net::OK) {
    // If the Flush introduced an error, overwrite the status.
    // Otherwise, keep the original error status.
    error = NetErrorToPlatformFileError(flush_error);
    progress_status = GetCompletionStatusOnError();
  }
  write_callback_.Run(error, bytes_written, progress_status);
}

}  // namespace fileapi
