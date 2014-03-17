// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_DESKTOP_CLEANER_
#define ASH_FIRST_RUN_DESKTOP_CLEANER_

#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

namespace test {
class FirstRunHelperTest;
}

namespace internal {

class ContainerHider;
class NotificationBlocker;

// Class used to "clean" ash desktop, i.e. hide all windows and notifications.
class ASH_EXPORT DesktopCleaner {
 public:
  DesktopCleaner();
  ~DesktopCleaner();

 private:
  // Returns the list of containers that DesctopCleaner hides.
  static std::vector<int> GetContainersToHideForTest();

  std::vector<linked_ptr<ContainerHider> > container_hiders_;
  scoped_ptr<NotificationBlocker> notification_blocker_;

  friend class ash::test::FirstRunHelperTest;
  DISALLOW_COPY_AND_ASSIGN(DesktopCleaner);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_FIRST_RUN_DESKTOP_CLEANER_
