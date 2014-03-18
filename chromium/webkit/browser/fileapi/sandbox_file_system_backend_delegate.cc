// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_file_system_backend_delegate.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "net/base/net_util.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/quota/quota_backend_impl.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"
#include "webkit/browser/fileapi/quota/quota_reservation_manager.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

namespace {

const char kTemporaryOriginsCountLabel[] = "FileSystem.TemporaryOriginsCount";
const char kPersistentOriginsCountLabel[] = "FileSystem.PersistentOriginsCount";

const char kOpenFileSystemLabel[] = "FileSystem.OpenFileSystem";
const char kOpenFileSystemDetailLabel[] = "FileSystem.OpenFileSystemDetail";
const char kOpenFileSystemDetailNonThrottledLabel[] =
    "FileSystem.OpenFileSystemDetailNonthrottled";
int64 kMinimumStatsCollectionIntervalHours = 1;

// For type directory names in ObfuscatedFileUtil.
// TODO(kinuko,nhiroki): Each type string registration should be done
// via its own backend.
const char kTemporaryDirectoryName[] = "t";
const char kPersistentDirectoryName[] = "p";
const char kSyncableDirectoryName[] = "s";

const char* kPrepopulateTypes[] = {
  kPersistentDirectoryName,
  kTemporaryDirectoryName
};

enum FileSystemError {
  kOK = 0,
  kIncognito,
  kInvalidSchemeError,
  kCreateDirectoryError,
  kNotFound,
  kUnknownError,
  kFileSystemErrorMax,
};

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
const base::FilePath::CharType* const kRestrictedNames[] = {
  FILE_PATH_LITERAL("."), FILE_PATH_LITERAL(".."),
};

// Restricted chars.
const base::FilePath::CharType kRestrictedChars[] = {
  FILE_PATH_LITERAL('/'), FILE_PATH_LITERAL('\\'),
};

std::string GetTypeStringForURL(const FileSystemURL& url) {
  return SandboxFileSystemBackendDelegate::GetTypeString(url.type());
}

std::set<std::string> GetKnownTypeStrings() {
  std::set<std::string> known_type_strings;
  known_type_strings.insert(kTemporaryDirectoryName);
  known_type_strings.insert(kPersistentDirectoryName);
  known_type_strings.insert(kSyncableDirectoryName);
  return known_type_strings;
}

class ObfuscatedOriginEnumerator
    : public SandboxFileSystemBackendDelegate::OriginEnumerator {
 public:
  explicit ObfuscatedOriginEnumerator(ObfuscatedFileUtil* file_util) {
    enum_.reset(file_util->CreateOriginEnumerator());
  }
  virtual ~ObfuscatedOriginEnumerator() {}

  virtual GURL Next() OVERRIDE {
    return enum_->Next();
  }

  virtual bool HasFileSystemType(FileSystemType type) const OVERRIDE {
    return enum_->HasTypeDirectory(
        SandboxFileSystemBackendDelegate::GetTypeString(type));
  }

 private:
  scoped_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enum_;
};

void OpenFileSystemOnFileThread(
    ObfuscatedFileUtil* file_util,
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    base::PlatformFileError* error_ptr) {
  DCHECK(error_ptr);
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  file_util->GetDirectoryForOriginAndType(
      origin_url, SandboxFileSystemBackendDelegate::GetTypeString(type),
      create, error_ptr);
  if (*error_ptr != base::PLATFORM_FILE_OK) {
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel,
                              kCreateDirectoryError,
                              kFileSystemErrorMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel, kOK, kFileSystemErrorMax);
  }
  // The reference of file_util will be derefed on the FILE thread
  // when the storage of this callback gets deleted regardless of whether
  // this method is called or not.
}

void DidOpenFileSystem(
    base::WeakPtr<SandboxFileSystemBackendDelegate> delegate,
    const base::Callback<void(base::PlatformFileError error)>& callback,
    base::PlatformFileError* error) {
  if (delegate.get())
    delegate.get()->CollectOpenFileSystemMetrics(*error);
  callback.Run(*error);
}

