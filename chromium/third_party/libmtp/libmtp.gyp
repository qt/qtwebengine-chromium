# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_libmtp%': 0,
  },
  'conditions': [
    ['use_system_libmtp==0', {
      'targets': [
        {
          'target_name': 'libmtp',
          'type': 'shared_library',
          'product_name': 'mtp',
          'dependencies': [
            '../../third_party/libusb/libusb.gyp:libusb',
          ],
          'sources': [
            'src/libmtp.c',
            'src/libusb1-glue.c',
            'src/ptp.c',
            'src/unicode.c',
            'src/util.c',
          ],
          'cflags!': ['-fvisibility=hidden'],
          'include_dirs': [
            '.',
            'src',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
            ],
          },
        },
      ],
    }, { # use_system_libmtp==1
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
      'targets': [
        {
          'target_name': 'libmtp',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
                '<!@(<(pkg-config) --cflags libmtp)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libmtp)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libmtp)',
            ],
          },
        }
      ],
    }],
  ]
}
