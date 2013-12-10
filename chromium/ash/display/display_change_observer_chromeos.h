// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H
#define ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "chromeos/display/output_configurator.h"

namespace ash {
namespace internal {

struct Resolution;

// An object that observes changes in display configuration and
// update DisplayManagers.
class DisplayChangeObserver
    : public chromeos::OutputConfigurator::StateController,
      public chromeos::OutputConfigurator::Observer,
      public ShellObserver {
 public:
  // Returns true if the size info in the output_info isn't valid
  // and should be ignored. This is exposed for testing.
  // |mm_width| and |mm_height| are given in millimeters.
  ASH_EXPORT static bool ShouldIgnoreSize(unsigned long mm_width,
                                          unsigned long mm_height);

  // Returns the resolution list.
  ASH_EXPORT static std::vector<Resolution> GetResolutionList(
      const chromeos::OutputConfigurator::OutputSnapshot& output);

  DisplayChangeObserver();
  virtual ~DisplayChangeObserver();

  // chromeos::OutputConfigurator::StateController overrides:
  virtual chromeos::OutputState GetStateForDisplayIds(
      const std::vector<int64>& outputs) const OVERRIDE;
  virtual bool GetResolutionForDisplayId(int64 display_id,
                                         int* width,
                                         int* height) const OVERRIDE;

  // Overriden from chromeos::OutputConfigurator::Observer:
  virtual void OnDisplayModeChanged(
      const std::vector<chromeos::OutputConfigurator::OutputSnapshot>& outputs)
      OVERRIDE;

  // Overriden from ShellObserver:
  virtual void OnAppTerminating() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayChangeObserver);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_AURA_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H
