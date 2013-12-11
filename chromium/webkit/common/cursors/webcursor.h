// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_CURSORS_WEBCURSOR_H_
#define WEBKIT_COMMON_CURSORS_WEBCURSOR_H_

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebCursorInfo.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "webkit/common/webkit_common_export.h"

#include <vector>

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

#if defined(OS_WIN)
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#elif defined(TOOLKIT_GTK)
typedef struct _GdkCursor GdkCursor;
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSCursor;
#else
class NSCursor;
#endif
#endif

class Pickle;
class PickleIterator;

// This class encapsulates a cross-platform description of a cursor.  Platform
// specific methods are provided to translate the cross-platform cursor into a
// platform specific cursor.  It is also possible to serialize / de-serialize a
// WebCursor.
class WEBKIT_COMMON_EXPORT WebCursor {
 public:
  struct CursorInfo {
    explicit CursorInfo(WebKit::WebCursorInfo::Type cursor_type)
        : type(cursor_type),
          image_scale_factor(1) {
#if defined(OS_WIN)
      external_handle = NULL;
#endif
    }

    CursorInfo()
        : type(WebKit::WebCursorInfo::TypePointer),
          image_scale_factor(1) {
#if defined(OS_WIN)
      external_handle = NULL;
#endif
    }

    WebKit::WebCursorInfo::Type type;
    gfx::Point hotspot;
    float image_scale_factor;
    SkBitmap custom_image;
#if defined(OS_WIN)
    HCURSOR external_handle;
#endif
  };

  WebCursor();
  explicit WebCursor(const CursorInfo& cursor_info);
  ~WebCursor();

  // Copy constructor/assignment operator combine.
  WebCursor(const WebCursor& other);
  const WebCursor& operator=(const WebCursor& other);

  // Conversion from/to CursorInfo.
  void InitFromCursorInfo(const CursorInfo& cursor_info);
  void GetCursorInfo(CursorInfo* cursor_info) const;

  // Serialization / De-serialization
  bool Deserialize(PickleIterator* iter);
  bool Serialize(Pickle* pickle) const;

  // Returns true if GetCustomCursor should be used to allocate a platform
  // specific cursor object.  Otherwise GetCursor should be used.
  bool IsCustom() const;

  // Returns true if the current cursor object contains the same cursor as the
  // cursor object passed in. If the current cursor is a custom cursor, we also
  // compare the bitmaps to verify whether they are equal.
  bool IsEqual(const WebCursor& other) const;

  // Returns a native cursor representing the current WebCursor instance.
  gfx::NativeCursor GetNativeCursor();

#if defined(OS_WIN)
  // Initialize this from the given Windows cursor. The caller must ensure that
  // the HCURSOR remains valid by not invoking the DestroyCursor/DestroyIcon
  // APIs on it.
  void InitFromExternalCursor(HCURSOR handle);
#endif

#if defined(USE_AURA)
  const ui::PlatformCursor GetPlatformCursor();

  // Updates |device_scale_factor_| and |rotation_| based on |display|.
  void SetDisplayInfo(const gfx::Display& display);

#elif defined(OS_WIN)
  // Returns a HCURSOR representing the current WebCursor instance.
  // The ownership of the HCURSOR (does not apply to external cursors) remains
  // with the WebCursor instance.
  HCURSOR GetCursor(HINSTANCE module_handle);

#elif defined(TOOLKIT_GTK)
  // Return the stock GdkCursorType for this cursor, or GDK_CURSOR_IS_PIXMAP
  // if it's a custom cursor. Return GDK_LAST_CURSOR to indicate that the cursor
  // should be set to the system default.
  // Returns an int so we don't need to include GDK headers in this header file.
  int GetCursorType() const;

  // Return a new GdkCursor* for this cursor.  Only valid if GetCursorType
  // returns GDK_CURSOR_IS_PIXMAP.
  GdkCursor* GetCustomCursor();
#elif defined(OS_MACOSX)
  // Initialize this from the given Cocoa NSCursor.
  void InitFromNSCursor(NSCursor* cursor);
#endif

 private:
  // Copies the contents of the WebCursor instance passed in.
  void Copy(const WebCursor& other);

  // Cleans up the WebCursor instance.
  void Clear();

  // Platform specific initialization goes here.
  void InitPlatformData();

  // Platform specific Serialization / De-serialization
  bool SerializePlatformData(Pickle* pickle) const;
  bool DeserializePlatformData(PickleIterator* iter);

  // Returns true if the platform data in the current cursor object
  // matches that of the cursor passed in.
  bool IsPlatformDataEqual(const WebCursor& other) const ;

  // Copies platform specific data from the WebCursor instance passed in.
  void CopyPlatformData(const WebCursor& other);

  // Platform specific cleanup.
  void CleanupPlatformData();

  void SetCustomData(const SkBitmap& image);
  void ImageFromCustomData(SkBitmap* image) const;

  // Clamp the hotspot to the custom image's bounds, if this is a custom cursor.
  void ClampHotspot();

  // WebCore::PlatformCursor type.
  int type_;

  // Hotspot in cursor image in pixels.
  gfx::Point hotspot_;

  // Custom cursor data, as 32-bit RGBA.
  // Platform-inspecific because it can be serialized.
  gfx::Size custom_size_;  // In pixels.
  float custom_scale_;
  std::vector<char> custom_data_;

#if defined(OS_WIN)
  // An externally generated HCURSOR. We assume that it remains valid, i.e we
  // don't attempt to copy the HCURSOR.
  HCURSOR external_cursor_;
#endif

#if defined(USE_AURA) && defined(USE_X11)
  // Only used for custom cursors.
  ui::PlatformCursor platform_cursor_;
  float device_scale_factor_;
  gfx::Display::Rotation rotation_;
#elif defined(OS_WIN)
  // A custom cursor created from custom bitmap data by Webkit.
  HCURSOR custom_cursor_;
#elif defined(TOOLKIT_GTK)
  // A custom cursor created that should be unref'ed from the destructor.
  GdkCursor* unref_;
#endif
};

#endif  // WEBKIT_COMMON_CURSORS_WEBCURSOR_H_
