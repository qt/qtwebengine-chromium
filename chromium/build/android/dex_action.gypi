# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into an action to provide a rule that dexes
# compiled java files. If proguard_enabled == "true" and CONFIGURATION_NAME ==
# "Release", then it will dex the proguard_enabled_input_path instead of the
# normal dex_input_paths/dex_generated_input_paths.
#
# To use this, create a gyp target with the following form:
#  {
#    'action_name': 'some name for the action'
#    'actions': [
#      'variables': {
#        'dex_input_paths': [ 'files to dex (when proguard is not used) and add to input paths' ],
#        'dex_generated_input_dirs': [ 'dirs that contain generated files to dex' ],
#        'input_paths': [ 'additional files to be added to the list of inputs' ],
#
#        # For targets that use proguard:
#        'proguard_enabled': 'true',
#        'proguard_enabled_input_path': 'path to dex when using proguard',
#      },
#      'includes': [ 'relative/path/to/dex_action.gypi' ],
#    ],
#  },
#

{
  'message': 'Creating dex file: <(output_path)',
  'variables': {
    'dex_input_paths': [],
    'dex_generated_input_dirs': [],
    'input_paths': [],
    'proguard_enabled%': 'false',
    'proguard_enabled_input_path%': '',
    'dex_no_locals%': 0,
  },
  'inputs': [
    '<(DEPTH)/build/android/gyp/util/build_utils.py',
    '<(DEPTH)/build/android/gyp/util/md5_check.py',
    '<(DEPTH)/build/android/gyp/dex.py',
    '>@(input_paths)',
    '>@(dex_input_paths)',
  ],
  'outputs': [
    '<(output_path)',
  ],
  'action': [
    'python', '<(DEPTH)/build/android/gyp/dex.py',
    '--dex-path=<(output_path)',
    '--android-sdk-tools=<(android_sdk_tools)',
    '--configuration-name=<(CONFIGURATION_NAME)',
    '--proguard-enabled=<(proguard_enabled)',
    '--proguard-enabled-input-path=<(proguard_enabled_input_path)',
    '--no-locals=<(dex_no_locals)',

    # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
    '--ignore=>!(echo \'>(_inputs)\' | md5sum)',

    '>@(dex_input_paths)',
    '>@(dex_generated_input_dirs)',
  ]
}
