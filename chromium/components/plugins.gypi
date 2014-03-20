# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS != "ios"', {
  
      'targets': [
        {
          'target_name': 'plugins_renderer',
          'type': 'static_library',
          'dependencies': [
            '../skia/skia.gyp:skia',
            '../third_party/re2/re2.gyp:re2',
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'plugins/renderer/plugin_placeholder.cc',
            'plugins/renderer/plugin_placeholder.h',
            'plugins/renderer/webview_plugin.cc',
            'plugins/renderer/webview_plugin.h',
          ],
          'conditions' : [
            ['OS=="android"', {
              'sources': [
                'plugins/renderer/mobile_youtube_plugin.cc',
                'plugins/renderer/mobile_youtube_plugin.h',
              ]
            }],
          ],
        },
      ]
    }]
  ]
}
