# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'openssl',
      'type': '<(component)',
      'includes': [
        # Include the auto-generated gypi file.
        '../../third_party/openssl/openssl.gypi'
      ],
      'variables': {
        'openssl_include_dirs': [
          '.',
          'openssl',
          'openssl/crypto',
          'openssl/crypto/asn1',
          'openssl/crypto/evp',
          'openssl/crypto/modes',
          'openssl/include',
        ],
      },
      'sources': [
        '<@(openssl_common_sources)',
      ],
      'defines': [
        '<@(openssl_common_defines)',
        'PURIFY',
        'MONOLITH',
        'OPENSSL_NO_ASM',
      ],
      'defines!': [
        'TERMIO',
      ],
      'conditions': [
        ['os_posix==1 and OS!="android"', {
          'defines': [
            # ENGINESDIR must be defined if OPENSSLDIR is.
            'ENGINESDIR="/dev/null"',
            # Set to ubuntu default path for convenience. If necessary, override
            # this at runtime with the SSL_CERT_DIR environment variable.
            'OPENSSLDIR="/etc/ssl"',
          ],
        }],
        ['target_arch == "arm"', {
          'sources': [ '<@(openssl_arm_sources)' ],
          'sources!': [ '<@(openssl_arm_source_excludes)' ],
          'defines': [ '<@(openssl_arm_defines)' ],
          'defines!': [ 'OPENSSL_NO_ASM' ],
        }],
        ['target_arch == "mipsel"', {
          'sources': [ '<@(openssl_mips_sources)' ],
          'sources!': [ '<@(openssl_mips_source_excludes)' ],
          'defines': [ '<@(openssl_mips_defines)' ],
          'defines!': [ 'OPENSSL_NO_ASM' ],
        }],
        ['target_arch == "ia32"', {
          'sources': [ '<@(openssl_x86_sources)' ],
          'sources!': [ '<@(openssl_x86_source_excludes)' ],
          'defines': [ '<@(openssl_x86_defines)' ],
          'defines!': [ 'OPENSSL_NO_ASM' ],
        }],
        ['target_arch == "x64"', {
          'sources': [ '<@(openssl_x86_64_sources)' ],
          'sources!': [ '<@(openssl_x86_64_source_excludes)' ],
          'conditions': [
            ['OS != "android"', {
              # Because rc4-x86_64.S has a problem,
              # We use the C rc4 source instead of the ASM source.
              # This hurts performance, but it's not a problem
              # because no production code uses openssl on x86-64.
              'sources/': [
                ['exclude', 'openssl/crypto/rc4/asm/rc4-x86_64\\.S' ],
                ['include', 'openssl/crypto/rc4/rc4_enc\\.c' ],
                ['include', 'openssl/crypto/rc4/rc4_skey\\.c' ],
              ],
            }]
          ],
          'defines': [ '<@(openssl_x86_64_defines)' ],
          'defines!': [ 'OPENSSL_NO_ASM' ],
          'variables': {
            # Ensure the 64-bit opensslconf.h header is used.
            'openssl_include_dirs+': [ 'config/x64' ],
          },
        }],
        ['component == "shared_library"', {
          'cflags!': ['-fvisibility=hidden'],
        }],
        ['clang==1', {
          'cflags': [
            # OpenSSL has a few |if ((foo == NULL))| checks.
            '-Wno-parentheses-equality',
            # OpenSSL uses several function-style macros and then ignores the
            # returned value.
            '-Wno-unused-value',
          ],
        }, { # Not clang. Disable all warnings.
          'cflags': [
            '-w',
          ],
        }]
      ],
      'include_dirs': [
        '<@(openssl_include_dirs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'openssl/include',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
