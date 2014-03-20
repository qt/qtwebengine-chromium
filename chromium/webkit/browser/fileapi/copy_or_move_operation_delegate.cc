// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/copy_or_move_operation_delegate.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/recursive_operation_delegate.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

const int64 kFlushIntervalInBytes = 10 << 20;  // 10MB.

class CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  virtual ~CopyOrMoveImpl() {}
  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) = 0;
  virtual void Cancel() = 0;

 protected:
  CopyOrMoveImpl() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveImpl);
};

namespace {

// Copies a file on a (same) file system. Just delegate the operation to
// |operation_runner|.
class CopyOrMoveOnSameFileSystemImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  CopyOrMoveOnSameFileSystemImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOperationDelegate::CopyOrMoveOption option,
      const FileSystemOperation::CopyFileProgressCallback&
          file_progress_callback)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        file_progress_callback_(file_progress_callback) {
  }

  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) OVERRIDE {
    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_MOVE) {
      operation_runner_->MoveFileLocal(src_url_, dest_url_, option_, callback);
    } else {
      operation_runner_->CopyFileLocal(
          src_url_, dest_url_, option_, file_progress_callback_, callback);
    }
  }

  virtual void Cancel() OVERRIDE {
    // We can do nothing for the copy/move operation on a local file system.
    // Assuming the operation is quickly done, it should be ok to just wait
    // for the completion.
  }

 private:
  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;
  CopyOrMoveOperationDelegate::CopyOrMoveOption option_;
  FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOnSameFileSystemImpl);
};

