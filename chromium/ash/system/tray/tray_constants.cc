// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_constants.h"

#include "ash/ash_switches.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

const int kPaddingFromRightEdgeOfScreenBottomAlignment = 7;
const int kPaddingFromBottomOfScreenBottomAlignment = 7;
const int kPaddingFromOuterEdgeOfLauncherVerticalAlignment = 8;
const int kPaddingFromInnerEdgeOfLauncherVerticalAlignment = 9;
const int kPaddingFromBottomOfScreenVerticalAlignment = 10;

// Inset between the edge of the shelf region and the status tray.
const int kPaddingFromEdgeOfShelf = 3;

// Top inset of system tray bubble for bottom anchor alignment.
const int kTrayBubbleAnchorTopInsetBottomAnchor = 3;

const int kTrayImageItemHorizontalPaddingBottomAlignment = 1;
const int kTrayImageItemHorizontalPaddingVerticalAlignment = 1;
const int kTrayImageItemVerticalPaddingVerticalAlignment = 1;

const int kTrayLabelItemHorizontalPaddingBottomAlignment = 7;
const int kTrayLabelItemVerticalPaddingVeriticalAlignment = 4;

const int kTrayMenuBottomRowPadding = 5;
const int kTrayMenuBottomRowPaddingBetweenItems = -1;

const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupAutoCloseDelayForTextInSeconds = 5;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupPaddingBetweenItems = 10;
const int kTrayPopupTextSpacingVertical = 4;

const int kTrayPopupItemHeight = 48;
const int kTrayPopupDetailsIconWidth = 25;
const int kTrayPopupScrollSeparatorHeight = 15;
const int kTrayRoundedBorderRadius = 2;
const int kTrayBarButtonWidth = 39;

const SkColor kBackgroundColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kHoverBackgroundColor = SkColorSetRGB(0xf3, 0xf3, 0xf3);
const SkColor kPublicAccountBackgroundColor = SkColorSetRGB(0xf8, 0xe5, 0xb6);
const SkColor kPublicAccountUserCardTextColor = SkColorSetRGB(0x66, 0x66, 0x66);
const SkColor kPublicAccountUserCardNameColor = SK_ColorBLACK;

const SkColor kHeaderBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);

const SkColor kBorderDarkColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kBorderLightColor = SkColorSetRGB(0xeb, 0xeb, 0xeb);
const SkColor kButtonStrokeColor = SkColorSetRGB(0xdd, 0xdd, 0xdd);

const SkColor kHeaderTextColorNormal = SkColorSetARGB(0x7f, 0, 0, 0);
const SkColor kHeaderTextColorHover = SkColorSetARGB(0xd3, 0, 0, 0);

const int kTrayPopupMinWidth = 300;
const int kTrayPopupMaxWidth = 500;
const int kNotificationIconWidth = 40;
const int kNotificationButtonWidth = 32;
const int kTrayNotificationContentsWidth = kTrayPopupMinWidth -
    (kNotificationIconWidth + kNotificationButtonWidth +
     (kTrayPopupPaddingHorizontal / 2) * 3);
const int kTraySpacing = 8;
const int kAlternateTraySpacing = 4;

// Returns kTraySpacing or kAlternateTraySpacing as applicable
// (Determined by ash::switches::UseAlternateShelfLayout).
int GetTraySpacing() {
  return ash::switches::UseAlternateShelfLayout() ?
      kAlternateTraySpacing : kTraySpacing;
}

}  // namespace ash