template <typename T>
void DeleteSoon(base::SequencedTaskRunner* runner, T* ptr) {
  if (!runner->DeleteSoon(FROM_HERE, ptr))
    delete ptr;
}

}  // namespace

const base::FilePath::CharType
SandboxFileSystemBackendDelegate::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("File System");

// static
std::string SandboxFileSystemBackendDelegate::GetTypeString(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return kTemporaryDirectoryName;
    case kFileSystemTypePersistent:
      return kPersistentDirectoryName;
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return kSyncableDirectoryName;
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED() << "Unknown filesystem type requested:" << type;
      return std::string();
  }
}

SandboxFileSystemBackendDelegate::SandboxFileSystemBackendDelegate(
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& profile_path,
    quota::SpecialStoragePolicy* special_storage_policy,
    const FileSystemOptions& file_system_options)
    : file_task_runner_(file_task_runner),
      sandbox_file_util_(new AsyncFileUtilAdapter(
          new ObfuscatedFileUtil(
              special_storage_policy,
              profile_path.Append(kFileSystemDirectory),
              file_task_runner,
              base::Bind(&GetTypeStringForURL),
              GetKnownTypeStrings(),
              this))),
      file_system_usage_cache_(new FileSystemUsageCache(file_task_runner)),
      quota_observer_(new SandboxQuotaObserver(
          quota_manager_proxy,
          file_task_runner,
          obfuscated_file_util(),
          usage_cache())),
      quota_reservation_manager_(new QuotaReservationManager(
          scoped_ptr<QuotaReservationManager::QuotaBackend>(
              new QuotaBackendImpl(file_task_runner_,
                                   obfuscated_file_util(),
                                   usage_cache(),
                                   quota_manager_proxy)))),
      special_storage_policy_(special_storage_policy),
      file_system_options_(file_system_options),
      is_filesystem_opened_(false),
      weak_factory_(this) {
  // Prepopulate database only if it can run asynchronously (i.e. the current
  // thread is not file_task_runner). Usually this is the case but may not
  // in test code.
  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    std::vector<std::string> types_to_prepopulate(
        &kPrepopulateTypes[0],
        &kPrepopulateTypes[arraysize(kPrepopulateTypes)]);
    file_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ObfuscatedFileUtil::MaybePrepopulateDatabase,
                  base::Unretained(obfuscated_file_util()),
                  types_to_prepopulate));
  }
}

SandboxFileSystemBackendDelegate::~SandboxFileSystemBackendDelegate() {
  io_thread_checker_.DetachFromThread();

  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    DeleteSoon(file_task_runner_.get(), quota_reservation_manager_.release());
    DeleteSoon(file_task_runner_.get(), sandbox_file_util_.release());
    DeleteSoon(file_task_runner_.get(), quota_observer_.release());
    DeleteSoon(file_task_runner_.get(), file_system_usage_cache_.release());
  }
}

SandboxFileSystemBackendDelegate::OriginEnumerator*
SandboxFileSystemBackendDelegate::CreateOriginEnumerator() {
  return new ObfuscatedOriginEnumerator(obfuscated_file_util());
}

base::FilePath
SandboxFileSystemBackendDelegate::GetBaseDirectoryForOriginAndType(
    const GURL& origin_url,
    FileSystemType type,
    bool create) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = obfuscated_file_util()->GetDirectoryForOriginAndType(
      origin_url, GetTypeString(type), create, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return path;
}

void SandboxFileSystemBackendDelegate::OpenFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback,
    const GURL& root_url) {
  if (!IsAllowedScheme(origin_url)) {
    callback.Run(GURL(), std::string(), base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  std::string name = GetFileSystemName(origin_url, type);

  base::PlatformFileError* error_ptr = new base::PlatformFileError;
  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&OpenFileSystemOnFileThread,
                 obfuscated_file_util(), origin_url, type, mode,
                 base::Unretained(error_ptr)),
      base::Bind(&DidOpenFileSystem,
                 weak_factory_.GetWeakPtr(),
                 base::Bind(callback, root_url, name),
                 base::Owned(error_ptr)));

  io_thread_checker_.DetachFromThread();
  is_filesystem_opened_ = true;
}

