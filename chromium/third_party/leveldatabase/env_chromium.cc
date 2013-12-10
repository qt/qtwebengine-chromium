// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <errno.h>
#include <stdio.h>

#include <deque>

#include "base/at_exit.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "chromium_logger.h"
#include "env_chromium.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "port/port.h"
#include "third_party/re2/re2/re2.h"
#include "util/logging.h"

#if defined(OS_WIN)
#include <io.h>
#include "base/win/win_util.h"
#endif

#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif

using namespace leveldb;

namespace leveldb_env {

namespace {

#if (defined(OS_POSIX) && !defined(OS_LINUX)) || defined(OS_WIN)
// The following are glibc-specific

size_t fread_unlocked(void *ptr, size_t size, size_t n, FILE *file) {
  return fread(ptr, size, n, file);
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t n, FILE *file) {
  return fwrite(ptr, size, n, file);
}

int fflush_unlocked(FILE *file) {
  return fflush(file);
}

#if !defined(OS_ANDROID)
int fdatasync(int fildes) {
#if defined(OS_WIN)
  return _commit(fildes);
#else
  return HANDLE_EINTR(fsync(fildes));
#endif
}
#endif

#endif

// Wide-char safe fopen wrapper.
FILE* fopen_internal(const char* fname, const char* mode) {
#if defined(OS_WIN)
  return _wfopen(UTF8ToUTF16(fname).c_str(), ASCIIToUTF16(mode).c_str());
#else
  return fopen(fname, mode);
#endif
}

base::FilePath CreateFilePath(const std::string& file_path) {
#if defined(OS_WIN)
  return base::FilePath(UTF8ToUTF16(file_path));
#else
  return base::FilePath(file_path);
#endif
}

static const base::FilePath::CharType kLevelDBTestDirectoryPrefix[]
    = FILE_PATH_LITERAL("leveldb-test-");

const char* PlatformFileErrorString(const ::base::PlatformFileError& error) {
  switch (error) {
    case ::base::PLATFORM_FILE_ERROR_FAILED:
      return "No further details.";
    case ::base::PLATFORM_FILE_ERROR_IN_USE:
      return "File currently in use.";
    case ::base::PLATFORM_FILE_ERROR_EXISTS:
      return "File already exists.";
    case ::base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return "File not found.";
    case ::base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return "Access denied.";
    case ::base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED:
      return "Too many files open.";
    case ::base::PLATFORM_FILE_ERROR_NO_MEMORY:
      return "Out of memory.";
    case ::base::PLATFORM_FILE_ERROR_NO_SPACE:
      return "No space left on drive.";
    case ::base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      return "Not a directory.";
    case ::base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
      return "Invalid operation.";
    case ::base::PLATFORM_FILE_ERROR_SECURITY:
      return "Security error.";
    case ::base::PLATFORM_FILE_ERROR_ABORT:
      return "File operation aborted.";
    case ::base::PLATFORM_FILE_ERROR_NOT_A_FILE:
      return "The supplied path was not a file.";
    case ::base::PLATFORM_FILE_ERROR_NOT_EMPTY:
      return "The file was not empty.";
    case ::base::PLATFORM_FILE_ERROR_INVALID_URL:
      return "Invalid URL.";
    case ::base::PLATFORM_FILE_ERROR_IO:
      return "OS or hardware error.";
    case ::base::PLATFORM_FILE_OK:
      return "OK.";
    case ::base::PLATFORM_FILE_ERROR_MAX:
      NOTREACHED();
  }
  NOTIMPLEMENTED();
  return "Unknown error.";
}

class ChromiumSequentialFile: public SequentialFile {
 private:
  std::string filename_;
  FILE* file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumSequentialFile(const std::string& fname, FILE* f,
                         const UMALogger* uma_logger)
      : filename_(fname), file_(f), uma_logger_(uma_logger) { }
  virtual ~ChromiumSequentialFile() { fclose(file_); }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s;
    size_t r = fread_unlocked(scratch, 1, n, file_);
    *result = Slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // We leave status as ok if we hit the end of the file
      } else {
        // A partial read with an error: return a non-ok status
        s = MakeIOError(filename_, strerror(errno), kSequentialFileRead, errno);
        uma_logger_->RecordErrorAt(kSequentialFileRead);
      }
    }
    return s;
  }

  virtual Status Skip(uint64_t n) {
    if (fseek(file_, n, SEEK_CUR)) {
      int saved_errno = errno;
      uma_logger_->RecordErrorAt(kSequentialFileSkip);
      return MakeIOError(
          filename_, strerror(saved_errno), kSequentialFileSkip, saved_errno);
    }
    return Status::OK();
  }
};

