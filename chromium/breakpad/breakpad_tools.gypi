# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="linux" or OS=="android"', {
      'variables': {
        'host_arch': '<!(uname -m)',
      },
      'conditions': [
        ['host_arch=="x86_64"', {
          'cflags!': ['-m32', '-march=pentium4', '-msse2', '-mfpmath=sse'],
          'ldflags!': ['-m32'],
          'cflags': ['-O2'],
          'include_dirs!': ['/usr/include32'],
          'conditions': [
            ['target_arch=="ia32" and sysroot!=""', {
              'cflags!': ['--sysroot=<(sysroot)'],
            }],
          ],
        }],
      ],
    }],
    ['OS=="android"', {
      'toolsets': ['host'],
      'defines': ['__ANDROID__'],
    }],
    ['clang==1', {
      'cflags': ['-Wno-tautological-constant-out-of-range-compare'],
      'xcode_settings': {
        'WARNING_CFLAGS': ['-Wno-tautological-constant-out-of-range-compare'],
      },
    }],
  ],
  'include_dirs': [
    'src',
    'src/third_party',
  ],
}
