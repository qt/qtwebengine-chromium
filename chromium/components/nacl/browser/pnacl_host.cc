// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/pnacl_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/pnacl_translation_cache.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {
static const base::FilePath::CharType kTranslationCacheDirectoryName[] =
    FILE_PATH_LITERAL("PnaclTranslationCache");
// Delay to wait for initialization of the cache backend
static const int kTranslationCacheInitializationDelayMs = 20;
}

namespace pnacl {

PnaclHost::PnaclHost()
    : pending_backend_operations_(0),
      cache_state_(CacheUninitialized),
      weak_factory_(this) {}

PnaclHost::~PnaclHost() {
  // When PnaclHost is destroyed, it's too late to post anything to the cache
  // thread (it will hang shutdown). So just leak the cache backend.
  pnacl::PnaclTranslationCache* cache = disk_cache_.release();
  (void)cache;
}

PnaclHost* PnaclHost::GetInstance() { return Singleton<PnaclHost>::get(); }

PnaclHost::PendingTranslation::PendingTranslation()
    : process_handle(base::kNullProcessHandle),
      render_view_id(0),
      nexe_fd(base::kInvalidPlatformFileValue),
      got_nexe_fd(false),
      got_cache_reply(false),
      got_cache_hit(false),
      is_incognito(false),
      callback(NexeFdCallback()),
      cache_info(nacl::PnaclCacheInfo()) {}
PnaclHost::PendingTranslation::~PendingTranslation() {}

bool PnaclHost::TranslationMayBeCached(
    const PendingTranslationMap::iterator& entry) {
  return !entry->second.is_incognito &&
         !entry->second.cache_info.has_no_store_header;
}

/////////////////////////////////////// Initialization

static base::FilePath GetCachePath() {
  NaClBrowserDelegate* browser_delegate = nacl::NaClBrowser::GetDelegate();
  // Determine where the translation cache resides in the file system.  It
  // exists in Chrome's cache directory and is not tied to any specific
  // profile. If we fail, return an empty path.
  // Start by finding the user data directory.
  base::FilePath user_data_dir;
  if (!browser_delegate ||
      !browser_delegate->GetUserDirectory(&user_data_dir)) {
    return base::FilePath();
  }
  // The cache directory may or may not be the user data directory.
  base::FilePath cache_file_path;
  browser_delegate->GetCacheDirectory(&cache_file_path);

  // Append the base file name to the cache directory.
  return cache_file_path.Append(kTranslationCacheDirectoryName);
}

void PnaclHost::OnCacheInitialized(int net_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If the cache was cleared before the load completed, ignore.
  if (cache_state_ == CacheReady)
    return;
  if (net_error != net::OK) {
    // This will cause the cache to attempt to re-init on the next call to
    // GetNexeFd.
    cache_state_ = CacheUninitialized;
  } else {
    cache_state_ = CacheReady;
  }
}

void PnaclHost::Init() {
  // Extra check that we're on the real IO thread since this version of
  // Init isn't used in unit tests.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath cache_path(GetCachePath());
  if (cache_path.empty() || cache_state_ != CacheUninitialized)
    return;
  disk_cache_.reset(new pnacl::PnaclTranslationCache());
  cache_state_ = CacheInitializing;
  int rv = disk_cache_->InitOnDisk(
      cache_path,
      base::Bind(&PnaclHost::OnCacheInitialized, weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    OnCacheInitialized(rv);
}

// Initialize using the in-memory backend, and manually set the temporary file
// directory instead of using the system directory.
void PnaclHost::InitForTest(base::FilePath temp_dir) {
  DCHECK(thread_checker_.CalledOnValidThread());
  disk_cache_.reset(new pnacl::PnaclTranslationCache());
  cache_state_ = CacheInitializing;
  temp_dir_ = temp_dir;
  int rv = disk_cache_->InitInMemory(
      base::Bind(&PnaclHost::OnCacheInitialized, weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    OnCacheInitialized(rv);
}

///////////////////////////////////////// Temp files

// Create a temporary file on the blocking pool
// static
void PnaclHost::DoCreateTemporaryFile(base::FilePath temp_dir,
                                      TempFileCallback cb) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath file_path;
  base::PlatformFile file_handle(base::kInvalidPlatformFileValue);
  bool rv = temp_dir.empty()
                ? base::CreateTemporaryFile(&file_path)
                : base::CreateTemporaryFileInDir(temp_dir, &file_path);
  if (!rv) {
    PLOG(ERROR) << "Temp file creation failed.";
  } else {
    base::PlatformFileError error;
    file_handle = base::CreatePlatformFile(
        file_path,
        base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_READ |
            base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_TEMPORARY |
            base::PLATFORM_FILE_DELETE_ON_CLOSE,
        NULL,
        &error);

    if (error != base::PLATFORM_FILE_OK) {
      PLOG(ERROR) << "Temp file open failed: " << error;
      file_handle = base::kInvalidPlatformFileValue;
    }
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(cb, file_handle));
}

void PnaclHost::CreateTemporaryFile(TempFileCallback cb) {
  if (!BrowserThread::PostBlockingPoolSequencedTask(
           "PnaclHostCreateTempFile",
           FROM_HERE,
           base::Bind(&PnaclHost::DoCreateTemporaryFile, temp_dir_, cb))) {
    DCHECK(thread_checker_.CalledOnValidThread());
    cb.Run(base::kInvalidPlatformFileValue);
  }
}

///////////////////////////////////////// GetNexeFd implementation
////////////////////// Common steps

void PnaclHost::GetNexeFd(int render_process_id,
                          int render_view_id,
                          int pp_instance,
                          bool is_incognito,
                          const nacl::PnaclCacheInfo& cache_info,
                          const NexeFdCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ == CacheUninitialized) {
    Init();
  }
  if (cache_state_ != CacheReady) {
    // If the backend hasn't yet initialized, try the request again later.
    BrowserThread::PostDelayedTask(BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(&PnaclHost::GetNexeFd,
                                              weak_factory_.GetWeakPtr(),
                                              render_process_id,
                                              render_view_id,
                                              pp_instance,
                                              is_incognito,
                                              cache_info,
                                              cb),
                                   base::TimeDelta::FromMilliseconds(
                                       kTranslationCacheInitializationDelayMs));
    return;
  }

  TranslationID id(render_process_id, pp_instance);
  PendingTranslationMap::iterator entry = pending_translations_.find(id);
  if (entry != pending_translations_.end()) {
    // Existing translation must have been abandonded. Clean it up.
    LOG(ERROR) << "GetNexeFd for already-pending translation";
    pending_translations_.erase(entry);
  }

  std::string cache_key(disk_cache_->GetKey(cache_info));
  if (cache_key.empty()) {
    LOG(ERROR) << "GetNexeFd: Invalid cache info";
    cb.Run(base::kInvalidPlatformFileValue, false);
    return;
  }

  PendingTranslation pt;
  pt.render_view_id = render_view_id;
  pt.callback = cb;
  pt.cache_info = cache_info;
  pt.cache_key = cache_key;
  pt.is_incognito = is_incognito;
  pending_translations_[id] = pt;
  SendCacheQueryAndTempFileRequest(cache_key, id);
}

// Dispatch the cache read request and the temp file creation request
// simultaneously; currently we need a temp file regardless of whether the
// request hits.
void PnaclHost::SendCacheQueryAndTempFileRequest(const std::string& cache_key,
                                                 const TranslationID& id) {
  pending_backend_operations_++;
  disk_cache_->GetNexe(
      cache_key,
      base::Bind(
          &PnaclHost::OnCacheQueryReturn, weak_factory_.GetWeakPtr(), id));

  CreateTemporaryFile(
      base::Bind(&PnaclHost::OnTempFileReturn, weak_factory_.GetWeakPtr(), id));
}

// Callback from the translation cache query. |id| is bound from
// SendCacheQueryAndTempFileRequest, |net_error| is a net::Error code (which for
// our purposes means a hit if it's net::OK (i.e. 0). |buffer| is allocated
// by PnaclTranslationCache and now belongs to PnaclHost.
// (Bound callbacks must re-lookup the TranslationID because the translation
// could be cancelled before they get called).
void PnaclHost::OnCacheQueryReturn(
    const TranslationID& id,
    int net_error,
    scoped_refptr<net::DrainableIOBuffer> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_backend_operations_--;
  PendingTranslationMap::iterator entry(pending_translations_.find(id));
  if (entry == pending_translations_.end()) {
    LOG(ERROR) << "OnCacheQueryReturn: id not found";
    DeInitIfSafe();
    return;
  }
  PendingTranslation* pt = &entry->second;
  pt->got_cache_reply = true;
  pt->got_cache_hit = (net_error == net::OK);
  if (pt->got_cache_hit)
    pt->nexe_read_buffer = buffer;
  CheckCacheQueryReady(entry);
}

// Callback from temp file creation. |id| is bound from
// SendCacheQueryAndTempFileRequest, and fd is the created file descriptor.
// If there was an error, fd is kInvalidPlatformFileValue.
// (Bound callbacks must re-lookup the TranslationID because the translation
// could be cancelled before they get called).
void PnaclHost::OnTempFileReturn(const TranslationID& id,
                                 base::PlatformFile fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PendingTranslationMap::iterator entry(pending_translations_.find(id));
  if (entry == pending_translations_.end()) {
    // The renderer may have signaled an error or closed while the temp
    // file was being created.
    LOG(ERROR) << "OnTempFileReturn: id not found";
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE, base::Bind(base::IgnoreResult(base::ClosePlatformFile), fd));
    return;
  }
  if (fd == base::kInvalidPlatformFileValue) {
    // This translation will fail, but we need to retry any translation
    // waiting for its result.
    LOG(ERROR) << "OnTempFileReturn: temp file creation failed";
    std::string key(entry->second.cache_key);
    entry->second.callback.Run(fd, false);
    bool may_be_cached = TranslationMayBeCached(entry);
    pending_translations_.erase(entry);
    // No translations will be waiting for entries that will not be stored.
    if (may_be_cached)
      RequeryMatchingTranslations(key);
    return;
  }
  PendingTranslation* pt = &entry->second;
  pt->got_nexe_fd = true;
  pt->nexe_fd = fd;
  CheckCacheQueryReady(entry);
}

// Check whether both the cache query and the temp file have returned, and check
// whether we actually got a hit or not.
void PnaclHost::CheckCacheQueryReady(
    const PendingTranslationMap::iterator& entry) {
  PendingTranslation* pt = &entry->second;
  if (!(pt->got_cache_reply && pt->got_nexe_fd))
    return;
  if (!pt->got_cache_hit) {
    // Check if there is already a pending translation for this file. If there
    // is, we will wait for it to come back, to avoid redundant translations.
    for (PendingTranslationMap::iterator it = pending_translations_.begin();
         it != pending_translations_.end();
         ++it) {
      // Another translation matches if it's a request for the same file,
      if (it->second.cache_key == entry->second.cache_key &&
          // and it's not this translation,
          it->first != entry->first &&
          // and it can be stored in the cache,
          TranslationMayBeCached(it) &&
          // and it's already gotten past this check and returned the miss.
          it->second.got_cache_reply &&
          it->second.got_nexe_fd) {
        return;
      }
    }
    ReturnMiss(entry);
    return;
  }

  if (!base::PostTaskAndReplyWithResult(
           BrowserThread::GetBlockingPool(),
           FROM_HERE,
           base::Bind(
               &PnaclHost::CopyBufferToFile, pt->nexe_fd, pt->nexe_read_buffer),
           base::Bind(&PnaclHost::OnBufferCopiedToTempFile,
                      weak_factory_.GetWeakPtr(),
                      entry->first))) {
    pt->callback.Run(base::kInvalidPlatformFileValue, false);
  }
}

//////////////////// GetNexeFd miss path
// Return the temp fd to the renderer, reporting a miss.
void PnaclHost::ReturnMiss(const PendingTranslationMap::iterator& entry) {
  // Return the fd
  PendingTranslation* pt = &entry->second;
  NexeFdCallback cb(pt->callback);
  if (pt->nexe_fd == base::kInvalidPlatformFileValue) {
    // Bad FD is unrecoverable, so clear out the entry
    pending_translations_.erase(entry);
  }
  cb.Run(pt->nexe_fd, false);
}

// On error, just return a null refptr.
// static
scoped_refptr<net::DrainableIOBuffer> PnaclHost::CopyFileToBuffer(
    base::PlatformFile fd) {
  base::PlatformFileInfo info;
  scoped_refptr<net::DrainableIOBuffer> buffer;
  bool error = false;
  if (!base::GetPlatformFileInfo(fd, &info) ||
      info.size >= std::numeric_limits<int>::max()) {
    PLOG(ERROR) << "GetPlatformFileInfo failed";
    error = true;
  } else {
    buffer = new net::DrainableIOBuffer(
        new net::IOBuffer(static_cast<int>(info.size)), info.size);
    if (base::ReadPlatformFile(fd, 0, buffer->data(), buffer->size()) !=
        info.size) {
      PLOG(ERROR) << "CopyFileToBuffer file read failed";
      error = true;
    }
  }
  if (error) {
    buffer = NULL;
  }
  base::ClosePlatformFile(fd);
  return buffer;
}

// Called by the renderer in the miss path to report a finished translation
void PnaclHost::TranslationFinished(int render_process_id,
                                    int pp_instance,
                                    bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ != CacheReady)
    return;
  TranslationID id(render_process_id, pp_instance);
  PendingTranslationMap::iterator entry(pending_translations_.find(id));
  if (entry == pending_translations_.end()) {
    LOG(ERROR) << "TranslationFinished: TranslationID " << render_process_id
               << "," << pp_instance << " not found.";
    return;
  }
  bool store_nexe = true;
  // If this is a premature response (i.e. we haven't returned a temp file
  // yet) or if it's an unsuccessful translation, or if we are incognito,
  // don't store in the cache.
  // TODO(dschuff): use a separate in-memory cache for incognito
  // translations.
  if (!entry->second.got_nexe_fd || !entry->second.got_cache_reply ||
      !success || !TranslationMayBeCached(entry)) {
    store_nexe = false;
  } else if (!base::PostTaskAndReplyWithResult(
                  BrowserThread::GetBlockingPool(),
                  FROM_HERE,
                  base::Bind(&PnaclHost::CopyFileToBuffer,
                             entry->second.nexe_fd),
                  base::Bind(&PnaclHost::StoreTranslatedNexe,
                             weak_factory_.GetWeakPtr(),
                             id))) {
    store_nexe = false;
  }

  if (!store_nexe) {
    // If store_nexe is true, the fd will be closed by CopyFileToBuffer.
    if (entry->second.got_nexe_fd) {
      BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(base::ClosePlatformFile),
                     entry->second.nexe_fd));
    }
    pending_translations_.erase(entry);
  }
}