class ChromiumRandomAccessFile: public RandomAccessFile {
 private:
  std::string filename_;
  ::base::PlatformFile file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumRandomAccessFile(const std::string& fname, ::base::PlatformFile file,
                           const UMALogger* uma_logger)
      : filename_(fname), file_(file), uma_logger_(uma_logger) { }
  virtual ~ChromiumRandomAccessFile() { ::base::ClosePlatformFile(file_); }

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const {
    Status s;
    int r = ::base::ReadPlatformFile(file_, offset, scratch, n);
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = MakeIOError(
          filename_, "Could not perform read", kRandomAccessFileRead);
      uma_logger_->RecordErrorAt(kRandomAccessFileRead);
    }
    return s;
  }
};

class ChromiumFileLock : public FileLock {
 public:
  ::base::PlatformFile file_;
};

class Retrier {
 public:
  Retrier(MethodID method, RetrierProvider* provider)
      : start_(base::TimeTicks::Now()),
        limit_(start_ + base::TimeDelta::FromMilliseconds(
                            provider->MaxRetryTimeMillis())),
        last_(start_),
        time_to_sleep_(base::TimeDelta::FromMilliseconds(10)),
        success_(true),
        method_(method),
        last_error_(base::PLATFORM_FILE_OK),
        provider_(provider) {}
  ~Retrier() {
    if (success_) {
      provider_->GetRetryTimeHistogram(method_)->AddTime(last_ - start_);
      if (last_error_ != base::PLATFORM_FILE_OK) {
        DCHECK(last_error_ < 0);
        provider_->GetRecoveredFromErrorHistogram(method_)->Add(-last_error_);
      }
    }
  }
  bool ShouldKeepTrying(base::PlatformFileError last_error) {
    DCHECK_NE(last_error, base::PLATFORM_FILE_OK);
    last_error_ = last_error;
    if (last_ < limit_) {
      base::PlatformThread::Sleep(time_to_sleep_);
      last_ = base::TimeTicks::Now();
      return true;
    }
    success_ = false;
    return false;
  }

 private:
  base::TimeTicks start_;
  base::TimeTicks limit_;
  base::TimeTicks last_;
  base::TimeDelta time_to_sleep_;
  bool success_;
  MethodID method_;
  base::PlatformFileError last_error_;
  RetrierProvider* provider_;
};

class IDBEnv : public ChromiumEnv {
 public:
  IDBEnv() : ChromiumEnv() { name_ = "LevelDBEnv.IDB"; }
};

::base::LazyInstance<IDBEnv>::Leaky idb_env = LAZY_INSTANCE_INITIALIZER;

::base::LazyInstance<ChromiumEnv>::Leaky default_env =
    LAZY_INSTANCE_INITIALIZER;

}  // unnamed namespace

const char* MethodIDToString(MethodID method) {
  switch (method) {
    case kSequentialFileRead:
      return "SequentialFileRead";
    case kSequentialFileSkip:
      return "SequentialFileSkip";
    case kRandomAccessFileRead:
      return "RandomAccessFileRead";
    case kWritableFileAppend:
      return "WritableFileAppend";
    case kWritableFileClose:
      return "WritableFileClose";
    case kWritableFileFlush:
      return "WritableFileFlush";
    case kWritableFileSync:
      return "WritableFileSync";
    case kNewSequentialFile:
      return "NewSequentialFile";
    case kNewRandomAccessFile:
      return "NewRandomAccessFile";
    case kNewWritableFile:
      return "NewWritableFile";
    case kDeleteFile:
      return "DeleteFile";
    case kCreateDir:
      return "CreateDir";
    case kDeleteDir:
      return "DeleteDir";
    case kGetFileSize:
      return "GetFileSize";
    case kRenameFile:
      return "RenameFile";
    case kLockFile:
      return "LockFile";
    case kUnlockFile:
      return "UnlockFile";
    case kGetTestDirectory:
      return "GetTestDirectory";
    case kNewLogger:
      return "NewLogger";
    case kSyncParent:
      return "SyncParent";
    case kNumEntries:
      NOTREACHED();
      return "kNumEntries";
  }
  NOTREACHED();
  return "Unknown";
}

