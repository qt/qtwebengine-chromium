// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_

#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT CompositorClient {
 public:
  // Tells the client that it should schedule a composite.
  virtual void ScheduleComposite() = 0;

  // The compositor has completed swapping a frame.
  virtual void OnSwapBuffersCompleted() {}

  // The compositor will eventually swap a frame.
  virtual void OnSwapBuffersPosted() {}

  // Tells the client that GL resources were lost and need to be reinitialized.
  virtual void DidLoseResources() {}

  // Tells the client that UI resources were lost and need to be reinitialized.
  virtual void DidLoseUIResources() {}

  // Mark the UI Resources as being invalid for use.
  virtual void UIResourcesAreInvalid() {}

 protected:
  CompositorClient() {}
  virtual ~CompositorClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_
