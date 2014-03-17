// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_memory_uma_stats.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/gpu_switches.h"

using gpu::ManagedMemoryStats;
using gpu::MemoryAllocation;

namespace content {
namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

const uint64 kBytesAllocatedUnmanagedStep = 16 * 1024 * 1024;

void TrackValueChanged(uint64 old_size, uint64 new_size, uint64* total_size) {
  DCHECK(new_size > old_size || *total_size >= (old_size - new_size));
  *total_size += (new_size - old_size);
}

template<typename T>
T RoundUp(T n, T mul) {
  return ((n + mul - 1) / mul) * mul;
}

template<typename T>
T RoundDown(T n, T mul) {
  return (n / mul) * mul;
}

}

GpuMemoryManager::GpuMemoryManager(
    GpuChannelManager* channel_manager,
    uint64 max_surfaces_with_frontbuffer_soft_limit)
    : channel_manager_(channel_manager),
      manage_immediate_scheduled_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      priority_cutoff_(MemoryAllocation::CUTOFF_ALLOW_EVERYTHING),
      bytes_available_gpu_memory_(0),
      bytes_available_gpu_memory_overridden_(false),
      bytes_minimum_per_client_(0),
      bytes_default_per_client_(0),
      bytes_allocated_managed_current_(0),
      bytes_allocated_unmanaged_current_(0),
      bytes_allocated_historical_max_(0),
      bytes_allocated_unmanaged_high_(0),
      bytes_allocated_unmanaged_low_(0),
      bytes_unmanaged_limit_step_(kBytesAllocatedUnmanagedStep),
      disable_schedule_manage_(false)
{
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // Use a more conservative memory allocation policy on Linux and Mac because
  // the platform is unstable when under memory pressure.
  // http://crbug.com/145600 (Linux)
  // http://crbug.com/141377 (Mac)
#if defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  priority_cutoff_ = MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
#endif

#if defined(OS_ANDROID)
  bytes_default_per_client_ = 8 * 1024 * 1024;
  bytes_minimum_per_client_ = 8 * 1024 * 1024;
#elif defined(OS_CHROMEOS)
  bytes_default_per_client_ = 64 * 1024 * 1024;
  bytes_minimum_per_client_ = 4 * 1024 * 1024;
#elif defined(OS_MACOSX)
  bytes_default_per_client_ = 128 * 1024 * 1024;
  bytes_minimum_per_client_ = 128 * 1024 * 1024;
#else
  bytes_default_per_client_ = 64 * 1024 * 1024;
  bytes_minimum_per_client_ = 64 * 1024 * 1024;
#endif

  if (command_line->HasSwitch(switches::kForceGpuMemAvailableMb)) {
    base::StringToUint64(
        command_line->GetSwitchValueASCII(switches::kForceGpuMemAvailableMb),
        &bytes_available_gpu_memory_);
    bytes_available_gpu_memory_ *= 1024 * 1024;
    bytes_available_gpu_memory_overridden_ = true;
  } else
    bytes_available_gpu_memory_ = GetDefaultAvailableGpuMemory();
}

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
  DCHECK(clients_visible_mru_.empty());
  DCHECK(clients_nonvisible_mru_.empty());
  DCHECK(clients_nonsurface_.empty());
  DCHECK(!bytes_allocated_managed_current_);
  DCHECK(!bytes_allocated_unmanaged_current_);
}

uint64 GpuMemoryManager::GetAvailableGpuMemory() const {
  // Allow unmanaged allocations to over-subscribe by at most (high_ - low_)
  // before restricting managed (compositor) memory based on unmanaged usage.
  if (bytes_allocated_unmanaged_low_ > bytes_available_gpu_memory_)
    return 0;
  return bytes_available_gpu_memory_ - bytes_allocated_unmanaged_low_;
}

uint64 GpuMemoryManager::GetDefaultAvailableGpuMemory() const {
#if defined(OS_ANDROID)
  return 16 * 1024 * 1024;
#elif defined(OS_CHROMEOS)
  return 1024 * 1024 * 1024;
#else
  return 256 * 1024 * 1024;
#endif
}

