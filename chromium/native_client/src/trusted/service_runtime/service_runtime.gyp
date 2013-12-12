# -*- gyp -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {
        'syscall_handler': [
          'posix/nacl_syscall_impl.c'
        ],
      }],
      ['OS=="mac"', {
        'syscall_handler': [
          'posix/nacl_syscall_impl.c'
        ],
      }],
      ['OS=="win"', {
        'syscall_handler': [
          'win/nacl_syscall_impl.c'
        ],
        'msvs_cygwin_shell': 0,
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="sel"', {
        'sources': [
          'dyn_array.c',
          'elf_util.c',
          'load_file.c',
          'nacl_all_modules.c',
          'nacl_app_thread.c',
          'nacl_bootstrap_channel_error_reporter.c',
          'nacl_copy.c',
          'nacl_desc_effector_ldr.c',
          'nacl_desc_postmessage.c',
          'nacl_error_gio.c',
          'nacl_error_log_hook.c',
          'nacl_globals.c',
          'nacl_kernel_service.c',
          'nacl_resource.c',
          'nacl_reverse_host_interface.c',
          'nacl_reverse_quota_interface.c',
          'nacl_runtime_host_interface.c',
          'nacl_secure_service.c',
          'nacl_signal_common.c',
          'nacl_stack_safety.c',
          'nacl_syscall_common.c',
          'nacl_syscall_hook.c',
          'nacl_text.c',
          'nacl_valgrind_hooks.c',
          'name_service/default_name_service.c',
          'name_service/name_service.c',
          'sel_addrspace.c',
          'sel_ldr.c',
          'sel_ldr_standard.c',
          'sel_ldr_thread_interface.c',
          # TODO(mseaborn): Move sel_main_chrome.c to the
          # "sel_main_chrome" library once Chromium is changed to
          # depend on that rather than on "sel".
          'sel_main_chrome.c',
          'sel_mem.c',
          'sel_qualify.c',
          'sel_validate_image.c',
          'sys_exception.c',
          'sys_fdio.c',
          'sys_filename.c',
          'sys_imc.c',
          'sys_list_mappings.c',
          'sys_memory.c',
          'thread_suspension_common.c',
          'thread_suspension_unwind.c',
        ],
        'include_dirs': [
          # For generated header files from the x86-64 validator,
          # e.g. nacl_disallows.h.
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
        'sources!': [
           '<(syscall_handler)',
        ],
        'actions': [
          {
            'action_name': 'nacl_syscall_handler',
            'inputs': [
              'nacl_syscall_handlers_gen.py',
              '<(syscall_handler)',
            ],
            'action':
              # TODO(gregoryd): find out how to generate a file
              # in such a location that can be found in both
              # NaCl and Chrome builds.
              ['<@(python_exe)', 'nacl_syscall_handlers_gen.py',
               '-i', '<@(syscall_handler)',
               '-o', '<@(_outputs)'],

            'msvs_cygwin_shell': 0,
            'msvs_quote_cmd': 0,
            'outputs': [
              '<(INTERMEDIATE_DIR)/nacl_syscall_handlers.c',
            ],
            'process_outputs_as_sources': 1,
            'message': 'Creating nacl_syscall_handlers.c',
          },
        ],
        'conditions': [
            ['OS=="mac"', {
              'sources': [
                'osx/crash_filter.c',
                'osx/mach_exception_handler.c',
                'osx/mach_thread_map.c',
                'osx/nacl_ldt.c',
                'osx/nacl_thread_nice.c',
                'osx/outer_sandbox.c',
                'osx/thread_suspension.c',
                'posix/addrspace_teardown.c',
                'posix/sel_memory.c',
                'posix/x86/sel_segments.c',
              ],
              'actions': [
                {
                  'action_name': 'mig_exc_generation',
                  'variables': {
                    'gen_dir': '<(INTERMEDIATE_DIR)/native_client/src/trusted/service_runtime',
                  },
                  'inputs': [
                    'osx/run_mig.py',
                    '$(SDKROOT)/usr/include/mach/exc.defs',
                  ],
                  'outputs': [
                    '<(gen_dir)/nacl_exc.h',
                    '<(gen_dir)/nacl_exc_server.c',
                  ],
                  'process_outputs_as_sources': 1,
                  'action': [
                    '<(python_exe)', '<@(_inputs)', '<@(_outputs)',
                  ],
                  'message': 'Generating mig plumbing for exc.defs',
                },
              ],
              'include_dirs': [
                '<(INTERMEDIATE_DIR)',
              ],
            }],
            ['OS=="win"', {
              'sources': [
                'win/addrspace_teardown.c',
                'win/debug_exception_handler.c',
                'win/debug_exception_handler_standalone.c',
                'win/nacl_ldt.c',
                'win/nacl_thread_nice.c',
                'win/sel_memory.c',
                'win/sel_segments.c',
                'win/thread_handle_map.c',
              ],
            }],
            # TODO(gregoryd): move arm-specific stuff into a separate gyp file.
            ['target_arch=="arm"', {
              'sources': [
                'arch/arm/nacl_app.c',
                'arch/arm/nacl_switch_to_app_arm.c',
                'arch/arm/sel_rt.c',
                'arch/arm/nacl_tls.c',
                'arch/arm/sel_ldr_arm.c',
                'arch/arm/sel_addrspace_arm.c',
                'arch/arm/nacl_switch.S',
                'arch/arm/nacl_syscall.S',
                'arch/arm/tramp_arm.S',
                'linux/nacl_signal_arm.c',
              ],
            }],
            ['OS=="linux"', {
              'sources': [
                'linux/nacl_bootstrap_args.c',
                'linux/nacl_thread_nice.c',
                'linux/r_debug.c',
                'linux/reserved_at_zero.c',
                'linux/thread_suspension.c',
                'posix/addrspace_teardown.c',
                'posix/sel_memory.c',
              ],
              'conditions': [
                ['target_arch=="ia32" or target_arch=="x64"', {
                  'sources': [
                    'linux/x86/nacl_ldt.c',
                    'posix/x86/sel_segments.c',
                  ],
                }],
                ['target_arch=="arm"', {
                  'sources': [
                    'linux/arm/sel_segments.c',
                  ],
                }],
              ],
            }],
            ['OS=="linux" or OS=="mac" or OS=="FreeBSD"', {
              'sources': [
                'posix/nacl_signal_stack.c',
                'posix/sel_addrspace_posix.c',
               ],
            }],
            ['OS=="win"', {
              'sources': [
                'win/nacl_signal_stack.c',
                'win/sel_addrspace_win.c',
                'win/thread_suspension.c',
                'win/vm_hole.c',
              ],
            }],
            ['OS!="win"', {
              'sources': [
                'generic/vm_hole.c',
              ],
            }],
          ],
        }],
      ],
   },
  'targets': [
    {
      'target_name': 'sel',
      'type': 'static_library',
      'variables': {
        'target_base': 'sel',
      },
      'dependencies': [
        'env_cleanser',
        'nacl_error_code',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/debug_stub/debug_stub.gyp:debug_stub',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/fault_injection/fault_injection.gyp:nacl_fault_inject',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
        '<(DEPTH)/native_client/src/trusted/interval_multiset/interval_multiset.gyp:nacl_interval',
        '<(DEPTH)/native_client/src/trusted/perf_counter/perf_counter.gyp:nacl_perf_counter',
        '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
        '<(DEPTH)/native_client/src/trusted/manifest_name_service_proxy/manifest_name_service_proxy.gyp:manifest_proxy',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface',
        '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache',
        '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validators',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator_arm/validator_arm.gyp:ncvalidate_arm_v2',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
            'arch/x86/service_runtime_x86.gyp:service_runtime_x86_common',
          ],
        }],
        ['target_arch == "ia32"', {
          'dependencies': [
            'arch/x86_32/service_runtime_x86_32.gyp:service_runtime_x86_32',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nccopy_x86_32',
          ],
        }],
        ['target_arch == "x64"', {
          'dependencies': [
            'arch/x86_64/service_runtime_x86_64.gyp:service_runtime_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nccopy_x86_64',
          ],
        }],
        ['OS=="linux" or OS=="FreeBSD"', {
          'dependencies': [
            'nacl_signal',
          ],
        }],
      ],
    }, {
      'target_name': 'nacl_error_code',
      'type': 'static_library',
      'sources': [
        'nacl_error_code.c',
      ],
    }, {
      'target_name': 'env_cleanser',
      'type': 'static_library',
      'sources': [
        'env_cleanser.c',
      ],
    }, {
      'target_name': 'sel_main_chrome',
      'type': 'none',
      'dependencies': [
        'sel',
      ],
    }, {
      'target_name': 'sel_main',
      'type': 'static_library',
      'sources': [
        'sel_main.c',
      ],
      'dependencies': [
        'sel',
      ],
    }, {
      'target_name': 'sel_ldr',
      'type': 'executable',
      'dependencies': [
        'sel_main',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
      ],
      'sources': [
        'nacl_test_injection_main.c',
        'force_cpp.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            'linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
          ],
          'ldflags': [
            '-pie',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'sel64',
          'type': 'static_library',
          'variables': {
            'target_base': 'sel',
            'win_target': 'x64',
          },
          'dependencies': [
            'env_cleanser64',
            'nacl_error_code64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/debug_stub/debug_stub.gyp:debug_stub64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/fault_injection/fault_injection.gyp:nacl_fault_inject64',
            '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc64',
            '<(DEPTH)/native_client/src/trusted/interval_multiset/interval_multiset.gyp:nacl_interval64',
            '<(DEPTH)/native_client/src/trusted/perf_counter/perf_counter.gyp:nacl_perf_counter64',
            '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib64',
            '<(DEPTH)/native_client/src/trusted/manifest_name_service_proxy/manifest_name_service_proxy.gyp:manifest_proxy64',
            '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service64',
            '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface64',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:nccopy_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validation_cache64',
            '<(DEPTH)/native_client/src/trusted/validator/validator.gyp:validators64',
            'arch/x86/service_runtime_x86.gyp:service_runtime_x86_common64',
            'arch/x86_64/service_runtime_x86_64.gyp:service_runtime_x86_64',
          ],
        }, {
          'target_name': 'nacl_error_code64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'nacl_error_code.c',
          ],
        },
        {
          'target_name': 'env_cleanser64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'env_cleanser.c',
          ],
        },
        {
          'target_name': 'sel_main_chrome64',
          'type': 'none',
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            'sel64',
          ],
        },
        {
          'target_name': 'sel_main64',
          'type': 'static_library',
          'sources': [
            'sel_main.c',
          ],
          'dependencies': [
            'sel64',
          ],
          'variables': {
            'win_target': 'x64',
          },
        },
        {
          'target_name': 'sel_ldr64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            'sel_main64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc64',
          ],
          'sources': [
            'nacl_test_injection_main.c',
          ],
        },
      ],
    }],
    ['OS=="linux" or OS=="FreeBSD"', {
      'targets': [
        {
          # This has to be an independent target in order to benefit from
          # specific flags.
          'target_name': 'nacl_signal',
          'type': 'static_library',
          'conditions': [
            ['target_arch=="ia32"', {
              # nacl_signal.c needs to be compiled without the stack
              # protector on i386.
              # See https://code.google.com/p/nativeclient/issues/detail?id=3581.
              'cflags!': [
                '-fstack-protector',
                '-fstack-protector-all',
              ],
              'cflags': [
                '-fno-stack-protector',
              ],
            }],
          ],
          'sources': [
            'linux/nacl_signal.c',
          ],
        },
      ],
    }],
  ],
}
