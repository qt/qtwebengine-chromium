// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_WIN_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "ui/base/dragdrop/drop_target_win.h"

namespace content {
class InterstitialDropTarget;
class RenderViewHost;
class WebContents;
class WebDragDestDelegate;

// A helper object that provides drop capabilities to a WebContentsImpl. The
// DropTarget handles drags that enter the region of the WebContents by
// passing on the events to the renderer.
class CONTENT_EXPORT WebDragDest : public ui::DropTargetWin {
 public:
  // Create a new WebDragDest associating it with the given HWND and
  // WebContents.
  WebDragDest(HWND source_hwnd, WebContents* contents);
  virtual ~WebDragDest();

  DropData* current_drop_data() const { return drop_data_.get(); }

  void set_drag_cursor(blink::WebDragOperation op) {
    drag_cursor_ = op;
  }

  WebDragDestDelegate* delegate() const { return delegate_; }
  void set_delegate(WebDragDestDelegate* d) { delegate_ = d; }

 protected:
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);

  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  virtual void OnDragLeave(IDataObject* data_object);

  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // Our associated WebContents.
  WebContents* web_contents_;

  // We keep track of the render view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.  WARNING:
  // this pointer should never be dereferenced.  We only use it for comparing
  // pointers.
  RenderViewHost* current_rvh_;

  // Used to determine what cursor we should display when dragging over web
  // content area.  This can be updated async during a drag operation.
  blink::WebDragOperation drag_cursor_;

  // A special drop target handler for when we try to d&d while an interstitial
  // page is showing.
  scoped_ptr<InterstitialDropTarget> interstitial_drop_target_;

  // A delegate that can receive drag information about drag events.
  WebDragDestDelegate* delegate_;

  // The data for the current drag, or NULL if |context_| is NULL.
  scoped_ptr<DropData> drop_data_;

  // True if the drag has been canceled.
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(WebDragDest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_WIN_H_