uint64 GpuMemoryManager::GetMaximumTotalGpuMemory() const {
#if defined(OS_ANDROID)
  return 256 * 1024 * 1024;
#else
  return 1024 * 1024 * 1024;
#endif
}

uint64 GpuMemoryManager::GetMaximumClientAllocation() const {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return bytes_available_gpu_memory_;
#else
  // This is to avoid allowing a single page on to use a full 256MB of memory
  // (the current total limit). Long-scroll pages will hit this limit,
  // resulting in instability on some platforms (e.g, issue 141377).
  return bytes_available_gpu_memory_ / 2;
#endif
}

uint64 GpuMemoryManager::CalcAvailableFromGpuTotal(uint64 total_gpu_memory) {
#if defined(OS_ANDROID)
  // We don't need to reduce the total on Android, since
  // the total is an estimate to begin with.
  return total_gpu_memory;
#else
  // Allow Chrome to use 75% of total GPU memory, or all-but-64MB of GPU
  // memory, whichever is less.
  return std::min(3 * total_gpu_memory / 4, total_gpu_memory - 64*1024*1024);
#endif
}

void GpuMemoryManager::UpdateAvailableGpuMemory() {
  // If the amount of video memory to use was specified at the command
  // line, never change it.
  if (bytes_available_gpu_memory_overridden_)
    return;

  // On non-Android, we use an operating system query when possible.
  // We do not have a reliable concept of multiple GPUs existing in
  // a system, so just be safe and go with the minimum encountered.
  uint64 bytes_min = 0;

  // Only use the clients that are visible, because otherwise the set of clients
  // we are querying could become extremely large.
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
      it != clients_visible_mru_.end();
      ++it) {
    const GpuMemoryManagerClientState* client_state = *it;
    if (!client_state->has_surface_)
      continue;
    if (!client_state->visible_)
      continue;

    uint64 bytes = 0;
    if (client_state->client_->GetTotalGpuMemory(&bytes)) {
      if (!bytes_min || bytes < bytes_min)
        bytes_min = bytes;
    }
  }

  if (!bytes_min)
    return;

  bytes_available_gpu_memory_ = CalcAvailableFromGpuTotal(bytes_min);

  // Never go below the default allocation
  bytes_available_gpu_memory_ = std::max(bytes_available_gpu_memory_,
                                         GetDefaultAvailableGpuMemory());

  // Never go above the maximum.
  bytes_available_gpu_memory_ = std::min(bytes_available_gpu_memory_,
                                         GetMaximumTotalGpuMemory());
}

void GpuMemoryManager::UpdateUnmanagedMemoryLimits() {
  // Set the limit to be [current_, current_ + step_ / 4), with the endpoints
  // of the intervals rounded down and up to the nearest step_, to avoid
  // thrashing the interval.
  bytes_allocated_unmanaged_high_ = RoundUp(
      bytes_allocated_unmanaged_current_ + bytes_unmanaged_limit_step_ / 4,
      bytes_unmanaged_limit_step_);
  bytes_allocated_unmanaged_low_ = RoundDown(
      bytes_allocated_unmanaged_current_,
      bytes_unmanaged_limit_step_);
}

void GpuMemoryManager::ScheduleManage(
    ScheduleManageTime schedule_manage_time) {
  if (disable_schedule_manage_)
    return;
  if (manage_immediate_scheduled_)
    return;
  if (schedule_manage_time == kScheduleManageNow) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&GpuMemoryManager::Manage, AsWeakPtr()));
    manage_immediate_scheduled_ = true;
    if (!delayed_manage_callback_.IsCancelled())
      delayed_manage_callback_.Cancel();
  } else {
    if (!delayed_manage_callback_.IsCancelled())
      return;
    delayed_manage_callback_.Reset(base::Bind(&GpuMemoryManager::Manage,
                                              AsWeakPtr()));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        delayed_manage_callback_.callback(),
        base::TimeDelta::FromMilliseconds(kDelayedScheduleManageTimeoutMs));
  }
}