scoped_ptr<FileSystemOperationContext>
SandboxFileSystemBackendDelegate::CreateFileSystemOperationContext(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  if (!IsAccessValid(url)) {
    *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
    return scoped_ptr<FileSystemOperationContext>();
  }

  const UpdateObserverList* update_observers = GetUpdateObservers(url.type());
  const ChangeObserverList* change_observers = GetChangeObservers(url.type());
  DCHECK(update_observers);

  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(*update_observers);
  operation_context->set_change_observers(
      change_observers ? *change_observers : ChangeObserverList());

  return operation_context.Pass();
}

scoped_ptr<webkit_blob::FileStreamReader>
SandboxFileSystemBackendDelegate::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  if (!IsAccessValid(url))
    return scoped_ptr<webkit_blob::FileStreamReader>();
  return scoped_ptr<webkit_blob::FileStreamReader>(
      webkit_blob::FileStreamReader::CreateForFileSystemFile(
          context, url, offset, expected_modification_time));
}

scoped_ptr<FileStreamWriter>
SandboxFileSystemBackendDelegate::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context,
    FileSystemType type) const {
  if (!IsAccessValid(url))
    return scoped_ptr<FileStreamWriter>();
  const UpdateObserverList* observers = GetUpdateObservers(type);
  DCHECK(observers);
  return scoped_ptr<FileStreamWriter>(
      new SandboxFileStreamWriter(context, url, offset, *observers));
}

base::PlatformFileError
SandboxFileSystemBackendDelegate::DeleteOriginDataOnFileThread(
    FileSystemContext* file_system_context,
    quota::QuotaManagerProxy* proxy,
    const GURL& origin_url,
    FileSystemType type) {
  int64 usage = GetOriginUsageOnFileThread(
      file_system_context, origin_url, type);
  usage_cache()->CloseCacheFiles();
  bool result = obfuscated_file_util()->DeleteDirectoryForOriginAndType(
      origin_url, GetTypeString(type));
  if (result && proxy) {
    proxy->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        origin_url,
        FileSystemTypeToQuotaStorageType(type),
        -usage);
  }

  if (result)
    return base::PLATFORM_FILE_OK;
  return base::PLATFORM_FILE_ERROR_FAILED;
}

void SandboxFileSystemBackendDelegate::GetOriginsForTypeOnFileThread(
    FileSystemType type, std::set<GURL>* origins) {
  DCHECK(origins);
  scoped_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (enumerator->HasFileSystemType(type))
      origins->insert(origin);
  }
  switch (type) {
    case kFileSystemTypeTemporary:
      UMA_HISTOGRAM_COUNTS(kTemporaryOriginsCountLabel, origins->size());
      break;
    case kFileSystemTypePersistent:
      UMA_HISTOGRAM_COUNTS(kPersistentOriginsCountLabel, origins->size());
      break;
    default:
      break;
  }
}

void SandboxFileSystemBackendDelegate::GetOriginsForHostOnFileThread(
    FileSystemType type, const std::string& host,
    std::set<GURL>* origins) {
  DCHECK(origins);
  scoped_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (host == net::GetHostOrSpecFromURL(origin) &&
        enumerator->HasFileSystemType(type))
      origins->insert(origin);
  }
}

int64 SandboxFileSystemBackendDelegate::GetOriginUsageOnFileThread(
    FileSystemContext* file_system_context,
    const GURL& origin_url,
    FileSystemType type) {
  // Don't use usage cache and return recalculated usage for sticky invalidated
  // origins.
  if (ContainsKey(sticky_dirty_origins_, std::make_pair(origin_url, type)))
    return RecalculateUsage(file_system_context, origin_url, type);

  base::FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty() || !base::DirectoryExists(base_path))
    return 0;
  base::FilePath usage_file_path =
      base_path.Append(FileSystemUsageCache::kUsageFileName);

  bool is_valid = usage_cache()->IsValid(usage_file_path);
  uint32 dirty_status = 0;
  bool dirty_status_available =
      usage_cache()->GetDirty(usage_file_path, &dirty_status);
  bool visited = !visited_origins_.insert(origin_url).second;
  if (is_valid && (dirty_status == 0 || (dirty_status_available && visited))) {
    // The usage cache is clean (dirty == 0) or the origin is already
    // initialized and running.  Read the cache file to get the usage.
    int64 usage = 0;
    return usage_cache()->GetUsage(usage_file_path, &usage) ? usage : -1;
  }
  // The usage cache has not been initialized or the cache is dirty.
  // Get the directory size now and update the cache.
  usage_cache()->Delete(usage_file_path);

  int64 usage = RecalculateUsage(file_system_context, origin_url, type);

  // This clears the dirty flag too.
  usage_cache()->UpdateUsage(usage_file_path, usage);
  return usage;
}

