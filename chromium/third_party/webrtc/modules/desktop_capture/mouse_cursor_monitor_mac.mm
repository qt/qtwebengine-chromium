/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#include <assert.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class MouseCursorMonitorMac : public MouseCursorMonitor {
 public:
  MouseCursorMonitorMac(CGWindowID window_id);
  virtual ~MouseCursorMonitorMac();

  virtual void Init(Callback* callback, Mode mode) OVERRIDE;
  virtual void Capture() OVERRIDE;

 private:
  void CaptureImage();

  CGWindowID window_id_;

  Callback* callback_;
  Mode mode_;

  scoped_ptr<MouseCursor> last_cursor_;
};

MouseCursorMonitorMac::MouseCursorMonitorMac(CGWindowID window_id)
    : window_id_(window_id),
      callback_(NULL),
      mode_(SHAPE_AND_POSITION) {
}

MouseCursorMonitorMac::~MouseCursorMonitorMac() {}

void MouseCursorMonitorMac::Init(Callback* callback, Mode mode) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
  mode_ = mode;
}

void MouseCursorMonitorMac::Capture() {
  assert(callback_);

  CaptureImage();

  if (mode_ != SHAPE_AND_POSITION)
    return;

  CursorState state = INSIDE;

  CGEventRef event = CGEventCreate(NULL);
  CGPoint gc_position = CGEventGetLocation(event);
  CFRelease(event);

  DesktopVector position(gc_position.x, gc_position.y);

  // If we are capturing cursor for a specific window then we need to figure out
  // if the current mouse position is covered by another window and also adjust
  // |position| to make it relative to the window origin.
  if (window_id_ != kCGNullWindowID) {
    // Get list of windows that may be covering parts of |window_id_|.
    // CGWindowListCopyWindowInfo() returns windows in order from front to back,
    // so |window_id_| is expected to be the last in the list.
    CFArrayRef window_array =
        CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly |
                                       kCGWindowListOptionOnScreenAboveWindow |
                                       kCGWindowListOptionIncludingWindow,
                                   window_id_);
    bool found_window = false;
    if (window_array) {
      CFIndex count = CFArrayGetCount(window_array);
      for (CFIndex i = 0; i < count; ++i) {
        CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(window_array, i));

        // Skip the Dock window. Dock window covers the whole screen, but it is
        // transparent.
        CFStringRef window_name = reinterpret_cast<CFStringRef>(
            CFDictionaryGetValue(window, kCGWindowName));
        if (window_name && CFStringCompare(window_name, CFSTR("Dock"), 0) == 0)
          continue;

        CFDictionaryRef window_bounds = reinterpret_cast<CFDictionaryRef>(
            CFDictionaryGetValue(window, kCGWindowBounds));
        CFNumberRef window_number = reinterpret_cast<CFNumberRef>(
            CFDictionaryGetValue(window, kCGWindowNumber));

        if (window_bounds && window_number) {
          CGRect gc_window_rect;
          if (!CGRectMakeWithDictionaryRepresentation(window_bounds,
                                                      &gc_window_rect)) {
            continue;
          }
          DesktopRect window_rect =
              DesktopRect::MakeXYWH(gc_window_rect.origin.x,
                                    gc_window_rect.origin.y,
                                    gc_window_rect.size.width,
                                    gc_window_rect.size.height);

          CGWindowID window_id;
          if (!CFNumberGetValue(window_number, kCFNumberIntType, &window_id))
            continue;

          if (window_id == window_id_) {
            found_window = true;
            if (!window_rect.Contains(position))
              state = OUTSIDE;
            position = position.subtract(window_rect.top_left());

            assert(i == count - 1);
            break;
          } else if (window_rect.Contains(position)) {
            state = OUTSIDE;
            position.set(-1, -1);
            break;
          }
        }
      }

      CFRelease(window_array);
    }

    if (!found_window) {
      // If we failed to get list of windows or the window wasn't in the list
      // pretend that the cursor is outside the window. This can happen, e.g. if
      // the window was closed.
      state = OUTSIDE;
      position.set(-1, -1);
    }
  }

  callback_->OnMouseCursorPosition(state, position);
}

void MouseCursorMonitorMac::CaptureImage() {
  NSCursor* nscursor = [NSCursor currentSystemCursor];

  NSImage* nsimage = [nscursor image];
  NSSize nssize = [nsimage size];
  DesktopSize size(nssize.width, nssize.height);
  NSPoint nshotspot = [nscursor hotSpot];
  DesktopVector hotspot(
      std::max(0, std::min(size.width(), static_cast<int>(nshotspot.x))),
      std::max(0, std::min(size.height(), static_cast<int>(nshotspot.y))));
  CGImageRef cg_image =
      [nsimage CGImageForProposedRect:NULL context:nil hints:nil];
  if (!cg_image)
    return;

  if (CGImageGetBitsPerPixel(cg_image) != DesktopFrame::kBytesPerPixel * 8 ||
      CGImageGetBytesPerRow(cg_image) !=
          static_cast<size_t>(DesktopFrame::kBytesPerPixel * size.width()) ||
      CGImageGetBitsPerComponent(cg_image) != 8) {
    return;
  }

  CGDataProviderRef provider = CGImageGetDataProvider(cg_image);
  CFDataRef image_data_ref = CGDataProviderCopyData(provider);
  if (image_data_ref == NULL)
    return;

  const uint8_t* src_data =
      reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(image_data_ref));

  // Compare the cursor with the previous one.
  if (last_cursor_.get() &&
      last_cursor_->image().size().equals(size) &&
      last_cursor_->hotspot().equals(hotspot) &&
      memcmp(last_cursor_->image().data(), src_data,
             last_cursor_->image().stride() * size.height()) == 0) {
    return;
  }

  // Create a MouseCursor that describes the cursor and pass it to
  // the client.
  scoped_ptr<DesktopFrame> image(
      new BasicDesktopFrame(DesktopSize(size.width(), size.height())));
  memcpy(image->data(), src_data,
         size.width() * size.height() * DesktopFrame::kBytesPerPixel);

  CFRelease(image_data_ref);

  scoped_ptr<MouseCursor> cursor(new MouseCursor(image.release(), hotspot));
  last_cursor_.reset(MouseCursor::CopyOf(*cursor));

  callback_->OnMouseCursor(cursor.release());
}


MouseCursorMonitor* MouseCursorMonitor::CreateForWindow(
    const DesktopCaptureOptions& options, WindowId window) {
  return new MouseCursorMonitorMac(window);
}

MouseCursorMonitor* MouseCursorMonitor::CreateForScreen(
    const DesktopCaptureOptions& options) {
  return new MouseCursorMonitorMac(kCGNullWindowID);
}

}  // namespace webrtc