void GpuMemoryManager::TrackMemoryAllocatedChange(
    GpuMemoryTrackingGroup* tracking_group,
    uint64 old_size,
    uint64 new_size,
    gpu::gles2::MemoryTracker::Pool tracking_pool) {
  TrackValueChanged(old_size, new_size, &tracking_group->size_);
  switch (tracking_pool) {
    case gpu::gles2::MemoryTracker::kManaged:
      TrackValueChanged(old_size, new_size, &bytes_allocated_managed_current_);
      break;
    case gpu::gles2::MemoryTracker::kUnmanaged:
      TrackValueChanged(old_size,
                        new_size,
                        &bytes_allocated_unmanaged_current_);
      break;
    default:
      NOTREACHED();
      break;
  }
  if (new_size != old_size) {
    TRACE_COUNTER1("gpu",
                   "GpuMemoryUsage",
                   GetCurrentUsage());
  }

  // If we've gone past our current limit on unmanaged memory, schedule a
  // re-manage to take int account the unmanaged memory.
  if (bytes_allocated_unmanaged_current_ >= bytes_allocated_unmanaged_high_)
    ScheduleManage(kScheduleManageNow);
  if (bytes_allocated_unmanaged_current_ < bytes_allocated_unmanaged_low_)
    ScheduleManage(kScheduleManageLater);

  if (GetCurrentUsage() > bytes_allocated_historical_max_) {
      bytes_allocated_historical_max_ = GetCurrentUsage();
      // If we're blowing into new memory usage territory, spam the browser
      // process with the most up-to-date information about our memory usage.
      SendUmaStatsToBrowser();
  }
}

bool GpuMemoryManager::EnsureGPUMemoryAvailable(uint64 /* size_needed */) {
  // TODO: Check if there is enough space. Lose contexts until there is.
  return true;
}

GpuMemoryManagerClientState* GpuMemoryManager::CreateClientState(
    GpuMemoryManagerClient* client,
    bool has_surface,
    bool visible) {
  TrackingGroupMap::iterator tracking_group_it =
      tracking_groups_.find(client->GetMemoryTracker());
  DCHECK(tracking_group_it != tracking_groups_.end());
  GpuMemoryTrackingGroup* tracking_group = tracking_group_it->second;

  GpuMemoryManagerClientState* client_state = new GpuMemoryManagerClientState(
      this, client, tracking_group, has_surface, visible);
  AddClientToList(client_state);
  ScheduleManage(kScheduleManageNow);
  return client_state;
}

void GpuMemoryManager::OnDestroyClientState(
    GpuMemoryManagerClientState* client_state) {
  RemoveClientFromList(client_state);
  ScheduleManage(kScheduleManageLater);
}

void GpuMemoryManager::SetClientStateVisible(
    GpuMemoryManagerClientState* client_state, bool visible) {
  DCHECK(client_state->has_surface_);
  if (client_state->visible_ == visible)
    return;

  RemoveClientFromList(client_state);
  client_state->visible_ = visible;
  AddClientToList(client_state);
  ScheduleManage(visible ? kScheduleManageNow : kScheduleManageLater);
}

void GpuMemoryManager::SetClientStateManagedMemoryStats(
    GpuMemoryManagerClientState* client_state,
    const ManagedMemoryStats& stats)
{
  client_state->managed_memory_stats_ = stats;

  // If this is the first time that stats have been received for this
  // client, use them immediately.
  if (!client_state->managed_memory_stats_received_) {
    client_state->managed_memory_stats_received_ = true;
    ScheduleManage(kScheduleManageNow);
    return;
  }

  // If these statistics sit outside of the range that we used in our
  // computation of memory allocations then recompute the allocations.
  if (client_state->managed_memory_stats_.bytes_nice_to_have >
      client_state->bytes_nicetohave_limit_high_) {
    ScheduleManage(kScheduleManageNow);
  } else if (client_state->managed_memory_stats_.bytes_nice_to_have <
             client_state->bytes_nicetohave_limit_low_) {
    ScheduleManage(kScheduleManageLater);
  }
}

