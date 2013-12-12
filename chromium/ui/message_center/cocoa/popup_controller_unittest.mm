// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/popup_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#include "ui/message_center/notification.h"

class PopupControllerTest : public ui::CocoaTest {
};

TEST_F(PopupControllerTest, Creation) {
  scoped_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          "",
          ASCIIToUTF16("Added to circles"),
          ASCIIToUTF16("Jonathan and 5 others"),
          gfx::Image(),
          string16(),
          message_center::NotifierId(),
          message_center::RichNotificationData(),
          NULL));

  base::scoped_nsobject<MCPopupController> controller(
      [[MCPopupController alloc] initWithNotification:notification.get()
                                        messageCenter:nil
                                      popupCollection:nil]);
  // Add an extra ref count for scoped_nsobject since MCPopupController will
  // release itself when it is being closed.
  [controller retain];

  EXPECT_TRUE([controller window]);
  EXPECT_EQ(notification.get(), [controller notification]);

  [controller showWindow:nil];
  [controller close];
}