Status MakeIOError(Slice filename,
                   const char* message,
                   MethodID method,
                   int saved_errno) {
  char buf[512];
  snprintf(buf,
           sizeof(buf),
           "%s (ChromeMethodErrno: %d::%s::%d)",
           message,
           method,
           MethodIDToString(method),
           saved_errno);
  return Status::IOError(filename, buf);
}

Status MakeIOError(Slice filename,
                   const char* message,
                   MethodID method,
                   base::PlatformFileError error) {
  DCHECK(error < 0);
  char buf[512];
  snprintf(buf,
           sizeof(buf),
           "%s (ChromeMethodPFE: %d::%s::%d)",
           message,
           method,
           MethodIDToString(method),
           -error);
  return Status::IOError(filename, buf);
}

Status MakeIOError(Slice filename, const char* message, MethodID method) {
  char buf[512];
  snprintf(buf,
           sizeof(buf),
           "%s (ChromeMethodOnly: %d::%s)",
           message,
           method,
           MethodIDToString(method));
  return Status::IOError(filename, buf);
}

ErrorParsingResult ParseMethodAndError(const char* string,
                                       MethodID* method_param,
                                       int* error) {
  int method;
  if (RE2::PartialMatch(string, "ChromeMethodOnly: (\\d+)", &method)) {
    *method_param = static_cast<MethodID>(method);
    return METHOD_ONLY;
  }
  if (RE2::PartialMatch(
          string, "ChromeMethodPFE: (\\d+)::.*::(\\d+)", &method, error)) {
    *error = -*error;
    *method_param = static_cast<MethodID>(method);
    return METHOD_AND_PFE;
  }
  if (RE2::PartialMatch(
          string, "ChromeMethodErrno: (\\d+)::.*::(\\d+)", &method, error)) {
    *method_param = static_cast<MethodID>(method);
    return METHOD_AND_ERRNO;
  }
  return NONE;
}

bool IndicatesDiskFull(leveldb::Status status) {
  if (status.ok())
    return false;
  leveldb_env::MethodID method;
  int error = -1;
  leveldb_env::ErrorParsingResult result = leveldb_env::ParseMethodAndError(
      status.ToString().c_str(), &method, &error);
  return (result == leveldb_env::METHOD_AND_PFE &&
          static_cast<base::PlatformFileError>(error) ==
              base::PLATFORM_FILE_ERROR_NO_SPACE) ||
         (result == leveldb_env::METHOD_AND_ERRNO && error == ENOSPC);
}

std::string FilePathToString(const base::FilePath& file_path) {
#if defined(OS_WIN)
  return UTF16ToUTF8(file_path.value());
#else
  return file_path.value();
#endif
}

ChromiumWritableFile::ChromiumWritableFile(const std::string& fname,
                                           FILE* f,
                                           const UMALogger* uma_logger,
                                           WriteTracker* tracker)
    : filename_(fname), file_(f), uma_logger_(uma_logger), tracker_(tracker) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(fname);
  is_manifest_ =
      FilePathToString(path.BaseName()).find("MANIFEST") !=
      std::string::npos;
  if (!is_manifest_)
    tracker_->DidCreateNewFile(filename_);
  parent_dir_ = FilePathToString(CreateFilePath(fname).DirName());
}

ChromiumWritableFile::~ChromiumWritableFile() {
  if (file_ != NULL) {
    // Ignoring any potential errors
    fclose(file_);
  }
}

Status ChromiumWritableFile::SyncParent() {
  Status s;
#if !defined(OS_WIN)
  TRACE_EVENT0("leveldb", "SyncParent");

  int parent_fd =
      HANDLE_EINTR(open(parent_dir_.c_str(), O_RDONLY));
  if (parent_fd < 0) {
    int saved_errno = errno;
    return MakeIOError(
        parent_dir_, strerror(saved_errno), kSyncParent, saved_errno);
  }
  if (HANDLE_EINTR(fsync(parent_fd)) != 0) {
    int saved_errno = errno;
    s = MakeIOError(
        parent_dir_, strerror(saved_errno), kSyncParent, saved_errno);
  };
  HANDLE_EINTR(close(parent_fd));
#endif
  return s;
}