uint64 GpuMemoryManager::GetClientMemoryUsage(
    const GpuMemoryManagerClient* client) const{
  TrackingGroupMap::const_iterator tracking_group_it =
      tracking_groups_.find(client->GetMemoryTracker());
  DCHECK(tracking_group_it != tracking_groups_.end());
  return tracking_group_it->second->GetSize();
}

GpuMemoryTrackingGroup* GpuMemoryManager::CreateTrackingGroup(
    base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker) {
  GpuMemoryTrackingGroup* tracking_group = new GpuMemoryTrackingGroup(
      pid, memory_tracker, this);
  DCHECK(!tracking_groups_.count(tracking_group->GetMemoryTracker()));
  tracking_groups_.insert(std::make_pair(tracking_group->GetMemoryTracker(),
                                         tracking_group));
  return tracking_group;
}

void GpuMemoryManager::OnDestroyTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  DCHECK(tracking_groups_.count(tracking_group->GetMemoryTracker()));
  tracking_groups_.erase(tracking_group->GetMemoryTracker());
}

void GpuMemoryManager::GetVideoMemoryUsageStats(
    GPUVideoMemoryUsageStats* video_memory_usage_stats) const {
  // For each context group, assign its memory usage to its PID
  video_memory_usage_stats->process_map.clear();
  for (TrackingGroupMap::const_iterator i =
       tracking_groups_.begin(); i != tracking_groups_.end(); ++i) {
    const GpuMemoryTrackingGroup* tracking_group = i->second;
    video_memory_usage_stats->process_map[
        tracking_group->GetPid()].video_memory += tracking_group->GetSize();
  }

  // Assign the total across all processes in the GPU process
  video_memory_usage_stats->process_map[
      base::GetCurrentProcId()].video_memory = GetCurrentUsage();
  video_memory_usage_stats->process_map[
      base::GetCurrentProcId()].has_duplicates = true;

  video_memory_usage_stats->bytes_allocated = GetCurrentUsage();
  video_memory_usage_stats->bytes_allocated_historical_max =
      bytes_allocated_historical_max_;
}

void GpuMemoryManager::Manage() {
  manage_immediate_scheduled_ = false;
  delayed_manage_callback_.Cancel();

  // Update the amount of GPU memory available on the system.
  UpdateAvailableGpuMemory();

  // Update the limit on unmanaged memory.
  UpdateUnmanagedMemoryLimits();

  // Determine which clients are "hibernated" (which determines the
  // distribution of frontbuffers and memory among clients that don't have
  // surfaces).
  SetClientsHibernatedState();

  // Assign memory allocations to clients that have surfaces.
  AssignSurfacesAllocations();

  // Assign memory allocations to clients that don't have surfaces.
  AssignNonSurfacesAllocations();

  SendUmaStatsToBrowser();
}

// static
uint64 GpuMemoryManager::ComputeCap(
    std::vector<uint64> bytes, uint64 bytes_sum_limit)
{
  size_t bytes_size = bytes.size();
  uint64 bytes_sum = 0;

  if (bytes_size == 0)
    return std::numeric_limits<uint64>::max();

  // Sort and add up all entries
  std::sort(bytes.begin(), bytes.end());
  for (size_t i = 0; i < bytes_size; ++i)
    bytes_sum += bytes[i];

  // As we go through the below loop, let bytes_partial_sum be the
  // sum of bytes[0] + ... + bytes[bytes_size - i - 1]
  uint64 bytes_partial_sum = bytes_sum;

  // Try using each entry as a cap, and see where we get cut off.
  for (size_t i = 0; i < bytes_size; ++i) {
    // Try limiting cap to bytes[bytes_size - i - 1]
    uint64 test_cap = bytes[bytes_size - i - 1];
    uint64 bytes_sum_with_test_cap = i * test_cap + bytes_partial_sum;

    // If that fits, raise test_cap to give an even distribution to the
    // last i entries.
    if (bytes_sum_with_test_cap <= bytes_sum_limit) {
      if (i == 0)
        return std::numeric_limits<uint64>::max();
      else
        return test_cap + (bytes_sum_limit - bytes_sum_with_test_cap) / i;
    } else {
      bytes_partial_sum -= test_cap;
    }
  }

  // If we got here, then we can't fully accommodate any of the clients,
  // so distribute bytes_sum_limit evenly.
  return bytes_sum_limit / bytes_size;
}

