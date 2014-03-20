# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    ['OS=="android"',
      {
        'targets': [
          {
            'target_name': 'overscroller_jni_headers',
            'type': 'none',
            'variables': {
              'jni_gen_package': 'webkit',
              'input_java_class': 'android/widget/OverScroller.class',
            },
            'includes': [ '../../build/jar_file_jni_generator.gypi' ],
          },
        ],
      }
    ],
  ],
  'targets': [
    {
      'target_name': 'webkit_child',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_CHILD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/native_theme/native_theme.gyp:native_theme',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/common/user_agent/webkit_user_agent.gyp:user_agent',
        '<(DEPTH)/webkit/common/webkit_common.gyp:webkit_common',
      ],
      'include_dirs': [
        # For JNI generated header.
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      # This target exports a hard dependency because dependent targets may
      # include the header generated above.
      'hard_dependency': 1,
      'sources': [
        'fling_animator_impl_android.cc',
        'fling_animator_impl_android.h',
        'fling_curve_configuration.cc',
        'fling_curve_configuration.h',
        'ftp_directory_listing_response_delegate.cc',
        'ftp_directory_listing_response_delegate.h',
        'multipart_response_delegate.cc',
        'multipart_response_delegate.h',
        'resource_loader_bridge.cc',
        'resource_loader_bridge.h',
        'touch_fling_gesture_curve.cc',
        'touch_fling_gesture_curve.h',
        'web_discardable_memory_impl.cc',
        'web_discardable_memory_impl.h',
        'webfallbackthemeengine_impl.cc',
        'webfallbackthemeengine_impl.h',
        'webkit_child_export.h',
        'webkit_child_helpers.cc',
        'webkit_child_helpers.h',
        'webkitplatformsupport_child_impl.cc',
        'webkitplatformsupport_child_impl.h',
        'webkitplatformsupport_impl.cc',
        'webkitplatformsupport_impl.h',
        'websocketstreamhandle_bridge.h',
        'websocketstreamhandle_delegate.h',
        'websocketstreamhandle_impl.cc',
        'websocketstreamhandle_impl.h',
        'webthemeengine_impl_android.cc',
        'webthemeengine_impl_android.h',
        'webthemeengine_impl_default.cc',
        'webthemeengine_impl_default.h',
        'webthemeengine_impl_mac.cc',
        'webthemeengine_impl_mac.h',
        'webthemeengine_impl_win.cc',
        'webthemeengine_impl_win.h',
        'webthread_impl.cc',
        'webthread_impl.h',
        'weburlloader_impl.cc',
        'weburlloader_impl.h',
        'weburlrequest_extradata_impl.cc',
        'weburlrequest_extradata_impl.h',
        'weburlresponse_extradata_impl.cc',
        'weburlresponse_extradata_impl.h',
        'worker_task_runner.cc',
        'worker_task_runner.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267 ],
      'conditions': [
        ['use_default_render_theme==0',
          {
            'sources/': [
              ['exclude', 'webthemeengine_impl_default.cc'],
              ['exclude', 'webthemeengine_impl_default.h'],
            ],
          }
        ],
        ['OS=="mac"',
          {
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
              ],
            },
          }
        ],
        ['OS=="android"',
          {
            'dependencies': [
              'overscroller_jni_headers',
            ],
          }
        ],
      ],
    },
  ],
}