Status ChromiumWritableFile::Append(const Slice& data) {
  if (is_manifest_ && tracker_->DoesDirNeedSync(filename_)) {
    Status s = SyncParent();
    if (!s.ok())
      return s;
    tracker_->DidSyncDir(filename_);
  }

  size_t r = fwrite_unlocked(data.data(), 1, data.size(), file_);
  if (r != data.size()) {
    int saved_errno = errno;
    uma_logger_->RecordOSError(kWritableFileAppend, saved_errno);
    return MakeIOError(
        filename_, strerror(saved_errno), kWritableFileAppend, saved_errno);
  }
  return Status::OK();
}

Status ChromiumWritableFile::Close() {
  Status result;
  if (fclose(file_) != 0) {
    result = MakeIOError(filename_, strerror(errno), kWritableFileClose, errno);
    uma_logger_->RecordErrorAt(kWritableFileClose);
  }
  file_ = NULL;
  return result;
}

Status ChromiumWritableFile::Flush() {
  Status result;
  if (HANDLE_EINTR(fflush_unlocked(file_))) {
    int saved_errno = errno;
    result = MakeIOError(
        filename_, strerror(saved_errno), kWritableFileFlush, saved_errno);
    uma_logger_->RecordOSError(kWritableFileFlush, saved_errno);
  }
  return result;
}

Status ChromiumWritableFile::Sync() {
  TRACE_EVENT0("leveldb", "ChromiumEnv::Sync");
  Status result;
  int error = 0;

  if (HANDLE_EINTR(fflush_unlocked(file_)))
    error = errno;
  // Sync even if fflush gave an error; perhaps the data actually got out,
  // even though something went wrong.
  if (fdatasync(fileno(file_)) && !error)
    error = errno;
  // Report the first error we found.
  if (error) {
    result = MakeIOError(filename_, strerror(error), kWritableFileSync, error);
    uma_logger_->RecordErrorAt(kWritableFileSync);
  }
  return result;
}

ChromiumEnv::ChromiumEnv()
    : name_("LevelDBEnv"),
      bgsignal_(&mu_),
      started_bgthread_(false),
      kMaxRetryTimeMillis(1000) {
}

ChromiumEnv::~ChromiumEnv() {
  // In chromium, ChromiumEnv is leaked. It'd be nice to add NOTREACHED here to
  // ensure that behavior isn't accidentally changed, but there's an instance in
  // a unit test that is deleted.
}

Status ChromiumEnv::NewSequentialFile(const std::string& fname,
                                      SequentialFile** result) {
  FILE* f = fopen_internal(fname.c_str(), "rb");
  if (f == NULL) {
    *result = NULL;
    int saved_errno = errno;
    RecordOSError(kNewSequentialFile, saved_errno);
    return MakeIOError(
        fname, strerror(saved_errno), kNewSequentialFile, saved_errno);
  } else {
    *result = new ChromiumSequentialFile(fname, f, this);
    return Status::OK();
  }
}

void ChromiumEnv::RecordOpenFilesLimit(const std::string& type) {
#if defined(OS_POSIX)
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile))
    return;
  GetMaxFDHistogram(type)->Add(nofile.rlim_cur);
#endif
}

Status ChromiumEnv::NewRandomAccessFile(const std::string& fname,
                                        RandomAccessFile** result) {
  int flags = ::base::PLATFORM_FILE_READ | ::base::PLATFORM_FILE_OPEN;
  bool created;
  ::base::PlatformFileError error_code;
  ::base::PlatformFile file = ::base::CreatePlatformFile(
      CreateFilePath(fname), flags, &created, &error_code);
  if (error_code == ::base::PLATFORM_FILE_OK) {
    *result = new ChromiumRandomAccessFile(fname, file, this);
    RecordOpenFilesLimit("Success");
    return Status::OK();
  }
  if (error_code == ::base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED)
    RecordOpenFilesLimit("TooManyOpened");
  else
    RecordOpenFilesLimit("OtherError");
  *result = NULL;
  RecordOSError(kNewRandomAccessFile, error_code);
  return MakeIOError(fname,
                     PlatformFileErrorString(error_code),
                     kNewRandomAccessFile,
                     error_code);
}

