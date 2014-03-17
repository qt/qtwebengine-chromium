// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_PNACL_HOST_H_
#define COMPONENTS_NACL_BROWSER_PNACL_HOST_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/nacl/browser/nacl_file_host.h"
#include "components/nacl/common/pnacl_types.h"
#include "ipc/ipc_platform_file.h"

namespace net {
class DrainableIOBuffer;
}

namespace pnacl {

class PnaclHostTest;
class PnaclTranslationCache;

// Shared state (translation cache) and common utilities (temp file creation)
// for all PNaCl translations. Unless otherwise specified, all methods should be
// called on the IO thread.
class PnaclHost {
 public:
  typedef base::Callback<void(base::PlatformFile)> TempFileCallback;
  typedef base::Callback<void(base::PlatformFile, bool is_hit)> NexeFdCallback;

  static PnaclHost* GetInstance();

  PnaclHost();
  ~PnaclHost();

  // Initialize cache backend. GetNexeFd will also initialize the backend if
  // necessary, but calling Init ahead of time will minimize the latency.
  void Init();

  // Creates a temporary file that will be deleted when the last handle
  // is closed, or earlier. Returns a PlatformFile handle.
  void CreateTemporaryFile(TempFileCallback cb);

  // Create a temporary file, which will be deleted by the time the last
  // handle is closed (or earlier on POSIX systems), to use for the nexe
  // with the cache information given in |cache_info|. The specific instance
  // is identified by the combination of |render_process_id| and |pp_instance|.
  // Returns by calling |cb| with a PlatformFile handle.
  // If the nexe is already present
  // in the cache, |is_hit| is set to true and the contents of the nexe
  // have been copied into the temporary file. Otherwise |is_hit| is set to
  // false and the temporary file will be writeable.
  // Currently the implementation is a stub, which always sets is_hit to false
  // and calls the implementation of CreateTemporaryFile.
  // If the cache request was a miss, the caller is expected to call
  // TranslationFinished after it finishes translation to allow the nexe to be
  // stored in the cache.
  // The returned temp fd may be closed at any time by PnaclHost, so it should
  // be duplicated (e.g. with IPC::GetFileHandleForProcess) before the callback
  // returns.
  // If |is_incognito| is true, the nexe will not be stored
  // in the cache, but the renderer is still expected to call
  // TranslationFinished.
  void GetNexeFd(int render_process_id,
                 int render_view_id,
                 int pp_instance,
                 bool is_incognito,
                 const nacl::PnaclCacheInfo& cache_info,
                 const NexeFdCallback& cb);

  // Called after the translation of a pexe instance identified by
  // |render_process_id| and |pp_instance| finishes. If |success| is true,
  // store the nexe translated for the instance in the cache.
  void TranslationFinished(int render_process_id,
                           int pp_instance,
                           bool success);

  // Called when the renderer identified by |render_process_id| is closing.
  // Clean up any outstanding translations for that renderer. If there are no
  // more pending translations, the backend is freed, allowing it to flush.
  void RendererClosing(int render_process_id);

  // Doom all entries between |initial_time| and |end_time|. Like disk_cache_,
  // PnaclHost supports supports unbounded deletes in either direction by using
  // null Time values for either argument. |callback| will be called on the UI
  // thread when finished.
  void ClearTranslationCacheEntriesBetween(base::Time initial_time,
                                           base::Time end_time,
                                           const base::Closure& callback);

  // Return the number of tracked translations or FD requests currently pending.
  size_t pending_translations() { return pending_translations_.size(); }

 private:
  // PnaclHost is a singleton because there is only one translation cache, and
  // so that the BrowsingDataRemover can clear it even if no translation has
  // ever been started.
  friend struct DefaultSingletonTraits<PnaclHost>;
  friend class pnacl::PnaclHostTest;
  enum CacheState {
    CacheUninitialized,
    CacheInitializing,
    CacheReady
  };
  class PendingTranslation {
   public:
    PendingTranslation();
    ~PendingTranslation();
    base::ProcessHandle process_handle;
    int render_view_id;
    base::PlatformFile nexe_fd;
    bool got_nexe_fd;
    bool got_cache_reply;
    bool got_cache_hit;
    bool is_incognito;
    scoped_refptr<net::DrainableIOBuffer> nexe_read_buffer;
    NexeFdCallback callback;
    std::string cache_key;
    nacl::PnaclCacheInfo cache_info;
  };

  typedef std::pair<int, int> TranslationID;
  typedef std::map<TranslationID, PendingTranslation> PendingTranslationMap;
  static bool TranslationMayBeCached(
      const PendingTranslationMap::iterator& entry);

  void InitForTest(base::FilePath temp_dir);
  void OnCacheInitialized(int net_error);

  static void DoCreateTemporaryFile(base::FilePath temp_dir_,
                                    TempFileCallback cb);

  // GetNexeFd common steps
  void SendCacheQueryAndTempFileRequest(const std::string& key,
                                        const TranslationID& id);
  void OnCacheQueryReturn(const TranslationID& id,
                          int net_error,
                          scoped_refptr<net::DrainableIOBuffer> buffer);
  void OnTempFileReturn(const TranslationID& id, base::PlatformFile fd);
  void CheckCacheQueryReady(const PendingTranslationMap::iterator& entry);

  // GetNexeFd miss path
  void ReturnMiss(const PendingTranslationMap::iterator& entry);
  static scoped_refptr<net::DrainableIOBuffer> CopyFileToBuffer(
      base::PlatformFile fd);
  void StoreTranslatedNexe(TranslationID id,
                           scoped_refptr<net::DrainableIOBuffer>);
  void OnTranslatedNexeStored(const TranslationID& id, int net_error);
  void RequeryMatchingTranslations(const std::string& key);

  // GetNexeFd hit path
  static int CopyBufferToFile(base::PlatformFile fd,
                              scoped_refptr<net::DrainableIOBuffer> buffer);
  void OnBufferCopiedToTempFile(const TranslationID& id, int file_error);

  void OnEntriesDoomed(const base::Closure& callback, int net_error);

  void DeInitIfSafe();

  // Operations which are pending with the cache backend, which we should
  // wait for before destroying it (see comment on DeInitIfSafe).
  int pending_backend_operations_;
  CacheState cache_state_;
  base::FilePath temp_dir_;
  scoped_ptr<pnacl::PnaclTranslationCache> disk_cache_;
  PendingTranslationMap pending_translations_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<PnaclHost> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PnaclHost);
};

}  // namespace pnacl

#endif  // COMPONENTS_NACL_BROWSER_PNACL_HOST_H_
