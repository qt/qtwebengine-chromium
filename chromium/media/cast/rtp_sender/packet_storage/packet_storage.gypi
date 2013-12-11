# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'packet_storage',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'packet_storage.h',
        'packet_storage.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
  ],
}