Status ChromiumEnv::NewWritableFile(const std::string& fname,
                                    WritableFile** result) {
  *result = NULL;
  FILE* f = fopen_internal(fname.c_str(), "wb");
  if (f == NULL) {
    int saved_errno = errno;
    RecordErrorAt(kNewWritableFile);
    return MakeIOError(
        fname, strerror(saved_errno), kNewWritableFile, saved_errno);
  } else {
    *result = new ChromiumWritableFile(fname, f, this, this);
    return Status::OK();
  }
}

bool ChromiumEnv::FileExists(const std::string& fname) {
  return ::base::PathExists(CreateFilePath(fname));
}

Status ChromiumEnv::GetChildren(const std::string& dir,
                                std::vector<std::string>* result) {
  result->clear();
  base::FileEnumerator iter(
      CreateFilePath(dir), false, base::FileEnumerator::FILES);
  base::FilePath current = iter.Next();
  while (!current.empty()) {
    result->push_back(FilePathToString(current.BaseName()));
    current = iter.Next();
  }
  // TODO(jorlow): Unfortunately, the FileEnumerator swallows errors, so
  //               we'll always return OK. Maybe manually check for error
  //               conditions like the file not existing?
  return Status::OK();
}

Status ChromiumEnv::DeleteFile(const std::string& fname) {
  Status result;
  // TODO(jorlow): Should we assert this is a file?
  if (!::base::DeleteFile(CreateFilePath(fname), false)) {
    result = MakeIOError(fname, "Could not delete file.", kDeleteFile);
    RecordErrorAt(kDeleteFile);
  }
  return result;
}

