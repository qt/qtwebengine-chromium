// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_REQUEST_H_
#define CC_RESOURCES_UI_RESOURCE_REQUEST_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class CC_EXPORT UIResourceRequest {
 public:
  enum UIResourceRequestType {
    UIResourceCreate,
    UIResourceDelete,
    UIResourceInvalidRequest
  };

  UIResourceRequest(UIResourceRequestType type, UIResourceId id);
  UIResourceRequest(UIResourceRequestType type,
                    UIResourceId id,
                    const UIResourceBitmap& bitmap);
  UIResourceRequest(const UIResourceRequest& request);

  ~UIResourceRequest();

  UIResourceRequestType GetType() const { return type_; }
  UIResourceId GetId() const { return id_; }
  UIResourceBitmap GetBitmap() const {
    DCHECK(bitmap_);
    return *bitmap_.get();
  }

  UIResourceRequest& operator=(const UIResourceRequest& request);

 private:
  UIResourceRequestType type_;
  UIResourceId id_;
  scoped_ptr<UIResourceBitmap> bitmap_;
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_REQUEST_H_
