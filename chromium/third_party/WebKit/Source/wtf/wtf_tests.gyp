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
{
  'includes': [
    '../build/win/precompile.gypi',
    'wtf.gypi',
  ],
  'targets': [
    {
      'target_name': 'wtf_unittests',
      'type': 'executable',
      'dependencies': [
        'run_all_tests',
        'wtf_unittest_helpers',
        'wtf.gyp:wtf',
        '../config.gyp:unittest_config',
      ],
      'sources': [
        '<@(wtf_unittest_files)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [4127, 4510, 4512, 4610, 4706, 4068, 4267],
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="android" and OS!="ios" and linux_use_tcmalloc==1', {
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }]
      ]
    },
    {
      'target_name': 'run_all_tests',
      'type': 'static_library',
      'dependencies': [
        '../config.gyp:unittest_config',
        '<(DEPTH)/base/base.gyp:test_support_base',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:test_support_base',
      ],
      'sources': [
        'testing/RunAllTests.cpp',
      ]
    },
    {
      'target_name': 'wtf_unittest_helpers',
      'type': '<(component)',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'wtf.gyp:wtf',
      ],
      'defines': [
        'WTF_UNITTEST_HELPERS_IMPLEMENTATION=1',
      ],
      'sources': [
        '<@(wtf_unittest_helper_files)',
      ],
    },
  ],
}
