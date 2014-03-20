// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SaveResponseCallback(bool* called,
                          int64* store_result,
                          ServiceWorkerRegistrationStatus status,
                          int64 result) {
  *called = true;
  *store_result = result;
}

ServiceWorkerContextCore::RegistrationCallback MakeRegisteredCallback(
    bool* called,
    int64* store_result) {
  return base::Bind(&SaveResponseCallback, called, store_result);
}

void CallCompletedCallback(bool* called, ServiceWorkerRegistrationStatus) {
  *called = true;
}

ServiceWorkerContextCore::UnregistrationCallback MakeUnregisteredCallback(
    bool* called) {
  return base::Bind(&CallCompletedCallback, called);
}

}  // namespace

class ServiceWorkerContextTest : public testing::Test {
 public:
  ServiceWorkerContextTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
  }

  virtual void TearDown() OVERRIDE { context_.reset(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
};

void RegistrationCallback(
    scoped_refptr<ServiceWorkerRegistration>* registration,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  *registration = result;
}

// Make sure basic registration is working.
TEST_F(ServiceWorkerContextTest, Register) {
  int64 registration_id = -1L;
  bool called = false;
  context_->RegisterServiceWorker(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(-1L, registration_id);
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerContextTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called = false;
  int64 registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      MakeRegisteredCallback(&called, &registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  called = false;
  context_->UnregisterServiceWorker(pattern, MakeUnregisteredCallback(&called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerContextTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called = false;
  int64 old_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  called = false;
  int64 new_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(old_registration_id, new_registration_id);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerContextTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called = false;
  int64 old_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      script_url,
      MakeRegisteredCallback(&called, &old_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  called = false;
  int64 new_registration_id = -1L;
  context_->RegisterServiceWorker(
      pattern,
      script_url,
      MakeRegisteredCallback(&called, &new_registration_id));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration_id, new_registration_id);
}

}  // namespace content
