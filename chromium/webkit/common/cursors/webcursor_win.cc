// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/pickle.h"
#include "grit/ui_unscaled_resources.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/icon_util.h"
#include "webkit/common/cursors/webcursor.h"

using blink::WebCursorInfo;

static LPCWSTR ToCursorID(WebCursorInfo::Type type) {
  switch (type) {
    case WebCursorInfo::TypePointer:
      return IDC_ARROW;
    case WebCursorInfo::TypeCross:
      return IDC_CROSS;
    case WebCursorInfo::TypeHand:
      return IDC_HAND;
    case WebCursorInfo::TypeIBeam:
      return IDC_IBEAM;
    case WebCursorInfo::TypeWait:
      return IDC_WAIT;
    case WebCursorInfo::TypeHelp:
      return IDC_HELP;
    case WebCursorInfo::TypeEastResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeNorthEastResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeNorthWestResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeEastWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case WebCursorInfo::TypeRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case WebCursorInfo::TypeMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case WebCursorInfo::TypeEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case WebCursorInfo::TypeNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case WebCursorInfo::TypeNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case WebCursorInfo::TypeNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case WebCursorInfo::TypeSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case WebCursorInfo::TypeSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case WebCursorInfo::TypeSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case WebCursorInfo::TypeWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case WebCursorInfo::TypeMove:
      return IDC_SIZEALL;
    case WebCursorInfo::TypeVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case WebCursorInfo::TypeCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case WebCursorInfo::TypeContextMenu:
      return MAKEINTRESOURCE(IDC_ARROW);
    case WebCursorInfo::TypeAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case WebCursorInfo::TypeProgress:
      return IDC_APPSTARTING;
    case WebCursorInfo::TypeNoDrop:
      return IDC_NO;
    case WebCursorInfo::TypeCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case WebCursorInfo::TypeNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case WebCursorInfo::TypeNotAllowed:
      return IDC_NO;
    case WebCursorInfo::TypeZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case WebCursorInfo::TypeZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case WebCursorInfo::TypeGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case WebCursorInfo::TypeGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
  }
  NOTREACHED();
  return NULL;
}

static bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

HCURSOR WebCursor::GetCursor(HINSTANCE module_handle){
  if (!IsCustom()) {
    const wchar_t* cursor_id =
        ToCursorID(static_cast<WebCursorInfo::Type>(type_));

    if (IsSystemCursorID(cursor_id))
      module_handle = NULL;

    return LoadCursor(module_handle, cursor_id);
  }

  if (custom_cursor_) {
    DCHECK(external_cursor_ == NULL);
    return custom_cursor_;
  }

  if (external_cursor_)
    return external_cursor_;

  custom_cursor_ =
      IconUtil::CreateCursorFromDIB(
          custom_size_,
          hotspot_,
          !custom_data_.empty() ? &custom_data_[0] : NULL,
          custom_data_.size());
  return custom_cursor_;
}

gfx::NativeCursor WebCursor::GetNativeCursor() {
  return GetCursor(NULL);
}

void WebCursor::InitPlatformData() {
  custom_cursor_ = NULL;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  // There are some issues with converting certain HCURSORS to bitmaps. The
  // HCURSOR being a user object can be marshaled as is.
  // HCURSORs are always 32 bits on Windows, even on 64 bit systems.
  return pickle->WriteUInt32(reinterpret_cast<uint32>(external_cursor_));
}

bool WebCursor::DeserializePlatformData(PickleIterator* iter) {
  return iter->ReadUInt32(reinterpret_cast<uint32*>(&external_cursor_));
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  if (!IsCustom())
    return true;

  return (external_cursor_ == other.external_cursor_);
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  external_cursor_ = other.external_cursor_;
  // The custom_cursor_ member will be initialized to a HCURSOR the next time
  // the GetCursor member function is invoked on this WebCursor instance. The
  // cursor is created using the data in the custom_data_ vector.
  custom_cursor_ = NULL;
}

void WebCursor::CleanupPlatformData() {
  external_cursor_ = NULL;

  if (custom_cursor_) {
    DestroyIcon(custom_cursor_);
    custom_cursor_ = NULL;
  }
}