uint64 GpuMemoryManager::ComputeClientAllocationWhenVisible(
    GpuMemoryManagerClientState* client_state,
    uint64 bytes_above_required_cap,
    uint64 bytes_above_minimum_cap,
    uint64 bytes_overall_cap) {
  ManagedMemoryStats* stats = &client_state->managed_memory_stats_;

  if (!client_state->managed_memory_stats_received_)
    return GetDefaultClientAllocation();

  uint64 bytes_required = 9 * stats->bytes_required / 8;
  bytes_required = std::min(bytes_required, GetMaximumClientAllocation());
  bytes_required = std::max(bytes_required, GetMinimumClientAllocation());

  uint64 bytes_nicetohave = 4 * stats->bytes_nice_to_have / 3;
  bytes_nicetohave = std::min(bytes_nicetohave, GetMaximumClientAllocation());
  bytes_nicetohave = std::max(bytes_nicetohave, GetMinimumClientAllocation());
  bytes_nicetohave = std::max(bytes_nicetohave, bytes_required);

  uint64 allocation = GetMinimumClientAllocation();
  allocation += std::min(bytes_required - GetMinimumClientAllocation(),
                         bytes_above_minimum_cap);
  allocation += std::min(bytes_nicetohave - bytes_required,
                         bytes_above_required_cap);
  allocation = std::min(allocation,
                        bytes_overall_cap);
  return allocation;
}

void GpuMemoryManager::ComputeVisibleSurfacesAllocations() {
  uint64 bytes_available_total = GetAvailableGpuMemory();
  uint64 bytes_above_required_cap = std::numeric_limits<uint64>::max();
  uint64 bytes_above_minimum_cap = std::numeric_limits<uint64>::max();
  uint64 bytes_overall_cap_visible = GetMaximumClientAllocation();

  // Compute memory usage at three levels
  // - painting everything that is nicetohave for visible clients
  // - painting only what that is visible
  // - giving every client the minimum allocation
  uint64 bytes_nicetohave_visible = 0;
  uint64 bytes_required_visible = 0;
  uint64 bytes_minimum_visible = 0;
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->bytes_allocation_ideal_nicetohave_ =
        ComputeClientAllocationWhenVisible(
            client_state,
            bytes_above_required_cap,
            bytes_above_minimum_cap,
            bytes_overall_cap_visible);
    client_state->bytes_allocation_ideal_required_ =
        ComputeClientAllocationWhenVisible(
            client_state,
            0,
            bytes_above_minimum_cap,
            bytes_overall_cap_visible);
    client_state->bytes_allocation_ideal_minimum_ =
        ComputeClientAllocationWhenVisible(
            client_state,
            0,
            0,
            bytes_overall_cap_visible);

    bytes_nicetohave_visible +=
        client_state->bytes_allocation_ideal_nicetohave_;
    bytes_required_visible +=
        client_state->bytes_allocation_ideal_required_;
    bytes_minimum_visible +=
        client_state->bytes_allocation_ideal_minimum_;
  }

  // Determine which of those three points we can satisfy, and limit
  // bytes_above_required_cap and bytes_above_minimum_cap to not go
  // over the limit.
  if (bytes_minimum_visible > bytes_available_total) {
    bytes_above_required_cap = 0;
    bytes_above_minimum_cap = 0;
  } else if (bytes_required_visible > bytes_available_total) {
    std::vector<uint64> bytes_to_fit;
    for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
         it != clients_visible_mru_.end();
         ++it) {
      GpuMemoryManagerClientState* client_state = *it;
      bytes_to_fit.push_back(client_state->bytes_allocation_ideal_required_ -
                             client_state->bytes_allocation_ideal_minimum_);
    }
    bytes_above_required_cap = 0;
    bytes_above_minimum_cap = ComputeCap(
        bytes_to_fit, bytes_available_total - bytes_minimum_visible);
  } else if (bytes_nicetohave_visible > bytes_available_total) {
    std::vector<uint64> bytes_to_fit;
    for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
         it != clients_visible_mru_.end();
         ++it) {
      GpuMemoryManagerClientState* client_state = *it;
      bytes_to_fit.push_back(client_state->bytes_allocation_ideal_nicetohave_ -
                             client_state->bytes_allocation_ideal_required_);
    }
    bytes_above_required_cap = ComputeCap(
        bytes_to_fit, bytes_available_total - bytes_required_visible);
    bytes_above_minimum_cap = std::numeric_limits<uint64>::max();
  }

  // Given those computed limits, set the actual memory allocations for the
  // visible clients, tracking the largest allocation and the total allocation
  // for future use.
  uint64 bytes_allocated_visible = 0;
  uint64 bytes_allocated_max_client_allocation = 0;
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->bytes_allocation_when_visible_ =
        ComputeClientAllocationWhenVisible(
            client_state,
            bytes_above_required_cap,
            bytes_above_minimum_cap,
            bytes_overall_cap_visible);
    bytes_allocated_visible += client_state->bytes_allocation_when_visible_;
    bytes_allocated_max_client_allocation = std::max(
        bytes_allocated_max_client_allocation,
        client_state->bytes_allocation_when_visible_);
  }

  // Set the limit for nonvisible clients for when they become visible.
  // Use the same formula, with a lowered overall cap in case any of the
  // currently-nonvisible clients are much more resource-intensive than any
  // of the existing clients.
  uint64 bytes_overall_cap_nonvisible = bytes_allocated_max_client_allocation;
  if (bytes_available_total > bytes_allocated_visible) {
    bytes_overall_cap_nonvisible +=
        bytes_available_total - bytes_allocated_visible;
  }
  bytes_overall_cap_nonvisible = std::min(bytes_overall_cap_nonvisible,
                                          GetMaximumClientAllocation());
  for (ClientStateList::const_iterator it = clients_nonvisible_mru_.begin();
       it != clients_nonvisible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->bytes_allocation_when_visible_ =
        ComputeClientAllocationWhenVisible(
            client_state,
            bytes_above_required_cap,
            bytes_above_minimum_cap,
            bytes_overall_cap_nonvisible);
  }
}