scoped_refptr<QuotaReservation>
SandboxFileSystemBackendDelegate::CreateQuotaReservationOnFileTaskRunner(
    const GURL& origin,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(quota_reservation_manager_);
  return quota_reservation_manager_->CreateReservation(origin, type);
}

void SandboxFileSystemBackendDelegate::AddFileUpdateObserver(
    FileSystemType type,
    FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
  update_observers_[type] =
      update_observers_[type].AddObserver(observer, task_runner);
}

void SandboxFileSystemBackendDelegate::AddFileChangeObserver(
    FileSystemType type,
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
  change_observers_[type] =
      change_observers_[type].AddObserver(observer, task_runner);
}

void SandboxFileSystemBackendDelegate::AddFileAccessObserver(
    FileSystemType type,
    FileAccessObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
  access_observers_[type] =
      access_observers_[type].AddObserver(observer, task_runner);
}

const UpdateObserverList* SandboxFileSystemBackendDelegate::GetUpdateObservers(
    FileSystemType type) const {
  std::map<FileSystemType, UpdateObserverList>::const_iterator iter =
      update_observers_.find(type);
  if (iter == update_observers_.end())
    return NULL;
  return &iter->second;
}

const ChangeObserverList* SandboxFileSystemBackendDelegate::GetChangeObservers(
    FileSystemType type) const {
  std::map<FileSystemType, ChangeObserverList>::const_iterator iter =
      change_observers_.find(type);
  if (iter == change_observers_.end())
    return NULL;
  return &iter->second;
}

const AccessObserverList* SandboxFileSystemBackendDelegate::GetAccessObservers(
    FileSystemType type) const {
  std::map<FileSystemType, AccessObserverList>::const_iterator iter =
      access_observers_.find(type);
  if (iter == access_observers_.end())
    return NULL;
  return &iter->second;
}

void SandboxFileSystemBackendDelegate::RegisterQuotaUpdateObserver(
    FileSystemType type) {
  AddFileUpdateObserver(type, quota_observer_.get(), file_task_runner_.get());
}

void SandboxFileSystemBackendDelegate::InvalidateUsageCache(
    const GURL& origin,
    FileSystemType type) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      obfuscated_file_util(), origin, type, &error);
  if (error != base::PLATFORM_FILE_OK)
    return;
  usage_cache()->IncrementDirty(usage_file_path);
}

void SandboxFileSystemBackendDelegate::StickyInvalidateUsageCache(
    const GURL& origin,
    FileSystemType type) {
  sticky_dirty_origins_.insert(std::make_pair(origin, type));
  quota_observer()->SetUsageCacheEnabled(origin, type, false);
  InvalidateUsageCache(origin, type);
}

FileSystemFileUtil* SandboxFileSystemBackendDelegate::sync_file_util() {
  return static_cast<AsyncFileUtilAdapter*>(file_util())->sync_file_util();
}

bool SandboxFileSystemBackendDelegate::IsAccessValid(
    const FileSystemURL& url) const {
  if (!IsAllowedScheme(url.origin()))
    return false;

  if (url.path().ReferencesParent())
    return false;

  // Return earlier if the path is '/', because VirtualPath::BaseName()
  // returns '/' for '/' and we fail the "basename != '/'" check below.
  // (We exclude '.' because it's disallowed by spec.)
  if (VirtualPath::IsRootPath(url.path()) &&
      url.path() != base::FilePath(base::FilePath::kCurrentDirectory))
    return true;

  // Restricted names specified in
  // http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
  base::FilePath filename = VirtualPath::BaseName(url.path());
  // See if the name is allowed to create.
  for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
    if (filename.value() == kRestrictedNames[i])
      return false;
  }
  for (size_t i = 0; i < arraysize(kRestrictedChars); ++i) {
    if (filename.value().find(kRestrictedChars[i]) !=
        base::FilePath::StringType::npos)
      return false;
  }

  return true;
}

