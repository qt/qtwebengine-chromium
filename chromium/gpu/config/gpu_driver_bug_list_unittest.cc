// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/config/gpu_control_list_jsons.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace gpu {

class GpuDriverBugListTest : public testing::Test {
 public:
  GpuDriverBugListTest() { }

  virtual ~GpuDriverBugListTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

 protected:
  virtual void SetUp() {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.driver_vendor = "NVIDIA";
    gpu_info_.driver_version = "1.6.18";
    gpu_info_.driver_date = "7-14-2009";
    gpu_info_.machine_model = "MacBookPro 7.1";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    gpu_info_.performance_stats.graphics = 5.0;
    gpu_info_.performance_stats.gaming = 5.0;
    gpu_info_.performance_stats.overall = 5.0;
  }

  virtual void TearDown() {
  }

 private:
  GPUInfo gpu_info_;
};

TEST_F(GpuDriverBugListTest, CurrentDriverBugListValidation) {
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  std::string json;
  EXPECT_TRUE(list->LoadList(kGpuDriverBugListJson, GpuControlList::kAllOs));
}

TEST_F(GpuDriverBugListTest, CurrentListForARM) {
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  EXPECT_TRUE(list->LoadList(kGpuDriverBugListJson, GpuControlList::kAllOs));

  GPUInfo gpu_info;
  gpu_info.gl_vendor = "ARM";
  gpu_info.gl_renderer = "MALi_T604";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

TEST_F(GpuDriverBugListTest, CurrentListForImagination) {
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  EXPECT_TRUE(list->LoadList(kGpuDriverBugListJson, GpuControlList::kAllOs));

  GPUInfo gpu_info;
  gpu_info.gl_vendor = "Imagination Technologies";
  gpu_info.gl_renderer = "PowerVR SGX 540";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

TEST_F(GpuDriverBugListTest, GpuSwitching) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu driver bug list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "macosx"
            },
            "features": [
              "force_discrete_gpu"
            ]
          },
          {
            "id": 2,
            "os": {
              "type": "win"
            },
            "features": [
              "force_integrated_gpu"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuDriverBugList> driver_bug_list(GpuDriverBugList::Create());
  EXPECT_TRUE(driver_bug_list->LoadList(json, GpuControlList::kAllOs));
  std::set<int> switching = driver_bug_list->MakeDecision(
      GpuControlList::kOsMacosx, "10.8", gpu_info());
  EXPECT_EQ(1u, switching.size());
  EXPECT_EQ(1u, switching.count(FORCE_DISCRETE_GPU));
  std::vector<uint32> entries;
  driver_bug_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(1u, entries[0]);

  driver_bug_list.reset(GpuDriverBugList::Create());
  EXPECT_TRUE(driver_bug_list->LoadList(json, GpuControlList::kAllOs));
  switching = driver_bug_list->MakeDecision(
      GpuControlList::kOsWin, "6.1", gpu_info());
  EXPECT_EQ(1u, switching.size());
  EXPECT_EQ(1u, switching.count(FORCE_INTEGRATED_GPU));
  driver_bug_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(2u, entries[0]);
}

}  // namespace gpu