void GpuMemoryManager::DistributeRemainingMemoryToVisibleSurfaces() {
  uint64 bytes_available_total = GetAvailableGpuMemory();
  uint64 bytes_allocated_total = 0;

  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    bytes_allocated_total += client_state->bytes_allocation_when_visible_;
  }

  if (bytes_allocated_total >= bytes_available_total)
    return;

  std::vector<uint64> bytes_extra_requests;
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    CHECK(GetMaximumClientAllocation() >=
          client_state->bytes_allocation_when_visible_);
    uint64 bytes_extra = GetMaximumClientAllocation() -
                         client_state->bytes_allocation_when_visible_;
    bytes_extra_requests.push_back(bytes_extra);
  }
  uint64 bytes_extra_cap = ComputeCap(
      bytes_extra_requests, bytes_available_total - bytes_allocated_total);
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    uint64 bytes_extra = GetMaximumClientAllocation() -
                         client_state->bytes_allocation_when_visible_;
    client_state->bytes_allocation_when_visible_ += std::min(
        bytes_extra, bytes_extra_cap);
  }
}

void GpuMemoryManager::AssignSurfacesAllocations() {
  // Compute allocation when for all clients.
  ComputeVisibleSurfacesAllocations();

  // Distribute the remaining memory to visible clients.
  DistributeRemainingMemoryToVisibleSurfaces();

  // Send that allocation to the clients.
  ClientStateList clients = clients_visible_mru_;
  clients.insert(clients.end(),
                 clients_nonvisible_mru_.begin(),
                 clients_nonvisible_mru_.end());
  for (ClientStateList::const_iterator it = clients.begin();
       it != clients.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;

    // Re-assign memory limits to this client when its "nice to have" bucket
    // grows or shrinks by 1/4.
    client_state->bytes_nicetohave_limit_high_ =
        5 * client_state->managed_memory_stats_.bytes_nice_to_have / 4;
    client_state->bytes_nicetohave_limit_low_ =
        3 * client_state->managed_memory_stats_.bytes_nice_to_have / 4;

    // Populate and send the allocation to the client
    MemoryAllocation allocation;

    allocation.bytes_limit_when_visible =
        client_state->bytes_allocation_when_visible_;
    allocation.priority_cutoff_when_visible = priority_cutoff_;

    client_state->client_->SetMemoryAllocation(allocation);
    client_state->client_->SuggestHaveFrontBuffer(!client_state->hibernated_);
  }
}