// Store the translated nexe in the translation cache. Called back with the
// TranslationID from the host and the result of CopyFileToBuffer.
// (Bound callbacks must re-lookup the TranslationID because the translation
// could be cancelled before they get called).
void PnaclHost::StoreTranslatedNexe(
    TranslationID id,
    scoped_refptr<net::DrainableIOBuffer> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ != CacheReady)
    return;
  PendingTranslationMap::iterator it(pending_translations_.find(id));
  if (it == pending_translations_.end()) {
    LOG(ERROR) << "StoreTranslatedNexe: TranslationID " << id.first << ","
               << id.second << " not found.";
    return;
  }

  if (buffer.get() == NULL) {
    LOG(ERROR) << "Error reading translated nexe";
    return;
  }
  pending_backend_operations_++;
  disk_cache_->StoreNexe(it->second.cache_key,
                         buffer,
                         base::Bind(&PnaclHost::OnTranslatedNexeStored,
                                    weak_factory_.GetWeakPtr(),
                                    it->first));
}

// After we know the nexe has been stored, we can clean up, and unblock any
// outstanding requests for the same file.
// (Bound callbacks must re-lookup the TranslationID because the translation
// could be cancelled before they get called).
void PnaclHost::OnTranslatedNexeStored(const TranslationID& id, int net_error) {
  PendingTranslationMap::iterator entry(pending_translations_.find(id));
  pending_backend_operations_--;
  if (entry == pending_translations_.end()) {
    // If the renderer closed while we were storing the nexe, we land here.
    // Make sure we try to de-init.
    DeInitIfSafe();
    return;
  }
  std::string key(entry->second.cache_key);
  pending_translations_.erase(entry);
  RequeryMatchingTranslations(key);
}

