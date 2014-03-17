# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'oak',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../../url/url.gyp:url_lib',
        '../aura/aura.gyp:aura',
        '../compositor/compositor.gyp:compositor',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../resources/ui_resources.gyp:ui_resources',
        '../ui.gyp:ui',
        '../views/views.gyp:views',
      ],
      'defines': [
        'OAK_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under oak, except unittests
        'oak.h',
        'oak_aura_window_display.cc',
        'oak_aura_window_display.h',
        'oak_export.h',
        'oak_pretty_print.cc',
        'oak_pretty_print.h',
        'oak_tree_model.cc',
        'oak_tree_model.h',
        'oak_window.cc',
        'oak_window.h',
        'oak_details_model.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
