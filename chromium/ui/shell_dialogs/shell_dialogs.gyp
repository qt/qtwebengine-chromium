# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'shell_dialogs',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../base/strings/ui_strings.gyp:ui_strings',
        '../ui.gyp:ui',
      ],
      'defines': [
        'SHELL_DIALOGS_IMPLEMENTATION',
      ],
      'sources': [
        'android/shell_dialogs_jni_registrar.cc',
        'android/shell_dialogs_jni_registrar.h',
        'base_shell_dialog.cc',
        'base_shell_dialog.h',
        'base_shell_dialog_win.cc',
        'base_shell_dialog_win.h',
        'gtk/select_file_dialog_impl.cc',
        'gtk/select_file_dialog_impl.h',
        'gtk/select_file_dialog_impl_gtk.cc',
        'gtk/select_file_dialog_impl_kde.cc',
        'linux_shell_dialog.cc',
        'linux_shell_dialog.h',
        'print_settings_dialog_win.cc',
        'print_settings_dialog_win.h',
        'select_file_dialog.cc',
        'select_file_dialog.h',
        'select_file_dialog_android.cc',
        'select_file_dialog_android.h',
        'select_file_dialog_factory.cc',
        'select_file_dialog_factory.h',
        'select_file_dialog_mac.h',
        'select_file_dialog_mac.mm',
        'select_file_dialog_win.cc',
        'select_file_dialog_win.h',
        'select_file_policy.cc',
        'select_file_policy.h',
        'selected_file_info.cc',
        'selected_file_info.h',
      ],
      'conditions': [
        ['use_aura==1',
          {
            'dependencies': [
              '../aura/aura.gyp:aura',
            ],
            'sources/': [
              ['exclude', 'select_file_dialog_mac.mm'],
            ],
          }
        ],
        ['OS=="android"',
          {
            'dependencies': [
              '../ui.gyp:ui_base_jni_headers',
            ],
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/ui',
            ],
            'link_settings': {
              'libraries': [
                '-ljnigraphics',
              ],
            },
          }
        ],
        ['OS=="android" and android_webview_build==0',
          {
            'dependencies': [
              '../android/ui_android.gyp:ui_java',
            ],
          }
        ],
      ],
    },  # target_name: shell_dialogs
  ],
}