// Check if any pending translations match |key|. If so, re-issue the cache
// query. In the overlapped miss case, we expect a hit this time, but a miss
// is also possible in case of an error.
void PnaclHost::RequeryMatchingTranslations(const std::string& key) {
  // Check for outstanding misses to this same file
  for (PendingTranslationMap::iterator it = pending_translations_.begin();
       it != pending_translations_.end();
       ++it) {
    if (it->second.cache_key == key) {
      // Re-send the cache read request. This time we expect a hit, but if
      // something goes wrong, it will just handle it like a miss.
      it->second.got_cache_reply = false;
      pending_backend_operations_++;
      disk_cache_->GetNexe(key,
                           base::Bind(&PnaclHost::OnCacheQueryReturn,
                                      weak_factory_.GetWeakPtr(),
                                      it->first));
    }
  }
}

//////////////////// GetNexeFd hit path

// static
int PnaclHost::CopyBufferToFile(base::PlatformFile fd,
                                scoped_refptr<net::DrainableIOBuffer> buffer) {
  int rv = base::WritePlatformFile(fd, 0, buffer->data(), buffer->size());
  if (rv == -1)
    PLOG(ERROR) << "CopyBufferToFile write error";
  return rv;
}

void PnaclHost::OnBufferCopiedToTempFile(const TranslationID& id,
                                         int file_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PendingTranslationMap::iterator entry(pending_translations_.find(id));
  if (entry == pending_translations_.end()) {
    return;
  }
  if (file_error == -1) {
    // Write error on the temp file. Request a new file and start over.
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(base::ClosePlatformFile),
                   entry->second.nexe_fd));
    entry->second.got_nexe_fd = false;
    CreateTemporaryFile(base::Bind(&PnaclHost::OnTempFileReturn,
                                   weak_factory_.GetWeakPtr(),
                                   entry->first));
    return;
  }
  base::PlatformFile fd = entry->second.nexe_fd;
  entry->second.callback.Run(fd, true);
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(base::IgnoreResult(base::ClosePlatformFile), fd));
  pending_translations_.erase(entry);
}