bool SandboxFileSystemBackendDelegate::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  if (url.SchemeIsHTTPOrHTTPS())
    return true;
  if (url.SchemeIsFileSystem())
    return url.inner_url() && IsAllowedScheme(*url.inner_url());

  for (size_t i = 0;
       i < file_system_options_.additional_allowed_schemes().size();
       ++i) {
    if (url.SchemeIs(
            file_system_options_.additional_allowed_schemes()[i].c_str()))
      return true;
  }
  return false;
}

base::FilePath
SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
    const GURL& origin_url,
    FileSystemType type) {
  base::PlatformFileError error;
  base::FilePath path = GetUsageCachePathForOriginAndType(
      obfuscated_file_util(), origin_url, type, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return path;
}

// static
base::FilePath
SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
    ObfuscatedFileUtil* sandbox_file_util,
    const GURL& origin_url,
    FileSystemType type,
    base::PlatformFileError* error_out) {
  DCHECK(error_out);
  *error_out = base::PLATFORM_FILE_OK;
  base::FilePath base_path = sandbox_file_util->GetDirectoryForOriginAndType(
      origin_url, GetTypeString(type), false /* create */, error_out);
  if (*error_out != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return base_path.Append(FileSystemUsageCache::kUsageFileName);
}

int64 SandboxFileSystemBackendDelegate::RecalculateUsage(
    FileSystemContext* context,
    const GURL& origin,
    FileSystemType type) {
  FileSystemOperationContext operation_context(context);
  FileSystemURL url = context->CreateCrackedFileSystemURL(
      origin, type, base::FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      obfuscated_file_util()->CreateFileEnumerator(
          &operation_context, url, true));

  base::FilePath file_path_each;
  int64 usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    usage += enumerator->Size();
    usage += ObfuscatedFileUtil::ComputeFilePathCost(file_path_each);
  }

  return usage;
}

void SandboxFileSystemBackendDelegate::CollectOpenFileSystemMetrics(
    base::PlatformFileError error_code) {
  base::Time now = base::Time::Now();
  bool throttled = now < next_release_time_for_open_filesystem_stat_;
  if (!throttled) {
    next_release_time_for_open_filesystem_stat_ =
        now + base::TimeDelta::FromHours(kMinimumStatsCollectionIntervalHours);
  }

#define REPORT(report_value)                                            \
  UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemDetailLabel,                 \
                            (report_value),                             \
                            kFileSystemErrorMax);                       \
  if (!throttled) {                                                     \
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemDetailNonThrottledLabel,   \
                              (report_value),                           \
                              kFileSystemErrorMax);                     \
  }

  switch (error_code) {
    case base::PLATFORM_FILE_OK:
      REPORT(kOK);
      break;
    case base::PLATFORM_FILE_ERROR_INVALID_URL:
      REPORT(kInvalidSchemeError);
      break;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      REPORT(kNotFound);
      break;
    case base::PLATFORM_FILE_ERROR_FAILED:
    default:
      REPORT(kUnknownError);
      break;
  }
#undef REPORT
}

ObfuscatedFileUtil* SandboxFileSystemBackendDelegate::obfuscated_file_util() {
  return static_cast<ObfuscatedFileUtil*>(sync_file_util());
}

// Declared in obfuscated_file_util.h.
// static
ObfuscatedFileUtil* ObfuscatedFileUtil::CreateForTesting(
    quota::SpecialStoragePolicy* special_storage_policy,
    const base::FilePath& file_system_directory,
    base::SequencedTaskRunner* file_task_runner) {
  return new ObfuscatedFileUtil(special_storage_policy,
                                file_system_directory,
                                file_task_runner,
                                base::Bind(&GetTypeStringForURL),
                                GetKnownTypeStrings(),
                                NULL);
}

}  // namespace fileapi
