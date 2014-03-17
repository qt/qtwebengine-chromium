// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
#define UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {
class LocatedEvent;
class OSExchangeData;
}

namespace aura {
class Window;
namespace client {

// An interface implemented by an object that controls a drag and drop session.
class AURA_EXPORT DragDropClient {
 public:
  virtual ~DragDropClient() {}

  // Initiates a drag and drop session. Returns the drag operation that was
  // applied at the end of the drag drop session. |root_location| is in the
  // root Window's coordinate system.
  virtual int StartDragAndDrop(const ui::OSExchangeData& data,
                               aura::Window* root_window,
                               aura::Window* source_window,
                               const gfx::Point& root_location,
                               int operation,
                               ui::DragDropTypes::DragEventSource source) = 0;

  // Called when mouse is dragged during a drag and drop.
  virtual void DragUpdate(aura::Window* target,
                          const ui::LocatedEvent& event) = 0;

  // Called when mouse is released during a drag and drop.
  virtual void Drop(aura::Window* target,
                    const ui::LocatedEvent& event) = 0;

  // Called when a drag and drop session is cancelled.
  virtual void DragCancel() = 0;

  // Returns true if a drag and drop session is in progress.
  virtual bool IsDragDropInProgress() = 0;
};

AURA_EXPORT void SetDragDropClient(Window* root_window,
                                   DragDropClient* client);
AURA_EXPORT DragDropClient* GetDragDropClient(Window* root_window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