///////////////////

void PnaclHost::RendererClosing(int render_process_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ != CacheReady)
    return;
  for (PendingTranslationMap::iterator it = pending_translations_.begin();
       it != pending_translations_.end();) {
    PendingTranslationMap::iterator to_erase(it++);
    if (to_erase->first.first == render_process_id) {
      // Clean up the open files.
      BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(base::ClosePlatformFile),
                     to_erase->second.nexe_fd));
      std::string key(to_erase->second.cache_key);
      bool may_be_cached = TranslationMayBeCached(to_erase);
      pending_translations_.erase(to_erase);
      // No translations will be waiting for entries that will not be stored.
      if (may_be_cached)
        RequeryMatchingTranslations(key);
    }
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PnaclHost::DeInitIfSafe, weak_factory_.GetWeakPtr()));
}

////////////////// Cache data removal
void PnaclHost::ClearTranslationCacheEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ == CacheUninitialized) {
    Init();
  }
  if (cache_state_ == CacheInitializing) {
    // If the backend hasn't yet initialized, try the request again later.
    BrowserThread::PostDelayedTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&PnaclHost::ClearTranslationCacheEntriesBetween,
                   weak_factory_.GetWeakPtr(),
                   initial_time,
                   end_time,
                   callback),
        base::TimeDelta::FromMilliseconds(
            kTranslationCacheInitializationDelayMs));
    return;
  }
  pending_backend_operations_++;
  int rv = disk_cache_->DoomEntriesBetween(
      initial_time,
      end_time,
      base::Bind(
          &PnaclHost::OnEntriesDoomed, weak_factory_.GetWeakPtr(), callback));
  if (rv != net::ERR_IO_PENDING)
    OnEntriesDoomed(callback, rv);
}

