// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_DELEGATE_H_
#define ASH_MEDIA_DELEGATE_H_

namespace ash {

// A delegate class to control media playback.
class MediaDelegate {
 public:
  virtual ~MediaDelegate() {}

  // Handles the Next Track Media shortcut key.
  virtual void HandleMediaNextTrack() = 0;

  // Handles the Play/Pause Toggle Media shortcut key.
  virtual void HandleMediaPlayPause() = 0;

  // Handles the Previous Track Media shortcut key.
  virtual void HandleMediaPrevTrack() = 0;
};

}  // namespace ash

#endif  // ASH_MEDIA_DELEGATE_H_