void GpuMemoryManager::AssignNonSurfacesAllocations() {
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    MemoryAllocation allocation;

    if (!client_state->hibernated_) {
      allocation.bytes_limit_when_visible =
          GetMinimumClientAllocation();
      allocation.priority_cutoff_when_visible =
          MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
    }

    client_state->client_->SetMemoryAllocation(allocation);
  }
}

void GpuMemoryManager::SetClientsHibernatedState() const {
  // Re-set all tracking groups as being hibernated.
  for (TrackingGroupMap::const_iterator it = tracking_groups_.begin();
       it != tracking_groups_.end();
       ++it) {
    GpuMemoryTrackingGroup* tracking_group = it->second;
    tracking_group->hibernated_ = true;
  }
  // All clients with surfaces that are visible are non-hibernated.
  uint64 non_hibernated_clients = 0;
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->hibernated_ = false;
    client_state->tracking_group_->hibernated_ = false;
    non_hibernated_clients++;
  }
  // Then an additional few clients with surfaces are non-hibernated too, up to
  // a fixed limit.
  for (ClientStateList::const_iterator it = clients_nonvisible_mru_.begin();
       it != clients_nonvisible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    if (non_hibernated_clients < max_surfaces_with_frontbuffer_soft_limit_) {
      client_state->hibernated_ = false;
      client_state->tracking_group_->hibernated_ = false;
      non_hibernated_clients++;
    } else {
      client_state->hibernated_ = true;
    }
  }
  // Clients that don't have surfaces are non-hibernated if they are
  // in a GL share group with a non-hibernated surface.
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->hibernated_ = client_state->tracking_group_->hibernated_;
  }
}

void GpuMemoryManager::SendUmaStatsToBrowser() {
  if (!channel_manager_)
    return;
  GPUMemoryUmaStats params;
  params.bytes_allocated_current = GetCurrentUsage();
  params.bytes_allocated_max = bytes_allocated_historical_max_;
  params.bytes_limit = bytes_available_gpu_memory_;
  params.client_count = clients_visible_mru_.size() +
                        clients_nonvisible_mru_.size() +
                        clients_nonsurface_.size();
  params.context_group_count = tracking_groups_.size();
  channel_manager_->Send(new GpuHostMsg_GpuMemoryUmaStats(params));
}

GpuMemoryManager::ClientStateList* GpuMemoryManager::GetClientList(
    GpuMemoryManagerClientState* client_state) {
  if (client_state->has_surface_) {
    if (client_state->visible_)
      return &clients_visible_mru_;
    else
      return &clients_nonvisible_mru_;
  }
  return &clients_nonsurface_;
}

void GpuMemoryManager::AddClientToList(
    GpuMemoryManagerClientState* client_state) {
  DCHECK(!client_state->list_iterator_valid_);
  ClientStateList* client_list = GetClientList(client_state);
  client_state->list_iterator_ = client_list->insert(
      client_list->begin(), client_state);
  client_state->list_iterator_valid_ = true;
}

void GpuMemoryManager::RemoveClientFromList(
    GpuMemoryManagerClientState* client_state) {
  DCHECK(client_state->list_iterator_valid_);
  ClientStateList* client_list = GetClientList(client_state);
  client_list->erase(client_state->list_iterator_);
  client_state->list_iterator_valid_ = false;
}

}  // namespace content
