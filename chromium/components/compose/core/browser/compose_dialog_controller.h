// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_DIALOG_CONTROLLER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_DIALOG_CONTROLLER_H_

namespace compose {

// The interface for embedder-independent dialog controllers.
class ComposeDialogController {
 public:
  virtual ~ComposeDialogController() = default;
  // Shows the compose dialog.
  virtual void ShowUI() = 0;
  // Closes the compose dialog.
  virtual void Close() = 0;
  // Returns true when the dialog is showing and false otherwise.
  virtual bool IsDialogShowing() = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_DIALOG_CONTROLLER_H_