// Specifically for cross file system copy/move operation, this class creates
// a snapshot file, validates it if necessary, runs copying process,
// validates the created file, and removes source file for move (noop for
// copy).
class SnapshotCopyOrMoveImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  SnapshotCopyOrMoveImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOperationDelegate::CopyOrMoveOption option,
      CopyOrMoveFileValidatorFactory* validator_factory,
      const FileSystemOperation::CopyFileProgressCallback&
          file_progress_callback)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        validator_factory_(validator_factory),
        file_progress_callback_(file_progress_callback),
        cancel_requested_(false),
        weak_factory_(this) {
  }

  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) OVERRIDE {
    file_progress_callback_.Run(0);
    operation_runner_->CreateSnapshotFile(
        src_url_,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterCreateSnapshot,
                   weak_factory_.GetWeakPtr(), callback));
  }

  virtual void Cancel() OVERRIDE {
    cancel_requested_ = true;
  }

 private:
  void RunAfterCreateSnapshot(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    // For now we assume CreateSnapshotFile always return a valid local file
    // path.
    DCHECK(!platform_path.empty());

    if (!validator_factory_) {
      // No validation is needed.
      RunAfterPreWriteValidation(platform_path, file_info, file_ref, callback,
                                 base::PLATFORM_FILE_OK);
      return;
    }

    // Run pre write validation.
    PreWriteValidation(
        platform_path,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterPreWriteValidation,
                   weak_factory_.GetWeakPtr(),
                   platform_path, file_info, file_ref, callback));
  }

  void RunAfterPreWriteValidation(
      const base::FilePath& platform_path,
      const base::PlatformFileInfo& file_info,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    // |file_ref| is unused but necessary to keep the file alive until
    // CopyInForeignFile() is completed.
    operation_runner_->CopyInForeignFile(
        platform_path, dest_url_,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterCopyInForeignFile,
                   weak_factory_.GetWeakPtr(), file_info, file_ref, callback));
  }

  void RunAfterCopyInForeignFile(
      const base::PlatformFileInfo& file_info,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    file_progress_callback_.Run(file_info.size);

    if (option_ == FileSystemOperation::OPTION_NONE) {
      RunAfterTouchFile(callback, base::PLATFORM_FILE_OK);
      return;
    }

    operation_runner_->TouchFile(
        dest_url_, base::Time::Now() /* last_access */,
        file_info.last_modified,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterTouchFile,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterTouchFile(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    // Even if TouchFile is failed, just ignore it.

    if (cancel_requested_) {
      callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
      return;
    }

    // |validator_| is NULL when the destination filesystem does not do
    // validation.
    if (!validator_) {
      // No validation is needed.
      RunAfterPostWriteValidation(callback, base::PLATFORM_FILE_OK);
      return;
    }

    PostWriteValidation(
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterPostWriteValidation,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterPostWriteValidation(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (cancel_requested_) {
      callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
      return;
    }

    if (error != base::PLATFORM_FILE_OK) {
      // Failed to validate. Remove the destination file.
      operation_runner_->Remove(
          dest_url_, true /* recursive */,
          base::Bind(&SnapshotCopyOrMoveImpl::DidRemoveDestForError,
                     weak_factory_.GetWeakPtr(), error, callback));
      return;
    }

    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_COPY) {
      callback.Run(base::PLATFORM_FILE_OK);
      return;
    }

    DCHECK_EQ(CopyOrMoveOperationDelegate::OPERATION_MOVE, operation_type_);

    // Remove the source for finalizing move operation.
    operation_runner_->Remove(
        src_url_, true /* recursive */,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterRemoveSourceForMove,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterRemoveSourceForMove(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      error = base::PLATFORM_FILE_OK;
    callback.Run(error);
  }

  void DidRemoveDestForError(
      base::PlatformFileError prior_error,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (error != base::PLATFORM_FILE_OK) {
      VLOG(1) << "Error removing destination file after validation error: "
              << error;
    }
    callback.Run(prior_error);
  }

  // Runs pre-write validation.
  void PreWriteValidation(
      const base::FilePath& platform_path,
      const CopyOrMoveOperationDelegate::StatusCallback& callback) {
    DCHECK(validator_factory_);
    validator_.reset(
        validator_factory_->CreateCopyOrMoveFileValidator(
            src_url_, platform_path));
    validator_->StartPreWriteValidation(callback);
  }

  // Runs post-write validation.
  void PostWriteValidation(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) {
    operation_runner_->CreateSnapshotFile(
        dest_url_,
        base::Bind(
            &SnapshotCopyOrMoveImpl::PostWriteValidationAfterCreateSnapshotFile,
            weak_factory_.GetWeakPtr(), callback));
  }

  void PostWriteValidationAfterCreateSnapshotFile(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    DCHECK(validator_);
    // Note: file_ref passed here to keep the file alive until after
    // the StartPostWriteValidation operation finishes.
    validator_->StartPostWriteValidation(
        platform_path,
        base::Bind(&SnapshotCopyOrMoveImpl::DidPostWriteValidation,
                   weak_factory_.GetWeakPtr(), file_ref, callback));
  }

  // |file_ref| is unused; it is passed here to make sure the reference is
  // alive until after post-write validation is complete.
  void DidPostWriteValidation(
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    callback.Run(error);
  }

  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;

  CopyOrMoveOperationDelegate::CopyOrMoveOption option_;
  CopyOrMoveFileValidatorFactory* validator_factory_;
  scoped_ptr<CopyOrMoveFileValidator> validator_;
  FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
  bool cancel_requested_;
  base::WeakPtrFactory<SnapshotCopyOrMoveImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SnapshotCopyOrMoveImpl);
};

// The size of buffer for StreamCopyHelper.
const int kReadBufferSize = 32768;

// To avoid too many progress callbacks, it should be called less
// frequently than 50ms.
const int kMinProgressCallbackInvocationSpanInMilliseconds = 50;

// Specifically for cross file system copy/move operation, this class uses
// stream reader and writer for copying. Validator is not supported, so if
// necessary SnapshotCopyOrMoveImpl should be used.
class StreamCopyOrMoveImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  StreamCopyOrMoveImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOperationDelegate::CopyOrMoveOption option,
      scoped_ptr<webkit_blob::FileStreamReader> reader,
      scoped_ptr<FileStreamWriter> writer,
      const FileSystemOperation::CopyFileProgressCallback&
          file_progress_callback)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        reader_(reader.Pass()),
        writer_(writer.Pass()),
        file_progress_callback_(file_progress_callback),
        cancel_requested_(false),
        weak_factory_(this) {
  }

  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) OVERRIDE {
    // Reader can be created even if the entry does not exist or the entry is
    // a directory. To check errors before destination file creation,
    // check metadata first.
    operation_runner_->GetMetadata(
        src_url_,
        base::Bind(&StreamCopyOrMoveImpl::RunAfterGetMetadataForSource,
                   weak_factory_.GetWeakPtr(), callback));
  }

  virtual void Cancel() OVERRIDE {
    cancel_requested_ = true;
    if (copy_helper_)
      copy_helper_->Cancel();
  }

 private:
  void RunAfterGetMetadataForSource(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    if (file_info.is_directory) {
      // If not a directory, failed with appropriate error code.
      callback.Run(base::PLATFORM_FILE_ERROR_NOT_A_FILE);
      return;
    }

    // To use FileStreamWriter, we need to ensure the destination file exists.
    operation_runner_->CreateFile(
        dest_url_, false /* exclusive */,
        base::Bind(&StreamCopyOrMoveImpl::RunAfterCreateFileForDestination,
                   weak_factory_.GetWeakPtr(),
                   callback, file_info.last_modified));
  }

  void RunAfterCreateFileForDestination(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      const base::Time& last_modified,
      base::PlatformFileError error) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    const bool need_flush = dest_url_.mount_option().copy_sync_option() ==
        fileapi::COPY_SYNC_OPTION_SYNC;

    DCHECK(!copy_helper_);
    copy_helper_.reset(
        new CopyOrMoveOperationDelegate::StreamCopyHelper(
            reader_.Pass(), writer_.Pass(),
            need_flush,
            kReadBufferSize,
            file_progress_callback_,
            base::TimeDelta::FromMilliseconds(
                kMinProgressCallbackInvocationSpanInMilliseconds)));
    copy_helper_->Run(
        base::Bind(&StreamCopyOrMoveImpl::RunAfterStreamCopy,
                   weak_factory_.GetWeakPtr(), callback, last_modified));
  }

  void RunAfterStreamCopy(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      const base::Time& last_modified,
      base::PlatformFileError error) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;

    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    if (option_ == FileSystemOperation::OPTION_NONE) {
      RunAfterTouchFile(callback, base::PLATFORM_FILE_OK);
      return;
    }

    operation_runner_->TouchFile(
        dest_url_, base::Time::Now() /* last_access */, last_modified,
        base::Bind(&StreamCopyOrMoveImpl::RunAfterTouchFile,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterTouchFile(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    // Even if TouchFile is failed, just ignore it.
    if (cancel_requested_) {
      callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
      return;
    }

    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_COPY) {
      callback.Run(base::PLATFORM_FILE_OK);
      return;
    }

    DCHECK_EQ(CopyOrMoveOperationDelegate::OPERATION_MOVE, operation_type_);

    // Remove the source for finalizing move operation.
    operation_runner_->Remove(
        src_url_, false /* recursive */,
        base::Bind(&StreamCopyOrMoveImpl::RunAfterRemoveForMove,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterRemoveForMove(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (cancel_requested_)
      error = base::PLATFORM_FILE_ERROR_ABORT;
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      error = base::PLATFORM_FILE_OK;
    callback.Run(error);
  }

  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;
  CopyOrMoveOperationDelegate::CopyOrMoveOption option_;
  scoped_ptr<webkit_blob::FileStreamReader> reader_;
  scoped_ptr<FileStreamWriter> writer_;
  FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
  scoped_ptr<CopyOrMoveOperationDelegate::StreamCopyHelper> copy_helper_;
  bool cancel_requested_;
  base::WeakPtrFactory<StreamCopyOrMoveImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(StreamCopyOrMoveImpl);
};

}  // namespace

CopyOrMoveOperationDelegate::StreamCopyHelper::StreamCopyHelper(
    scoped_ptr<webkit_blob::FileStreamReader> reader,
    scoped_ptr<FileStreamWriter> writer,
    bool need_flush,
    int buffer_size,
    const FileSystemOperation::CopyFileProgressCallback&
        file_progress_callback,
    const base::TimeDelta& min_progress_callback_invocation_span)
    : reader_(reader.Pass()),
      writer_(writer.Pass()),
      need_flush_(need_flush),
      file_progress_callback_(file_progress_callback),
      io_buffer_(new net::IOBufferWithSize(buffer_size)),
      num_copied_bytes_(0),
      previous_flush_offset_(0),
      min_progress_callback_invocation_span_(
          min_progress_callback_invocation_span),
      cancel_requested_(false),
      weak_factory_(this) {
}

CopyOrMoveOperationDelegate::StreamCopyHelper::~StreamCopyHelper() {
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Run(
    const StatusCallback& callback) {
  file_progress_callback_.Run(0);
  last_progress_callback_invocation_time_ = base::Time::Now();
  Read(callback);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Cancel() {
  cancel_requested_ = true;
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Read(
    const StatusCallback& callback) {
  int result = reader_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::Bind(&StreamCopyHelper::DidRead,
                 weak_factory_.GetWeakPtr(), callback));
  if (result != net::ERR_IO_PENDING)
    DidRead(callback, result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidRead(
    const StatusCallback& callback, int result) {
  if (cancel_requested_) {
    callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    callback.Run(NetErrorToPlatformFileError(result));
    return;
  }

  if (result == 0) {
    // Here is the EOF.
    if (need_flush_)
      Flush(callback, true /* is_eof */);
    else
      callback.Run(base::PLATFORM_FILE_OK);
    return;
  }

  Write(callback, new net::DrainableIOBuffer(io_buffer_.get(), result));
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Write(
    const StatusCallback& callback,
    scoped_refptr<net::DrainableIOBuffer> buffer) {
  DCHECK_GT(buffer->BytesRemaining(), 0);

  int result = writer_->Write(
      buffer.get(), buffer->BytesRemaining(),
      base::Bind(&StreamCopyHelper::DidWrite,
                 weak_factory_.GetWeakPtr(), callback, buffer));
  if (result != net::ERR_IO_PENDING)
    DidWrite(callback, buffer, result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidWrite(
    const StatusCallback& callback,
    scoped_refptr<net::DrainableIOBuffer> buffer,
    int result) {
  if (cancel_requested_) {
    callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    callback.Run(NetErrorToPlatformFileError(result));
    return;
  }

  buffer->DidConsume(result);
  num_copied_bytes_ += result;

  // Check the elapsed time since last |file_progress_callback_| invocation.
  base::Time now = base::Time::Now();
  if (now - last_progress_callback_invocation_time_ >=
      min_progress_callback_invocation_span_) {
    file_progress_callback_.Run(num_copied_bytes_);
    last_progress_callback_invocation_time_ = now;
  }

  if (buffer->BytesRemaining() > 0) {
    Write(callback, buffer);
    return;
  }

  if (need_flush_ &&
      (num_copied_bytes_ - previous_flush_offset_) > kFlushIntervalInBytes) {
    Flush(callback, false /* not is_eof */);
  } else {
    Read(callback);
  }
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Flush(
    const StatusCallback& callback, bool is_eof) {
  int result = writer_->Flush(
      base::Bind(&StreamCopyHelper::DidFlush,
                 weak_factory_.GetWeakPtr(), callback, is_eof));
  if (result != net::ERR_IO_PENDING)
    DidFlush(callback, is_eof, result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidFlush(
    const StatusCallback& callback, bool is_eof, int result) {
  if (cancel_requested_) {
    callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
    return;
  }

  previous_flush_offset_ = num_copied_bytes_;
  if (is_eof)
    callback.Run(NetErrorToPlatformFileError(result));
  else
    Read(callback);
}

CopyOrMoveOperationDelegate::CopyOrMoveOperationDelegate(
    FileSystemContext* file_system_context,
    const FileSystemURL& src_root,
    const FileSystemURL& dest_root,
    OperationType operation_type,
    CopyOrMoveOption option,
    const CopyProgressCallback& progress_callback,
    const StatusCallback& callback)
    : RecursiveOperationDelegate(file_system_context),
      src_root_(src_root),
      dest_root_(dest_root),
      operation_type_(operation_type),
      option_(option),
      progress_callback_(progress_callback),
      callback_(callback),
      weak_factory_(this) {
  same_file_system_ = src_root_.IsInSameFileSystem(dest_root_);
}

CopyOrMoveOperationDelegate::~CopyOrMoveOperationDelegate() {
  STLDeleteElements(&running_copy_set_);
}

void CopyOrMoveOperationDelegate::Run() {
  // Not supported; this should never be called.
  NOTREACHED();
}

void CopyOrMoveOperationDelegate::RunRecursively() {
  // Perform light-weight checks first.

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system_ && src_root_.path().IsParent(dest_root_.path())) {
    callback_.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // It is an error to copy/move an entry into the same path.
  if (same_file_system_ && src_root_.path() == dest_root_.path()) {
    callback_.Run(base::PLATFORM_FILE_ERROR_EXISTS);
    return;
  }

  // Start to process the source directory recursively.
  // TODO(kinuko): This could be too expensive for same_file_system_==true
  // and operation==MOVE case, probably we can just rename the root directory.
  // http://crbug.com/172187
  StartRecursiveOperation(src_root_, callback_);
}

void CopyOrMoveOperationDelegate::ProcessFile(
    const FileSystemURL& src_url,
    const StatusCallback& callback) {
  if (!progress_callback_.is_null()) {
    progress_callback_.Run(
        FileSystemOperation::BEGIN_COPY_ENTRY, src_url, FileSystemURL(), 0);
  }

  FileSystemURL dest_url = CreateDestURL(src_url);
  CopyOrMoveImpl* impl = NULL;
  if (same_file_system_) {
    impl = new CopyOrMoveOnSameFileSystemImpl(
        operation_runner(), operation_type_, src_url, dest_url, option_,
        base::Bind(&CopyOrMoveOperationDelegate::OnCopyFileProgress,
                   weak_factory_.GetWeakPtr(), src_url));
  } else {
    // Cross filesystem case.
    base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
    CopyOrMoveFileValidatorFactory* validator_factory =
        file_system_context()->GetCopyOrMoveFileValidatorFactory(
            dest_root_.type(), &error);
    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    if (!validator_factory) {
      scoped_ptr<webkit_blob::FileStreamReader> reader =
          file_system_context()->CreateFileStreamReader(
              src_url, 0, base::Time());
      scoped_ptr<FileStreamWriter> writer =
          file_system_context()->CreateFileStreamWriter(dest_url, 0);
      if (reader && writer) {
        impl = new StreamCopyOrMoveImpl(
            operation_runner(), operation_type_, src_url, dest_url, option_,
            reader.Pass(), writer.Pass(),
            base::Bind(&CopyOrMoveOperationDelegate::OnCopyFileProgress,
                       weak_factory_.GetWeakPtr(), src_url));
      }
    }

    if (!impl) {
      impl = new SnapshotCopyOrMoveImpl(
          operation_runner(), operation_type_, src_url, dest_url, option_,
          validator_factory,
          base::Bind(&CopyOrMoveOperationDelegate::OnCopyFileProgress,
                     weak_factory_.GetWeakPtr(), src_url));
    }
  }

  // Register the running task.
  running_copy_set_.insert(impl);
  impl->Run(base::Bind(
      &CopyOrMoveOperationDelegate::DidCopyOrMoveFile,
      weak_factory_.GetWeakPtr(), src_url, dest_url, callback, impl));
}

void CopyOrMoveOperationDelegate::ProcessDirectory(
    const FileSystemURL& src_url,
    const StatusCallback& callback) {
  if (src_url == src_root_) {
    // The src_root_ looks to be a directory.
    // Try removing the dest_root_ to see if it exists and/or it is an
    // empty directory.
    // We do not invoke |progress_callback_| for source root, because it is
    // already called in ProcessFile().
    operation_runner()->RemoveDirectory(
        dest_root_,
        base::Bind(&CopyOrMoveOperationDelegate::DidTryRemoveDestRoot,
                   weak_factory_.GetWeakPtr(), callback));
    return;
  }

  if (!progress_callback_.is_null()) {
    progress_callback_.Run(
        FileSystemOperation::BEGIN_COPY_ENTRY, src_url, FileSystemURL(), 0);
  }

  ProcessDirectoryInternal(src_url, CreateDestURL(src_url), callback);
}

void CopyOrMoveOperationDelegate::PostProcessDirectory(
    const FileSystemURL& src_url,
    const StatusCallback& callback) {
  if (option_ == FileSystemOperation::OPTION_NONE) {
    PostProcessDirectoryAfterTouchFile(
        src_url, callback, base::PLATFORM_FILE_OK);
    return;
  }

  operation_runner()->GetMetadata(
      src_url,
      base::Bind(
          &CopyOrMoveOperationDelegate::PostProcessDirectoryAfterGetMetadata,
          weak_factory_.GetWeakPtr(), src_url, callback));
}

void CopyOrMoveOperationDelegate::OnCancel() {
  // Request to cancel all running Copy/Move file.
  for (std::set<CopyOrMoveImpl*>::iterator iter = running_copy_set_.begin();
       iter != running_copy_set_.end(); ++iter)
    (*iter)->Cancel();
}

void CopyOrMoveOperationDelegate::DidCopyOrMoveFile(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback,
    CopyOrMoveImpl* impl,
    base::PlatformFileError error) {
  running_copy_set_.erase(impl);
  delete impl;

  if (!progress_callback_.is_null() && error == base::PLATFORM_FILE_OK) {
    progress_callback_.Run(
        FileSystemOperation::END_COPY_ENTRY, src_url, dest_url, 0);
  }

  callback.Run(error);
}

void CopyOrMoveOperationDelegate::DidTryRemoveDestRoot(
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY) {
    callback_.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    callback_.Run(error);
    return;
  }

  ProcessDirectoryInternal(src_root_, dest_root_, callback);
}

void CopyOrMoveOperationDelegate::ProcessDirectoryInternal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  // If operation_type == Move we may need to record directories and
  // restore directory timestamps in the end, though it may have
  // negative performance impact.
  // See http://crbug.com/171284 for more details.
  operation_runner()->CreateDirectory(
      dest_url, false /* exclusive */, false /* recursive */,
      base::Bind(&CopyOrMoveOperationDelegate::DidCreateDirectory,
                 weak_factory_.GetWeakPtr(), src_url, dest_url, callback));
}

void CopyOrMoveOperationDelegate::DidCreateDirectory(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (!progress_callback_.is_null() && error == base::PLATFORM_FILE_OK) {
    progress_callback_.Run(
        FileSystemOperation::END_COPY_ENTRY, src_url, dest_url, 0);
  }

  callback.Run(error);
}

void CopyOrMoveOperationDelegate::PostProcessDirectoryAfterGetMetadata(
    const FileSystemURL& src_url,
    const StatusCallback& callback,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info) {
  if (error != base::PLATFORM_FILE_OK) {
    // Ignore the error, and run post process which should run after TouchFile.
    PostProcessDirectoryAfterTouchFile(
        src_url, callback, base::PLATFORM_FILE_OK);
    return;
  }

  operation_runner()->TouchFile(
      CreateDestURL(src_url), base::Time::Now() /* last access */,
      file_info.last_modified,
      base::Bind(
          &CopyOrMoveOperationDelegate::PostProcessDirectoryAfterTouchFile,
          weak_factory_.GetWeakPtr(), src_url, callback));
}

void CopyOrMoveOperationDelegate::PostProcessDirectoryAfterTouchFile(
    const FileSystemURL& src_url,
    const StatusCallback& callback,
    base::PlatformFileError error) {
  // Even if the TouchFile is failed, just ignore it.

  if (operation_type_ == OPERATION_COPY) {
    callback.Run(base::PLATFORM_FILE_OK);
    return;
  }

  DCHECK_EQ(OPERATION_MOVE, operation_type_);

  // All files and subdirectories in the directory should be moved here,
  // so remove the source directory for finalizing move operation.
  operation_runner()->Remove(
      src_url, false /* recursive */,
      base::Bind(&CopyOrMoveOperationDelegate::DidRemoveSourceForMove,
                 weak_factory_.GetWeakPtr(), callback));
}

void CopyOrMoveOperationDelegate::DidRemoveSourceForMove(
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    error = base::PLATFORM_FILE_OK;
  callback.Run(error);
}

void CopyOrMoveOperationDelegate::OnCopyFileProgress(
    const FileSystemURL& src_url, int64 size) {
  if (!progress_callback_.is_null()) {
    progress_callback_.Run(
        FileSystemOperation::PROGRESS, src_url, FileSystemURL(), size);
  }
}

FileSystemURL CopyOrMoveOperationDelegate::CreateDestURL(
    const FileSystemURL& src_url) const {
  DCHECK_EQ(src_root_.type(), src_url.type());
  DCHECK_EQ(src_root_.origin(), src_url.origin());

  base::FilePath relative = dest_root_.virtual_path();
  src_root_.virtual_path().AppendRelativePath(src_url.virtual_path(),
                                              &relative);
  return file_system_context()->CreateCrackedFileSystemURL(
      dest_root_.origin(),
      dest_root_.mount_type(),
      relative);
}

}  // namespace fileapi