void PnaclHost::OnEntriesDoomed(const base::Closure& callback, int net_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, callback);
  pending_backend_operations_--;
  // When clearing the cache, the UI is blocked on all the cache-clearing
  // operations, and freeing the backend actually blocks the IO thread. So
  // instead of calling DeInitIfSafe directly, post it for later.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PnaclHost::DeInitIfSafe, weak_factory_.GetWeakPtr()));
}

// Destroying the cache backend causes it to post tasks to the cache thread to
// flush to disk. Because PnaclHost is a singleton, it does not get destroyed
// until all the browser threads have gone away and it's too late to post
// anything (attempting to do so hangs shutdown).  So we make sure to destroy it
// when we no longer have any outstanding operations that need it. These include
// pending translations, cache clear requests, and requests to read or write
// translated nexes.  We check when renderers close, when cache clear requests
// finish, and when backend operations complete.

// It is not safe to delete the backend while it is initializing, nor if it has
// outstanding entry open requests; it is in theory safe to delete it with
// outstanding read/write requests, but because that distinction is hidden
// inside PnaclTranslationCache, we do not delete the backend if there are any
// backend requests in flight.  As a last resort in the destructor, we just leak
// the backend to avoid hanging shutdown.
void PnaclHost::DeInitIfSafe() {
  DCHECK(pending_backend_operations_ >= 0);
  if (pending_translations_.empty() && pending_backend_operations_ <= 0) {
    cache_state_ = CacheUninitialized;
    disk_cache_.reset();
  }
}

}  // namespace pnacl
