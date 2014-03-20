#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  'includes': [
    '../build/features.gypi',
    '../build/scripts/scripts.gypi',
  ],

  'targets': [
    {
      'target_name': 'make_platform_derived_sources',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'FontFamilyNames',
          'inputs': [
            '<@(make_names_files)',
            'fonts/FontFamilyNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/FontFamilyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/FontFamilyNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            'fonts/FontFamilyNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'RuntimeEnabledFeatures',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_runtime_features.py',
            'RuntimeEnabledFeatures.in',
            '../build/scripts/templates/RuntimeEnabledFeatures.cpp.tmpl',
            '../build/scripts/templates/RuntimeEnabledFeatures.h.tmpl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/RuntimeEnabledFeatures.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/RuntimeEnabledFeatures.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_runtime_features.py',
            'RuntimeEnabledFeatures.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'ColorData',
          'inputs': [
            'ColorData.gperf',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/ColorData.cpp',
          ],
          'action': [
            '<(gperf_exe)',
            '--key-positions=*',
            '-D', '-s', '2',
            '<@(_inputs)',
            '--output-file=<(SHARED_INTERMEDIATE_DIR)/blink/ColorData.cpp',
          ],
        },
      ]
    },
  ],
}