Status ChromiumEnv::CreateDir(const std::string& name) {
  Status result;
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  Retrier retrier(kCreateDir, this);
  do {
    if (::file_util::CreateDirectoryAndGetError(CreateFilePath(name), &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));
  result = MakeIOError(name, "Could not create directory.", kCreateDir, error);
  RecordOSError(kCreateDir, error);
  return result;
}

Status ChromiumEnv::DeleteDir(const std::string& name) {
  Status result;
  // TODO(jorlow): Should we assert this is a directory?
  if (!::base::DeleteFile(CreateFilePath(name), false)) {
    result = MakeIOError(name, "Could not delete directory.", kDeleteDir);
    RecordErrorAt(kDeleteDir);
  }
  return result;
}

Status ChromiumEnv::GetFileSize(const std::string& fname, uint64_t* size) {
  Status s;
  int64_t signed_size;
  if (!::file_util::GetFileSize(CreateFilePath(fname), &signed_size)) {
    *size = 0;
    s = MakeIOError(fname, "Could not determine file size.", kGetFileSize);
    RecordErrorAt(kGetFileSize);
  } else {
    *size = static_cast<uint64_t>(signed_size);
  }
  return s;
}

Status ChromiumEnv::RenameFile(const std::string& src, const std::string& dst) {
  Status result;
  base::FilePath src_file_path = CreateFilePath(src);
  if (!::base::PathExists(src_file_path))
    return result;
  base::FilePath destination = CreateFilePath(dst);

  Retrier retrier(kRenameFile, this);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  do {
    if (base::ReplaceFile(src_file_path, destination, &error))
      return result;
  } while (retrier.ShouldKeepTrying(error));

  DCHECK(error != base::PLATFORM_FILE_OK);
  RecordOSError(kRenameFile, error);
  char buf[100];
  snprintf(buf,
           sizeof(buf),
           "Could not rename file: %s",
           PlatformFileErrorString(error));
  return MakeIOError(src, buf, kRenameFile, error);
}

Status ChromiumEnv::LockFile(const std::string& fname, FileLock** lock) {
  *lock = NULL;
  Status result;
  int flags = ::base::PLATFORM_FILE_OPEN_ALWAYS |
              ::base::PLATFORM_FILE_READ |
              ::base::PLATFORM_FILE_WRITE |
              ::base::PLATFORM_FILE_EXCLUSIVE_READ |
              ::base::PLATFORM_FILE_EXCLUSIVE_WRITE;
  bool created;
  ::base::PlatformFileError error_code;
  ::base::PlatformFile file;
  Retrier retrier(kLockFile, this);
  do {
    file = ::base::CreatePlatformFile(
        CreateFilePath(fname), flags, &created, &error_code);
  } while (error_code != ::base::PLATFORM_FILE_OK &&
           retrier.ShouldKeepTrying(error_code));

  if (error_code == ::base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    ::base::FilePath parent = CreateFilePath(fname).DirName();
    ::base::FilePath last_parent;
    int num_missing_ancestors = 0;
    do {
      if (base::DirectoryExists(parent))
        break;
      ++num_missing_ancestors;
      last_parent = parent;
      parent = parent.DirName();
    } while (parent != last_parent);
    RecordLockFileAncestors(num_missing_ancestors);
  }

  if (error_code != ::base::PLATFORM_FILE_OK) {
    result = MakeIOError(
        fname, PlatformFileErrorString(error_code), kLockFile, error_code);
    RecordOSError(kLockFile, error_code);
  } else {
    ChromiumFileLock* my_lock = new ChromiumFileLock;
    my_lock->file_ = file;
    *lock = my_lock;
  }
  return result;
}

Status ChromiumEnv::UnlockFile(FileLock* lock) {
  ChromiumFileLock* my_lock = reinterpret_cast<ChromiumFileLock*>(lock);
  Status result;
  if (!::base::ClosePlatformFile(my_lock->file_)) {
    result = MakeIOError("Could not close lock file.", "", kUnlockFile);
    RecordErrorAt(kUnlockFile);
  }
  delete my_lock;
  return result;
}

Status ChromiumEnv::GetTestDirectory(std::string* path) {
  mu_.Acquire();
  if (test_directory_.empty()) {
    if (!::file_util::CreateNewTempDirectory(kLevelDBTestDirectoryPrefix,
                                             &test_directory_)) {
      mu_.Release();
      RecordErrorAt(kGetTestDirectory);
      return MakeIOError(
          "Could not create temp directory.", "", kGetTestDirectory);
    }
  }
  *path = FilePathToString(test_directory_);
  mu_.Release();
  return Status::OK();
}

Status ChromiumEnv::NewLogger(const std::string& fname, Logger** result) {
  FILE* f = fopen_internal(fname.c_str(), "w");
  if (f == NULL) {
    *result = NULL;
    int saved_errno = errno;
    RecordOSError(kNewLogger, saved_errno);
    return MakeIOError(fname, strerror(saved_errno), kNewLogger, saved_errno);
  } else {
    *result = new ChromiumLogger(f);
    return Status::OK();
  }
}

uint64_t ChromiumEnv::NowMicros() {
  return ::base::TimeTicks::Now().ToInternalValue();
}

void ChromiumEnv::SleepForMicroseconds(int micros) {
  // Round up to the next millisecond.
  ::base::PlatformThread::Sleep(::base::TimeDelta::FromMicroseconds(micros));
}

void ChromiumEnv::RecordErrorAt(MethodID method) const {
  GetMethodIOErrorHistogram()->Add(method);
}

void ChromiumEnv::RecordLockFileAncestors(int num_missing_ancestors) const {
  GetLockFileAncestorHistogram()->Add(num_missing_ancestors);
}

void ChromiumEnv::RecordOSError(MethodID method,
                                base::PlatformFileError error) const {
  DCHECK(error < 0);
  RecordErrorAt(method);
  GetOSErrorHistogram(method, -base::PLATFORM_FILE_ERROR_MAX)->Add(-error);
}

void ChromiumEnv::RecordOSError(MethodID method, int error) const {
  DCHECK(error > 0);
  RecordErrorAt(method);
  GetOSErrorHistogram(method, ERANGE + 1)->Add(error);
}

base::HistogramBase* ChromiumEnv::GetOSErrorHistogram(MethodID method,
                                                      int limit) const {
  std::string uma_name(name_);
  // TODO(dgrogan): This is probably not the best way to concatenate strings.
  uma_name.append(".IOError.").append(MethodIDToString(method));
  return base::LinearHistogram::FactoryGet(uma_name, 1, limit, limit + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetMethodIOErrorHistogram() const {
  std::string uma_name(name_);
  uma_name.append(".IOError");
  return base::LinearHistogram::FactoryGet(uma_name, 1, kNumEntries,
      kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetMaxFDHistogram(
    const std::string& type) const {
  std::string uma_name(name_);
  uma_name.append(".MaxFDs.").append(type);
  // These numbers make each bucket twice as large as the previous bucket.
  const int kFirstEntry = 1;
  const int kLastEntry = 65536;
  const int kNumBuckets = 18;
  return base::Histogram::FactoryGet(
      uma_name, kFirstEntry, kLastEntry, kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetLockFileAncestorHistogram() const {
  std::string uma_name(name_);
  uma_name.append(".LockFileAncestorsNotFound");
  const int kMin = 1;
  const int kMax = 10;
  const int kNumBuckets = 11;
  return base::LinearHistogram::FactoryGet(
      uma_name, kMin, kMax, kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetRetryTimeHistogram(MethodID method) const {
  std::string uma_name(name_);
  // TODO(dgrogan): This is probably not the best way to concatenate strings.
  uma_name.append(".TimeUntilSuccessFor").append(MethodIDToString(method));

  const int kBucketSizeMillis = 25;
  // Add 2, 1 for each of the buckets <1 and >max.
  const int kNumBuckets = kMaxRetryTimeMillis / kBucketSizeMillis + 2;
  return base::Histogram::FactoryTimeGet(
      uma_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(kMaxRetryTimeMillis + 1),
      kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

base::HistogramBase* ChromiumEnv::GetRecoveredFromErrorHistogram(
    MethodID method) const {
  std::string uma_name(name_);
  uma_name.append(".RetryRecoveredFromErrorIn")
      .append(MethodIDToString(method));
  return base::LinearHistogram::FactoryGet(uma_name, 1, kNumEntries,
      kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

class Thread : public ::base::PlatformThread::Delegate {
 public:
  Thread(void (*function)(void* arg), void* arg)
      : function_(function), arg_(arg) {
    ::base::PlatformThreadHandle handle;
    bool success = ::base::PlatformThread::Create(0, this, &handle);
    DCHECK(success);
  }
  virtual ~Thread() {}
  virtual void ThreadMain() {
    (*function_)(arg_);
    delete this;
  }

 private:
  void (*function_)(void* arg);
  void* arg_;
};

void ChromiumEnv::Schedule(void (*function)(void*), void* arg) {
  mu_.Acquire();

  // Start background thread if necessary
  if (!started_bgthread_) {
    started_bgthread_ = true;
    StartThread(&ChromiumEnv::BGThreadWrapper, this);
  }

  // If the queue is currently empty, the background thread may currently be
  // waiting.
  if (queue_.empty()) {
    bgsignal_.Signal();
  }

  // Add to priority queue
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  mu_.Release();
}

void ChromiumEnv::BGThread() {
  base::PlatformThread::SetName(name_.c_str());

  while (true) {
    // Wait until there is an item that is ready to run
    mu_.Acquire();
    while (queue_.empty()) {
      bgsignal_.Wait();
    }

    void (*function)(void*) = queue_.front().function;
    void* arg = queue_.front().arg;
    queue_.pop_front();

    mu_.Release();
    TRACE_EVENT0("leveldb", "ChromiumEnv::BGThread-Task");
    (*function)(arg);
  }
}

void ChromiumEnv::StartThread(void (*function)(void* arg), void* arg) {
  new Thread(function, arg); // Will self-delete.
}

static std::string GetDirName(const std::string& filename) {
  base::FilePath file = base::FilePath::FromUTF8Unsafe(filename);
  return FilePathToString(file.DirName());
}

void ChromiumEnv::DidCreateNewFile(const std::string& filename) {
  base::AutoLock auto_lock(map_lock_);
  needs_sync_map_[GetDirName(filename)] = true;
}

bool ChromiumEnv::DoesDirNeedSync(const std::string& filename) {
  base::AutoLock auto_lock(map_lock_);
  return needs_sync_map_.find(GetDirName(filename)) != needs_sync_map_.end();
}

void ChromiumEnv::DidSyncDir(const std::string& filename) {
  base::AutoLock auto_lock(map_lock_);
  needs_sync_map_.erase(GetDirName(filename));
}

}  // namespace leveldb_env

namespace leveldb {

Env* IDBEnv() {
  return leveldb_env::idb_env.Pointer();
}

Env* Env::Default() {
  return leveldb_env::default_env.Pointer();
}

}  // namespace leveldb

