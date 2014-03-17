// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_SOURCE_GTK_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_SOURCE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector2d.h"
#include "url/gurl.h"

class SkBitmap;

namespace content {

class RenderViewHostImpl;
class WebContentsImpl;
struct DropData;

// WebDragSourceGtk takes care of managing the drag from a WebContents
// with Gtk.
class CONTENT_EXPORT WebDragSourceGtk :
    public base::MessageLoopForUI::Observer {
 public:
  explicit WebDragSourceGtk(WebContents* web_contents);
  virtual ~WebDragSourceGtk();

  // Starts a drag for the WebContents this WebDragSourceGtk was created for.
  // Returns false if the drag could not be started.
  bool StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     GdkEventButton* last_mouse_down,
                     const SkBitmap& image,
                     const gfx::Vector2d& image_offset);

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE;
  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_2(WebDragSourceGtk, gboolean, OnDragFailed,
                       GdkDragContext*, GtkDragResult);
  CHROMEGTK_CALLBACK_1(WebDragSourceGtk, void, OnDragBegin,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_1(WebDragSourceGtk, void, OnDragEnd,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_4(WebDragSourceGtk, void, OnDragDataGet,
                       GdkDragContext*, GtkSelectionData*, guint, guint);
  CHROMEGTK_CALLBACK_1(WebDragSourceGtk, gboolean, OnDragIconExpose,
                       GdkEventExpose*);

  gfx::NativeView GetContentNativeView() const;

  // The tab we're manging the drag for.
  WebContentsImpl* web_contents_;

  // The drop data for the current drag (for drags that originate in the render
  // view). Non-NULL iff there is a current drag.
  scoped_ptr<DropData> drop_data_;

  // The image used for depicting the drag, and the offset between the cursor
  // and the top left pixel.
  GdkPixbuf* drag_pixbuf_;
  gfx::Vector2d image_offset_;

  // The mime type for the file contents of the current drag (if any).
  GdkAtom drag_file_mime_type_;

  // Whether the current drag has failed. Meaningless if we are not the source
  // for a current drag.
  bool drag_failed_;

  // This is the widget we use to initiate drags. Since we don't use the
  // renderer widget, we can persist drags even when our contents is switched
  // out. We can't use an OwnedWidgetGtk because the GtkInvisible widget
  // initialization code sinks the reference.
  GtkWidget* drag_widget_;

  // Context created once drag starts.  A NULL value indicates that there is
  // no drag currently in progress.
  GdkDragContext* drag_context_;

  // The file mime type for a drag-out download.
  base::string16 wide_download_mime_type_;

  // The file name to be saved to for a drag-out download.
  base::FilePath download_file_name_;

  // The URL to download from for a drag-out download.
  GURL download_url_;

  // The widget that provides visual feedback for the drag. We can't use
  // an OwnedWidgetGtk because the GtkWindow initialization code sinks
  // the reference.
  GtkWidget* drag_icon_;

  ui::GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSourceGtk);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_SOURCE_GTK_H_
