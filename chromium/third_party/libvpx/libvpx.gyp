# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'libvpx_build_vp9%': 1,
    'libvpx_source%': 'source/libvpx',
    'conditions': [
      ['os_posix==1', {
        'asm_obj_extension': 'o',
      }],
      ['OS=="win"', {
        'asm_obj_extension': 'obj',
      }],

      ['(target_arch=="arm" or target_arch=="armv7") and arm_neon==1', {
        'target_arch_full': 'arm-neon',
      }, {
        'conditions': [
          ['OS=="android"', {
            'target_arch_full': 'arm-neon-cpu-detect',
          }, {
           'target_arch_full': '<(target_arch)',
          }],
        ],
      }],

      ['os_posix == 1 and OS != "mac"', {
        'OS_CATEGORY%': 'linux',
      }, {
        'OS_CATEGORY%': '<(OS)',
      }],
    ],

    # Location of the intermediate output.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
  },
  'target_defaults': {
    'target_conditions': [
      ['<(libvpx_build_vp9)==0', {
        'sources/': [ ['exclude', '(^|/)vp9/'], ],
      }],
    ],
  },
  'conditions': [
    ['target_arch=="ia32"', {
      'includes': ['libvpx_srcs_x86_intrinsics.gypi', ],
    }],
    ['target_arch=="x64"', {
      'includes': ['libvpx_srcs_x86_64_intrinsics.gypi', ],
    }],
    [ '(target_arch != "arm" and target_arch != "armv7") and target_arch != "mipsel"', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'variables': {
            'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
            'OS_CATEGORY%': '<(OS_CATEGORY)',
            'yasm_flags': [
              '-D', 'CHROMIUM',
              '-I', 'source/config/<(OS_CATEGORY)/<(target_arch)',
              '-I', 'source/config',
              '-I', '<(libvpx_source)',
              '-I', '<(shared_generated_dir)', # Generated assembly offsets
            ],
          },
          'dependencies': [
            'gen_asm_offsets_vp8',
          ],
          'includes': [
            '../yasm/yasm_compile.gypi'
          ],
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch)',
            'source/config',
            '<(libvpx_source)',
            '<(libvpx_source)/vp8/common',
            '<(libvpx_source)/vp8/decoder',
            '<(libvpx_source)/vp8/encoder',
            '<(shared_generated_dir)', # Provides vpx_rtcd.h.
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_source)',
            ],
          },
          # VS2010 does not correctly incrementally link obj files generated
          # from asm files. This flag disables UseLibraryDependencyInputs to
          # avoid this problem.
          'msvs_2010_disable_uldi_when_referenced': 1,
          'conditions': [
            ['target_arch=="ia32"', {
              'includes': [
                'libvpx_srcs_x86.gypi',
              ],
              'dependencies': [
                'libvpx_intrinsics_mmx',
                'libvpx_intrinsics_sse2',
                # Currently no sse3 intrinsic functions
                #'libvpx_intrinsics_sse3',
                'libvpx_intrinsics_ssse3',
                # Currently no sse4_1 intrinsic functions
                #'libvpx_intrinsics_sse4_1',
              ],
            }],
            ['target_arch=="x64"', {
              'includes': [
                'libvpx_srcs_x86_64.gypi',
              ],
              'dependencies': [
                'libvpx_intrinsics_mmx',
                'libvpx_intrinsics_sse2',
                # Currently no sse3 intrinsic functions
                #'libvpx_intrinsics_sse3',
                'libvpx_intrinsics_ssse3',
                # Currently no sse4_1 intrinsic functions
                #'libvpx_intrinsics_sse4_1',
              ],
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # libvpx heavily relies on implicit enum casting.
                  '-Wno-conversion',
                  # libvpx does `if ((a == b))` in some places.
                  '-Wno-parentheses-equality',
                ],
              },
              'cflags': [
                '-Wno-conversion',
                '-Wno-parentheses-equality',
              ],
            }],
            ['chromeos == 1', {
              # ChromeOS needs these files for animated WebM avatars.
              'sources': [
                '<(libvpx_source)/libmkv/EbmlIDs.h',
                '<(libvpx_source)/libmkv/EbmlWriter.c',
                '<(libvpx_source)/libmkv/EbmlWriter.h',
              ],
            }],
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for mips builds.
    [ 'target_arch=="mipsel" ', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'variables': {
            'shared_generated_dir':
              '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx',
          },
          'includes': [
            'libvpx_srcs_mips.gypi',
          ],
          'cflags': [
            '-EL -static -mips32',
          ],
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch)',
            'source/config',
            '<(libvpx_source)',
            '<(libvpx_source)/vp8/common',
            '<(libvpx_source)/vp8/decoder',
            '<(libvpx_source)/vp8/encoder',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_source)',
            ],
          },
          'sources': [
            'source/config/<(OS_CATEGORY)/<(target_arch)/vpx_config.c',
          ],
        },
      ],
    },
    ],
    # 'libvpx' target for ARM builds.
    [ '(target_arch=="arm" or target_arch=="armv7") ', {
      'targets': [
        {
          # This libvpx target contains both encoder and decoder.
          # Encoder is configured to be realtime only.
          'target_name': 'libvpx',
          'type': 'static_library',
          'dependencies': [
            'gen_asm_offsets_vp8',
            'gen_asm_offsets_vpx_scale',
          ],

          # Copy the script to the output folder so that we can use it with
          # absolute path.
          'copies': [{
            'destination': '<(shared_generated_dir)',
            'files': [
              '<(ads2gas_script_path)',
              '<(ads2gas_script_include)',
            ],
          }],

          # Rule to convert .asm files to .S files.
          'rules': [
            {
              'rule_name': 'convert_asm',
              'extension': 'asm',
              'inputs': [
                '<(shared_generated_dir)/<(ads2gas_script)',
                '<(shared_generated_dir)/thumb.pm',
              ],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
              ],
              'action': [
                'bash',
                '-c',
                'cat <(RULE_INPUT_PATH) | perl <(shared_generated_dir)/<(ads2gas_script) > <(shared_generated_dir)/<(RULE_INPUT_ROOT).S',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Convert libvpx asm file for ARM <(RULE_INPUT_PATH).',
            },
          ],

          'variables': {
            'variables': {
              'conditions': [
                ['OS=="ios"', {
                  'ads2gas_script%': 'ads2gas_apple.pl',
                }, {
                  'ads2gas_script%': 'ads2gas.pl',
                }],
              ],
            },
            'ads2gas_script%': '<(ads2gas_script)',
            # Location of the assembly conversion script.
            'ads2gas_script_path': '<(libvpx_source)/build/make/<(ads2gas_script)',
            'ads2gas_script_include': '<(libvpx_source)/build/make/thumb.pm',
            'assembler_include_flags': [
              # We need to explicitly tell the GCC assembler to look for
              # .include directive files from the place where they're
              # generated to.
              '-Wa,-I,<!(pwd)/source/config/<(OS_CATEGORY)/<(target_arch_full)',
              '-Wa,-I,<!(pwd)/source/config',
              '-Wa,-I,<(shared_generated_dir)',
            ],
          },
          'cflags': [
            '<@(assembler_include_flags)',
          ],
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-no-integrated-as',
              '<@(assembler_include_flags)',
            ],
          },
          'include_dirs': [
            'source/config/<(OS_CATEGORY)/<(target_arch_full)',
            'source/config',
            '<(libvpx_source)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(libvpx_source)',
            ],
          },
          'conditions': [
            # Libvpx optimizations for ARMv6 or ARMv7 without NEON.
            ['arm_neon==0', {
              'conditions': [
                ['OS=="android"', {
                  'includes': [
                    'libvpx_srcs_arm_neon_cpu_detect.gypi',
                  ],
                  'cflags': [
                    '-Wa,-mfpu=neon',
                  ],
                }, {
                  'includes': [
                    'libvpx_srcs_arm.gypi',
                  ],
                }],
              ],
            }],
            # Libvpx optimizations for ARMv7 with NEON.
            ['arm_neon==1', {
              'includes': [
                'libvpx_srcs_arm_neon.gypi',
              ],
            }],
            ['OS == "android"', {
              'include_dirs': [
                '<(android_ndk_include)',
                '<(android_ndk_root)/sources/android/cpufeatures',
              ],
            }],
            ['chromeos == 1', {
              # ChromeOS needs these files for animated WebM avatars.
              'sources': [
                '<(libvpx_source)/libmkv/EbmlIDs.h',
                '<(libvpx_source)/libmkv/EbmlWriter.c',
                '<(libvpx_source)/libmkv/EbmlWriter.h',
              ],
            }],
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      # A tool that runs on host to extract integers from object file.
      'target_name': 'libvpx_obj_int_extract',
      'type': 'executable',
      'toolsets': ['host'],
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        'source/config',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/build/make/obj_int_extract.c',
      ]
    },
    {
      # A library that contains assembly offsets needed.
      'target_name': 'libvpx_asm_offsets_vp8',
      'type': 'static_library',
      'hard_dependency': 1,
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        'source/config',
        '<(libvpx_source)',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [ '-fsanitize=address' ],
          'xcode_settings': { 'OTHER_CFLAGS!': [ '-fsanitize=address' ] },
          'ldflags!': [ '-fsanitize=address' ],
        }],
      ],
      'sources': [
        '<(libvpx_source)/vp8/encoder/vp8_asm_enc_offsets.c',
      ],
    },
    {
      # A library that contains assembly offsets needed.
      # TODO(fgalligan): Merge libvpx_asm_offsets_vpx_scale into
      # libvpx_asm_offsets.
      'target_name': 'libvpx_asm_offsets_vpx_scale',
      'type': 'static_library',
      'hard_dependency': 1,
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        'source/config',
        '<(libvpx_source)',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [ '-faddress-sanitizer', '-fsanitize=address', ],
          'xcode_settings': {
            'OTHER_CFLAGS!': [ '-faddress-sanitizer','-fsanitize=address' ],
          },
          'ldflags!': [ '-faddress-sanitizer', '-fsanitize=address', ],
        }],
      ],
      'sources': [
        '<(libvpx_source)/vpx_scale/vpx_scale_asm_offsets.c',
      ],
    },
    {
      # A target that takes assembly offsets library and generate the
      # corresponding assembly files.
      # This target is a hard dependency because the generated .asm files
      # are needed all assembly optimized files in libvpx.
      'target_name': 'gen_asm_offsets_vp8',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'libvpx_asm_offsets_vp8',
        'libvpx_obj_int_extract#host',
      ],
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'ninja_obj_dir': '<(PRODUCT_DIR)/obj/third_party/libvpx/<(libvpx_source)/vp8',
          },
          'actions': [
            {
              'action_name': 'copy_enc_offsets_obj',
              'inputs': [ 'copy_obj.sh' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.obj' ],
              'action': [
                '<(DEPTH)/third_party/libvpx/copy_obj.sh',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets_vp8/vp8_asm_enc_offsets.obj',
                '-s', '<(ninja_obj_dir)/encoder/libvpx_asm_offsets_vp8.vp8_asm_enc_offsets.obj',
                '-s', '<(PRODUCT_DIR)/obj/Source/WebKit/chromium/third_party/libvpx/<(libvpx_source)/vp8/encoder/libvpx_asm_offsets_vp8.vp8_asm_enc_offsets.obj',
              ],
              'process_output_as_sources': 1,
              'msvs_cygwin_shell': 1,
            },
          ],
          'sources': [
            '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.obj',
          ],
        }, {
          'actions': [
            {
              # Take archived .a file and unpack it unto .o files.
              'action_name': 'unpack_lib_posix',
              'inputs': [
                'unpack_lib_posix.sh',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.o',
              ],
              'action': [
                '<(DEPTH)/third_party/libvpx/unpack_lib_posix.sh',
                '-d', '<(INTERMEDIATE_DIR)',
                '-a', '<(PRODUCT_DIR)/libvpx_asm_offsets_vp8.a',
                '-a', '<(LIB_DIR)/third_party/libvpx/libvpx_asm_offsets_vp8.a',
                '-a', '<(LIB_DIR)/Source/WebKit/chromium/third_party/libvpx/libvpx_asm_offsets_vp8.a',
                '-f', 'vp8_asm_enc_offsets.o',
              ],
              'process_output_as_sources': 1,
              'msvs_cygwin_shell': 1,
            },
          ],
          # Need this otherwise gyp won't run the rule on them.
          'sources': [
            '<(INTERMEDIATE_DIR)/vp8_asm_enc_offsets.o',
          ],
        }],
      ],
      'rules': [
        {
          # Rule to extract integer values for each symbol from an object file.
          'rule_name': 'obj_int_extract',
          'extension': '<(asm_obj_extension)',
          'inputs': [
            '<(PRODUCT_DIR)/libvpx_obj_int_extract',
            'obj_int_extract.sh',
          ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).asm',
          ],
          'variables': {
            'conditions': [
              ['(target_arch=="arm" or target_arch=="armv7")', {
                'asm_format': 'gas',
              }, {
                'asm_format': 'rvds',
              }],
            ],
          },
          'action': [
            '<(DEPTH)/third_party/libvpx/obj_int_extract.sh',
            '-e', '<(PRODUCT_DIR)/libvpx_obj_int_extract',
            '-f', '<(asm_format)',
            '-b', '<(RULE_INPUT_PATH)',
            '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).asm',
          ],
          'message': 'Generate assembly offsets <(RULE_INPUT_PATH).',
          'msvs_cygwin_shell': 1,
        },
      ],
    },
    {
      # A target that takes assembly offsets library and generate the
      # corresponding assembly files.
      # This target is a hard dependency because the generated .asm files
      # are needed all assembly optimized files in libvpx.
      'target_name': 'gen_asm_offsets_vpx_scale',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'libvpx_asm_offsets_vpx_scale',
        'libvpx_obj_int_extract#host',
      ],
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'ninja_obj_dir': '<(PRODUCT_DIR)/obj/third_party/libvpx/<(libvpx_source)/vpx_scale',
          },
          'actions': [
            {
              'action_name': 'copy_enc_offsets_obj',
              'inputs': [ 'copy_obj.sh' ],
              'outputs': [ '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.obj' ],
              'action': [
                '<(DEPTH)/third_party/libvpx/copy_obj.sh',
                '-d', '<@(_outputs)',
                '-s', '<(PRODUCT_DIR)/obj/libvpx_asm_offsets_vpx_scale/vpx_scale_asm_offsets.obj',
                '-s', '<(ninja_obj_dir)/encoder/libvpx_asm_offsets_vpx_scale.vpx_scale_asm_offsets.obj',
                '-s', '<(PRODUCT_DIR)/obj/Source/WebKit/chromium/third_party/libvpx/<(libvpx_source)/vpx_scale/libvpx_asm_offsets_vpx_scale.vpx_scale_asm_offsets.obj',
              ],
              'process_output_as_sources': 1,
              'msvs_cygwin_shell': 1,
            },
          ],
          'sources': [
            '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.obj',
          ],
        }, {
          'actions': [
            {
              # Take archived .a file and unpack it unto .o files.
              'action_name': 'unpack_lib_posix',
              'inputs': [
                'unpack_lib_posix.sh',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.o',
              ],
              'action': [
                '<(DEPTH)/third_party/libvpx/unpack_lib_posix.sh',
                '-d', '<(INTERMEDIATE_DIR)',
                '-a', '<(PRODUCT_DIR)/libvpx_asm_offsets_vpx_scale.a',
                '-a', '<(LIB_DIR)/third_party/libvpx/libvpx_asm_offsets_vpx_scale.a',
                '-a', '<(LIB_DIR)/Source/WebKit/chromium/third_party/libvpx/libvpx_asm_offsets_vpx_scale.a',
                '-f', 'vpx_scale_asm_offsets.o',
              ],
              'process_output_as_sources': 1,
            },
          ],
          # Need this otherwise gyp won't run the rule on them.
          'sources': [
            '<(INTERMEDIATE_DIR)/vpx_scale_asm_offsets.o',
          ],
        }],
      ],
      'rules': [
        {
          # Rule to extract integer values for each symbol from an object file.
          'rule_name': 'obj_int_extract',
          'extension': '<(asm_obj_extension)',
          'inputs': [
            '<(PRODUCT_DIR)/libvpx_obj_int_extract',
            'obj_int_extract.sh',
          ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).asm',
          ],
          'variables': {
            'conditions': [
              ['(target_arch=="arm" or target_arch=="armv7")', {
                'asm_format': 'gas',
              }, {
                'asm_format': 'rvds',
              }],
            ],
          },
          'action': [
            '<(DEPTH)/third_party/libvpx/obj_int_extract.sh',
            '-e', '<(PRODUCT_DIR)/libvpx_obj_int_extract',
            '-f', '<(asm_format)',
            '-b', '<(RULE_INPUT_PATH)',
            '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).asm',
          ],
          'message': 'Generate assembly offsets <(RULE_INPUT_PATH).',
          'msvs_cygwin_shell': 1,
        },
      ],
    },
    {
      'target_name': 'simple_encoder',
      'type': 'executable',
      'dependencies': [
        'libvpx',
      ],

      # Copy the script to the output folder so that we can use it with
      # absolute path.
      'copies': [{
        'destination': '<(shared_generated_dir)/simple_encoder',
        'files': [
          '<(libvpx_source)/examples/gen_example_code.sh',
        ],
      }],

      # Rule to convert .txt files to .c files.
      'rules': [
        {
          'rule_name': 'generate_example',
          'extension': 'txt',
          'inputs': [ '<(shared_generated_dir)/simple_encoder/gen_example_code.sh', ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'action': [
            'bash',
            '-c',
            '<(shared_generated_dir)/simple_encoder/gen_example_code.sh <(RULE_INPUT_PATH) > <(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generate libvpx example code <(RULE_INPUT_PATH).',
        },
      ],
      'sources': [
        '<(libvpx_source)/examples/simple_encoder.txt',
      ]
    },
    {
      'target_name': 'simple_decoder',
      'type': 'executable',
      'dependencies': [
        'libvpx',
      ],

      # Copy the script to the output folder so that we can use it with
      # absolute path.
      'copies': [{
        'destination': '<(shared_generated_dir)/simple_decoder',
        'files': [
          '<(libvpx_source)/examples/gen_example_code.sh',
        ],
      }],

      # Rule to convert .txt files to .c files.
      'rules': [
        {
          'rule_name': 'generate_example',
          'extension': 'txt',
          'inputs': [ '<(shared_generated_dir)/simple_decoder/gen_example_code.sh', ],
          'outputs': [
            '<(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'action': [
            'bash',
            '-c',
            '<(shared_generated_dir)/simple_decoder/gen_example_code.sh <(RULE_INPUT_PATH) > <(shared_generated_dir)/<(RULE_INPUT_ROOT).c',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generate libvpx example code <(RULE_INPUT_PATH).',
        },
      ],
      'sources': [
        '<(libvpx_source)/examples/simple_decoder.txt',
      ]
    },
  ],
}
