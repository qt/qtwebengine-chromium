# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
     'audio_receiver/audio_receiver.gypi',
     'video_receiver/video_receiver.gypi',
  ],
  'targets': [
    {
      'target_name': 'cast_receiver_impl',
      'type': 'static_library',
      'sources': [
        'cast_receiver.h',
#       'cast_receiver_impl.cc',
#       'cast_receiver_impl.h',
      ], # source
      'dependencies': [
        'rtp_receiver/rtp_receiver.gyp:*',
        'cast_audio_receiver',
        'cast_video_receiver',
        'framer/framer.gyp:cast_framer',
        'pacing/paced_sender.gyp:paced_sender',
      ],
    },
  ],
}
