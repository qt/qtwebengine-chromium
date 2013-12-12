# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'sources_for_standard_interfaces': [
      'chdir.c',
      'clock.c',
      'clock_getres.c',
      'clock_gettime.c',
      'close.c',
      'dup.c',
      '_exit.c',
      'fstat.c',
      'getcwd.c',
      'getdents.c',
      'getpagesize.c',
      'getpid.c',
      'gettimeofday.c',
      'lock.c',
      'lseek.c',
      'malloc.c',
      'mkdir.c',
      'mmap.c',
      'mprotect.c',
      'munmap.c',
      'nanosleep.c',
      'nacl_irt_filename.c',
      'nacl_interface_query.c',
      'nacl_read_tp.c',
      'nacl_add_tp.c',
      'open.c',
      'read.c',
      'rmdir.c',
      'pthread_initialize_minimal.c',
      'pthread_stubs.c',
      'sbrk.c',
      'sched_yield.c',
      'stacktrace.c',
      'start.c',
      'stat.c',
      'sysconf.c',
      'tls.c',
      'unlink.c',
      'write.c',
    ],
    'sources_for_stubs': [
      'stubs/access.c',
      'stubs/chmod.c',
      'stubs/chown.c',
      'stubs/endpwent.c',
      'stubs/environ.c',
      'stubs/_execve.c',
      'stubs/fcntl.c',
      'stubs/fchdir.c',
      'stubs/fchmod.c',
      'stubs/fchown.c',
      'stubs/fdatasync.c',
      'stubs/fork.c',
      'stubs/fsync.c',
      'stubs/ftruncate.c',
      'stubs/get_current_dir_name.c',
      'stubs/getegid.c',
      'stubs/geteuid.c',
      'stubs/getgid.c',
      'stubs/getlogin.c',
      'stubs/getrusage.c',
      'stubs/getppid.c',
      'stubs/getpwent.c',
      'stubs/getpwnam.c',
      'stubs/getpwnam_r.c',
      'stubs/getpwuid.c',
      'stubs/getpwuid_r.c',
      'stubs/getuid.c',
      'stubs/getwd.c',
      'stubs/ioctl.c',
      'stubs/issetugid.c',
      'stubs/kill.c',
      'stubs/lchown.c',
      'stubs/link.c',
      'stubs/llseek.c',
      'stubs/lstat.c',
      'stubs/pipe.c',
      'stubs/pselect.c',
      'stubs/readlink.c',
      'stubs/select.c',
      'stubs/setegid.c',
      'stubs/seteuid.c',
      'stubs/setgid.c',
      'stubs/setpwent.c',
      'stubs/settimeofday.c',
      'stubs/setuid.c',
      'stubs/signal.c',
      'stubs/sigprocmask.c',
      'stubs/symlink.c',
      'stubs/times.c',
      'stubs/truncate.c',
      'stubs/ttyname.c',
      'stubs/ttyname_r.c',
      'stubs/umask.c',
      'stubs/utime.c',
      'stubs/utimes.c',
      'stubs/vfork.c',
      'stubs/wait.c',
      'stubs/waitpid.c',
    ],
    'sources_for_nacl_extensions': [
      'gc_hooks.c',
      'nacl_irt.c',
      'nacl_tls_get.c',
      'nacl_tls_init.c',
    ],
    'imc_syscalls': [
      'imc_accept.c',
      'imc_connect.c',
      'imc_makeboundsock.c',
      'imc_mem_obj_create.c',
      'imc_recvmsg.c',
      'imc_sendmsg.c',
      'imc_socketpair.c',
      'nameservice.c',
    ],
  },

  'targets' : [
    {
      'target_name': 'nacl_lib',
      'type': 'none',
      'dependencies': [
        'nacl_lib_newlib',
      ],
      'conditions': [
        # NOTE: We do not support glibc on arm yet.
        ['target_arch!="arm"', {
           'dependencies': [
             'nacl_lib_glibc'
           ]
         }],
      ],
    },

    {
      'target_name': 'nacl_lib_glibc',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl.a',
        'build_glibc': 1,
        'build_newlib': 0,
      },
      'sources': ['<@(sources_for_nacl_extensions)'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_lib_newlib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_irt': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': [
        '<@(sources_for_nacl_extensions)',
        '<@(sources_for_standard_interfaces)',
        '<@(sources_for_stubs)',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'variables': {
            'native_sources': [
              'aeabi_read_tp.S'
            ]
          }
        }],
      ],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_dynacode_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_dyncode.a',
        'nso_target': 'libnacl_dyncode.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': ['dyncode.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_dyncode_private_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_dyncode_private.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': ['dyncode_private.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_exception_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_exception.a',
        'nso_target': 'libnacl_exception.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': ['nacl_exception.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_exception_private_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_exception_private.a',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': ['nacl_exception_private.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_list_mappings_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_list_mappings.a',
        'nso_target': 'libnacl_list_mappings.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
      },
      'sources': ['list_mappings.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_list_mappings_private_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_list_mappings_private.a',
        'build_glibc': 0,
        'build_newlib': 1,
      },
      'sources': ['list_mappings_private.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'imc_syscalls_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libimc_syscalls.a',
        'nso_target': 'libimc_syscalls.so',
        'build_glibc': 1,
        'build_newlib': 1,
        'build_irt': 1,
      },
      'sources': ['<@(imc_syscalls)'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}
