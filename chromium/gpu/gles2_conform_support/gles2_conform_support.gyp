# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # These are defined here because we want to be able to compile them on
    # the buildbots without needed the OpenGL ES 2.0 conformance tests
    # which are not open source.
    'bootstrap_sources_native': [
      'native/main.cc',
    ],
   'conditions': [
     ['OS=="linux"', {
       'bootstrap_sources_native': [
         'native/egl_native_aura.cc',
         'native/egl_native.cc',
         'native/egl_native_gtk.cc',
         'native/egl_native_x11.cc',
       ],
     }],
     ['OS=="win"', {
       'bootstrap_sources_native': [
         'native/egl_native.cc',
         'native/egl_native_win.cc',
       ],
     }],
   ],

  },
  'targets': [
    {
      'target_name': 'egl_native',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gpu/gpu.gyp:gpu',
        '../../gpu/gpu.gyp:gles2_implementation_client_side_arrays_no_check',
        '../../gpu/gpu.gyp:command_buffer_service',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gl/gl.gyp:gl',
        '../../ui/ui.gyp:ui',
      ],
      'sources': [
        'egl/config.cc',
        'egl/config.h',
        'egl/display.cc',
        'egl/display.h',
        'egl/egl.cc',
        'egl/surface.cc',
        'egl/surface.h',
      ],
      'defines': [
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'egl_main_native',
      'type': 'static_library',
      'dependencies': [
        'egl_native',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
      'sources': [
        '<@(bootstrap_sources_native)',
      ],
      'defines': [
        'GLES2_CONFORM_SUPPORT_ONLY',
        'GTF_GLES20',
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
    },
    {
      'target_name': 'egl_main_windowless',
      'type': 'static_library',
      'dependencies': [
        'egl_native',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
      ],
      'sources': [
        'native/main.cc',
        'native/egl_native.cc',
        'native/egl_native_windowless.cc',
        '<@(bootstrap_sources_native)',
      ],
      'defines': [
        'GLES2_CONFORM_SUPPORT_ONLY',
        'GTF_GLES20',
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
    },
    {
      'target_name': 'gles2_conform_support',
      'type': 'executable',
      'dependencies': [
        'egl_native',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../gpu/gpu.gyp:gles2_c_lib_nocheck',
        '../../third_party/expat/expat.gyp:expat',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': ['../../build/linux/system.gyp:gtk'],
        }],
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and linux_use_tcmalloc==1', {
          'dependencies': [
            '../../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
      'defines': [
        'GLES2_CONFORM_SUPPORT_ONLY',
        'GTF_GLES20',
        'EGLAPI=',
        'EGLAPIENTRY=',
      ],
      'sources': [
        '<@(bootstrap_sources_native)',
        'gles2_conform_support.c'
      ],
    },
  ],
}
