# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {
        'compile_suid_client': 1,
        'compile_credentials': 1,
      }, {
        'compile_suid_client': 0,
        'compile_credentials': 0,
      }],
      ['((OS=="linux" or OS=="android") and '
             '(target_arch=="ia32" or target_arch=="x64" or '
              'target_arch=="arm"))', {
        'compile_seccomp_bpf': 1,
      }, {
        'compile_seccomp_bpf': 0,
      }],
      ['OS=="linux" and (target_arch=="ia32" or target_arch=="x64")', {
        'compile_seccomp_bpf_demo': 1,
      }, {
        'compile_seccomp_bpf_demo': 0,
      }],
    ],
  },
  'target_defaults': {
    'target_conditions': [
      # All linux/ files will automatically be excluded on Android
      # so make sure we re-include them explicitly.
      ['OS == "android"', {
        'sources/': [
          ['include', '^linux/'],
        ],
      }],
    ],
  },
  'targets': [
    # We have two principal targets: sandbox and sandbox_linux_unittests
    # All other targets are listed as dependencies.
    # FIXME(jln): for historial reasons, sandbox_linux is the setuid sandbox
    # and is its own target.
    {
      'target_name': 'sandbox',
      'type': 'none',
      'dependencies': [
        'sandbox_services',
      ],
      'conditions': [
        [ 'compile_suid_client==1', {
          'dependencies': [
            'suid_sandbox_client',
          ],
        }],
        # Compile seccomp BPF when we support it.
        [ 'compile_seccomp_bpf==1', {
          'dependencies': [
            'seccomp_bpf',
            'seccomp_bpf_helpers',
          ],
        }],
      ],
    },
    {
      # The main sandboxing test target.
      'target_name': 'sandbox_linux_unittests',
      'includes': [
        'sandbox_linux_test_sources.gypi',
      ],
      'type': 'executable',
    },
    {
      # This target is the shared library used by Android APK (i.e.
      # JNI-friendly) tests.
      'target_name': 'sandbox_linux_jni_unittests',
      'includes': [
        'sandbox_linux_test_sources.gypi',
      ],
      'type': 'shared_library',
      'conditions': [
        [ 'OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
          'ldflags!': [
              # Remove warnings about text relocations, to prevent build
              # failure.
              '-Wl,--warn-shared-textrel'
          ],
        }],
      ],
    },
    {
      'target_name': 'seccomp_bpf',
      'type': 'static_library',
      'sources': [
        'seccomp-bpf/basicblock.cc',
        'seccomp-bpf/basicblock.h',
        'seccomp-bpf/codegen.cc',
        'seccomp-bpf/codegen.h',
        'seccomp-bpf/die.cc',
        'seccomp-bpf/die.h',
        'seccomp-bpf/errorcode.cc',
        'seccomp-bpf/errorcode.h',
        'seccomp-bpf/instruction.h',
        'seccomp-bpf/linux_seccomp.h',
        'seccomp-bpf/sandbox_bpf.cc',
        'seccomp-bpf/sandbox_bpf.h',
        'seccomp-bpf/sandbox_bpf_policy.h',
        'seccomp-bpf/syscall.cc',
        'seccomp-bpf/syscall.h',
        'seccomp-bpf/syscall_iterator.cc',
        'seccomp-bpf/syscall_iterator.h',
        'seccomp-bpf/trap.cc',
        'seccomp-bpf/trap.h',
        'seccomp-bpf/verifier.cc',
        'seccomp-bpf/verifier.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'sandbox_services_headers',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      'target_name': 'seccomp_bpf_helpers',
      'type': 'static_library',
      'sources': [
        'seccomp-bpf-helpers/baseline_policy.cc',
        'seccomp-bpf-helpers/baseline_policy.h',
        'seccomp-bpf-helpers/sigsys_handlers.cc',
        'seccomp-bpf-helpers/sigsys_handlers.h',
        'seccomp-bpf-helpers/syscall_parameters_restrictions.cc',
        'seccomp-bpf-helpers/syscall_parameters_restrictions.h',
        'seccomp-bpf-helpers/syscall_sets.cc',
        'seccomp-bpf-helpers/syscall_sets.h',
      ],
      'dependencies': [
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      # A demonstration program for the seccomp-bpf sandbox.
      'target_name': 'seccomp_bpf_demo',
      'conditions': [
        ['compile_seccomp_bpf_demo==1', {
          'type': 'executable',
          'sources': [
            'seccomp-bpf/demo.cc',
          ],
          'dependencies': [
            'seccomp_bpf',
          ],
        }, {
          'type': 'none',
        }],
      ],
      'include_dirs': [
        '../../',
      ],
    },
    {
      # The setuid sandbox, for Linux
      'target_name': 'chrome_sandbox',
      'type': 'executable',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
        'suid/linux_util.c',
        'suid/linux_util.h',
        'suid/process_util.h',
        'suid/process_util_linux.c',
        'suid/sandbox.c',
      ],
      'cflags': [
        # For ULLONG_MAX
        '-std=gnu99',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    { 'target_name': 'sandbox_services',
      'type': 'static_library',
      'sources': [
        'services/broker_process.cc',
        'services/broker_process.h',
        'services/init_process_reaper.cc',
        'services/init_process_reaper.h',
        'services/thread_helpers.cc',
        'services/thread_helpers.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['compile_credentials==1', {
          'sources': [
            'services/credentials.cc',
            'services/credentials.h',
          ],
          'dependencies': [
            # for capabilities.cc.
            '../build/linux/system.gyp:libcap',
          ],
        }],
      ],
      'include_dirs': [
        '..',
      ],
    },
    { 'target_name': 'sandbox_services_headers',
      'type': 'none',
      'sources': [
        'services/android_arm_ucontext.h',
        'services/android_ucontext.h',
        'services/android_i386_ucontext.h',
        'services/arm_linux_syscalls.h',
        'services/linux_syscalls.h',
        'services/x86_32_linux_syscalls.h',
        'services/x86_64_linux_syscalls.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # We make this its own target so that it does not interfere
      # with our tests.
      'target_name': 'libc_urandom_override',
      'type': 'static_library',
      'sources': [
        'services/libc_urandom_override.cc',
        'services/libc_urandom_override.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'suid_sandbox_client',
      'type': 'static_library',
      'sources': [
        'suid/common/sandbox.h',
        'suid/common/suid_unsafe_environment_variables.h',
        'suid/client/setuid_sandbox_client.cc',
        'suid/client/setuid_sandbox_client.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'sandbox_services',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
  'conditions': [
    # Strategy copied from base_unittests_apk in base/base.gyp.
    [ 'OS=="android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
        'target_name': 'sandbox_linux_jni_unittests_apk',
        'type': 'none',
        'variables': {
          'test_suite_name': 'sandbox_linux_jni_unittests',
          'input_shlib_path':
              '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)'
              'sandbox_linux_jni_unittests'
              '<(SHARED_LIB_SUFFIX)',
        },
        'dependencies': [
          'sandbox_linux_jni_unittests',
        ],
        'includes': [ '../../build/apk_test.gypi' ],
        }
      ],
    }],
  ],
}
