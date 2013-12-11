// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "gpu/config/gpu_blacklist.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_switching_list.h"

namespace content {

class CONTENT_EXPORT GpuDataManagerImplPrivate {
 public:
  static GpuDataManagerImplPrivate* Create(GpuDataManagerImpl* owner);

  void InitializeForTesting(
      const std::string& gpu_blacklist_json,
      const gpu::GPUInfo& gpu_info);
  bool IsFeatureBlacklisted(int feature) const;
  bool IsDriverBugWorkaroundActive(int feature) const;
  gpu::GPUInfo GetGPUInfo() const;
  void GetGpuProcessHandles(
      const GpuDataManager::GetGpuProcessHandlesCallback& callback) const;
  bool GpuAccessAllowed(std::string* reason) const;
  void RequestCompleteGpuInfoIfNeeded();
  bool IsCompleteGpuInfoAvailable() const;
  void RequestVideoMemoryUsageStatsUpdate() const;
  bool ShouldUseSwiftShader() const;
  void RegisterSwiftShaderPath(const base::FilePath& path);
  void AddObserver(GpuDataManagerObserver* observer);
  void RemoveObserver(GpuDataManagerObserver* observer);
  void UnblockDomainFrom3DAPIs(const GURL& url);
  void DisableGpuWatchdog();
  void SetGLStrings(const std::string& gl_vendor,
                    const std::string& gl_renderer,
                    const std::string& gl_version);
  void GetGLStrings(std::string* gl_vendor,
                    std::string* gl_renderer,
                    std::string* gl_version);
  void DisableHardwareAcceleration();

  void Initialize();

  void UpdateGpuInfo(const gpu::GPUInfo& gpu_info);

  void UpdateVideoMemoryUsageStats(
      const GPUVideoMemoryUsageStats& video_memory_usage_stats);

  void AppendRendererCommandLine(CommandLine* command_line) const;

  void AppendGpuCommandLine(CommandLine* command_line) const;

  void AppendPluginCommandLine(CommandLine* command_line) const;

  void UpdateRendererWebPrefs(WebPreferences* prefs) const;

  gpu::GpuSwitchingOption GetGpuSwitchingOption() const;

  std::string GetBlacklistVersion() const;
  std::string GetDriverBugListVersion() const;

  void GetBlacklistReasons(base::ListValue* reasons) const;

  void GetDriverBugWorkarounds(base::ListValue* workarounds) const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  void ProcessCrashed(base::TerminationStatus exit_code);

  base::ListValue* GetLogMessages() const;

  void HandleGpuSwitch();

#if defined(OS_WIN)
  // Is the GPU process using the accelerated surface to present, instead of
  // presenting by itself.
  bool IsUsingAcceleratedSurface() const;
#endif

  bool CanUseGpuBrowserCompositor() const;

  void BlockDomainFrom3DAPIs(
      const GURL& url, GpuDataManagerImpl::DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& url,
                        int render_process_id,
                        int render_view_id,
                        ThreeDAPIType requester);

  void DisableDomainBlockingFor3DAPIsForTesting();

  void Notify3DAPIBlocked(const GURL& url,
                          int render_process_id,
                          int render_view_id,
                          ThreeDAPIType requester);

  size_t GetBlacklistedFeatureCount() const;

  void SetDisplayCount(unsigned int display_count);
  unsigned int GetDisplayCount() const;

  void OnGpuProcessInitFailure();

  virtual ~GpuDataManagerImplPrivate();

 private:
  friend class GpuDataManagerImplPrivateTest;

  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuSideBlacklisting);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuSideExceptions);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           DisableHardwareAcceleration);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           SwiftShaderRendering);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           SwiftShaderRendering2);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuInfoUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           NoGpuInfoUpdateWithSwiftShader);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GPUVideoMemoryUsageStatsUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           BlockAllDomainsFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockGuiltyDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockDomainOfUnknownGuiltFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockOtherDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockThisDomainFrom3DAPIs);
#if defined(OS_LINUX)
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           SetGLStrings);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           SetGLStringsNoEffects);
#endif
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuDriverBugListSingle);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuDriverBugListMultiple);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           BlacklistAllFeatures);

  struct DomainBlockEntry {
    GpuDataManagerImpl::DomainGuilt last_guilt;
  };

  typedef std::map<std::string, DomainBlockEntry> DomainBlockMap;

  typedef ObserverListThreadSafe<GpuDataManagerObserver>
      GpuDataManagerObserverList;

  explicit GpuDataManagerImplPrivate(GpuDataManagerImpl* owner);

  void InitializeImpl(const std::string& gpu_blacklist_json,
                      const std::string& gpu_switching_list_json,
                      const std::string& gpu_driver_bug_list_json,
                      const gpu::GPUInfo& gpu_info);

  void UpdateBlacklistedFeatures(const std::set<int>& features);

  // This should only be called once at initialization time, when preliminary
  // gpu info is collected.
  void UpdatePreliminaryBlacklistedFeatures();

  // Update the GPU switching status.
  // This should only be called once at initialization time.
  void UpdateGpuSwitchingManager(const gpu::GPUInfo& gpu_info);

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  // Try to switch to SwiftShader rendering, if possible and necessary.
  void EnableSwiftShaderIfNecessary();

  // Helper to extract the domain from a given URL.
  std::string GetDomainFromURL(const GURL& url) const;

  // Implementation functions for blocking of 3D graphics APIs, used
  // for unit testing.
  void BlockDomainFrom3DAPIsAtTime(const GURL& url,
                                   GpuDataManagerImpl::DomainGuilt guilt,
                                   base::Time at_time);
  GpuDataManagerImpl::DomainBlockStatus Are3DAPIsBlockedAtTime(
      const GURL& url, base::Time at_time) const;
  int64 GetBlockAllDomainsDurationInMs() const;

  bool complete_gpu_info_already_requested_;

  std::set<int> blacklisted_features_;
  std::set<int> preliminary_blacklisted_features_;

  gpu::GpuSwitchingOption gpu_switching_;

  std::set<int> gpu_driver_bugs_;

  gpu::GPUInfo gpu_info_;

  scoped_ptr<gpu::GpuBlacklist> gpu_blacklist_;
  scoped_ptr<gpu::GpuSwitchingList> gpu_switching_list_;
  scoped_ptr<gpu::GpuDriverBugList> gpu_driver_bug_list_;

  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  base::ListValue log_messages_;

  bool use_swiftshader_;

  base::FilePath swiftshader_path_;

  // Current card force-blacklisted due to GPU crashes, or disabled through
  // the --disable-gpu commandline switch.
  bool card_blacklisted_;

  // We disable histogram stuff in testing, especially in unit tests because
  // they cause random failures.
  bool update_histograms_;

  // Number of currently open windows, to be used in gpu memory allocation.
  int window_count_;

  DomainBlockMap blocked_domains_;
  mutable std::list<base::Time> timestamps_of_gpu_resets_;
  bool domain_blocking_enabled_;

  GpuDataManagerImpl* owner_;

  unsigned int display_count_;

  bool gpu_process_accessible_;

  bool use_software_compositor_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImplPrivate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_

