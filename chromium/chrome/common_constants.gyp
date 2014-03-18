# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'includes': [
    'version.gypi',
  ],

  'target_defaults': {
    'sources': [
      # TODO(yoz): Create an extension_constants target for these.
      '../extensions/common/constants.cc',
      '../extensions/common/constants.h',
      'common/chrome_constants.cc',
      'common/chrome_constants.h',
      'common/chrome_paths.cc',
      'common/chrome_paths.h',
      'common/chrome_paths_android.cc',
      'common/chrome_paths_internal.h',
      'common/chrome_paths_linux.cc',
      'common/chrome_paths_mac.mm',
      'common/chrome_paths_win.cc',
      'common/chrome_switches.cc',
      'common/chrome_switches.h',
      'common/env_vars.cc',
      'common/env_vars.h',
      'common/net/test_server_locations.cc',
      'common/net/test_server_locations.h',
      'common/pref_font_script_names-inl.h',
      'common/pref_font_webkit_names.h',
      'common/pref_names.cc',
      'common/pref_names.h',
      'common/widevine_cdm_constants.cc',
      'common/widevine_cdm_constants.h',
    ],
    'actions': [
      {
        'action_name': 'Make chrome_version.cc',
        'variables': {
          'make_version_cc_path': 'tools/build/make_version_cc.py',
        },
        'inputs': [
          '<(make_version_cc_path)',
          'VERSION',
        ],
        'outputs': [
          '<(INTERMEDIATE_DIR)/chrome_version.cc',
        ],
        'action': [
          'python',
          '<(make_version_cc_path)',
          '<@(_outputs)',
          '<(version_full)',
        ],
        'process_outputs_as_sources': 1,
      },
    ],
  },
  'targets': [
    {
      'target_name': 'common_constants',
      'type': 'static_library',
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',  # Needed by chrome_paths.cc.
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../components/nacl.gyp:nacl_switches',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'target_conditions': [
        ['OS=="ios"', {
          # iOS needs chrome_paths_mac, which is excluded by filename rules;
          # re-add it in target_conditionals so it's after that exclusion.
          'sources/': [
            ['include', '^common/chrome_paths_mac\\.mm$'],
          ],
        }],
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../build/linux/system.gyp:gtk'],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'common_constants_win64',
          'type': 'static_library',
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)',  # Needed by chrome_paths.cc.
          ],
          'dependencies': [
            '../base/base.gyp:base_nacl_win64',
            '../components/nacl.gyp:nacl_switches_win64',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
            'COMPILE_CONTENT_STATICALLY',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
