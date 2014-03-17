#  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

{
  'variables' : {
    # Override this value to build with small float FFT tables
    'big_float_fft%' : 1,
  },
  'targets': [
    {
      'target_name': 'openmax_dl',
      'type': 'static_library',
      'include_dirs': [
        '../',
      ],
      'sources': [
        'api/omxtypes.h',
        'sp/api/omxSP.h',
        'sp/src/armSP_FFT_F32TwiddleTable.c',
      ],
      'conditions' : [
        ['big_float_fft == 1', {
          'defines': [
            'BIG_FFT_TABLE',
          ],
        }],
        ['target_arch=="arm"', {
          'cflags!': [
            '-mfpu=vfpv3-d16',
          ],
          'cflags': [
            # We enable Neon instructions even with arm_neon==0, to support
            # runtime detection.
            '-mfpu=neon',
          ],
          'dependencies': [
            '<(android_ndk_root)/android_tools_ndk.gyp:cpu_features',
            'openmax_dl_armv7',
          ],
          'link_settings' : {
            'libraries': [
              # To get the __android_log_print routine
              '-llog',
            ],
          },
          'sources': [
            # Common files that are used by both the NEON and non-NEON code.
            'api/armCOMM_s.h',
            'api/armOMX.h',
            'api/omxtypes_s.h',
            'sp/api/armSP.h',
            'sp/src/arm/armSP_FFT_S32TwiddleTable.c',
            'sp/src/arm/detect.c',
            'sp/src/arm/omxSP_FFTGetBufSize_C_FC32.c',
            'sp/src/arm/omxSP_FFTGetBufSize_C_SC16.c',
            'sp/src/arm/omxSP_FFTGetBufSize_C_SC32.c',
            'sp/src/arm/omxSP_FFTGetBufSize_R_F32.c',
            'sp/src/arm/omxSP_FFTGetBufSize_R_S16.c',
            'sp/src/arm/omxSP_FFTGetBufSize_R_S16S32.c',
            'sp/src/arm/omxSP_FFTGetBufSize_R_S32.c',
            'sp/src/arm/omxSP_FFTInit_C_FC32.c',
            'sp/src/arm/omxSP_FFTInit_C_SC16.c',
            'sp/src/arm/omxSP_FFTInit_C_SC32.c',
            'sp/src/arm/omxSP_FFTInit_R_F32.c',
            'sp/src/arm/omxSP_FFTInit_R_S16.c',
            'sp/src/arm/omxSP_FFTInit_R_S16S32.c',
            'sp/src/arm/omxSP_FFTInit_R_S32.c',

            # Complex 32-bit fixed-point FFT.
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix2_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix2_ls_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix2_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix4_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix4_ls_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix2_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix4_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC32_Radix8_fs_unsafe_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CToC_SC32_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTFwd_CToC_SC32_Sfs_s.S',
            # Real 32-bit fixed-point FFT
            'sp/src/arm/neon/armSP_FFTInv_CCSToR_S32_preTwiddleRadix2_unsafe_s.S',
            'sp/src/arm/neon/omxSP_FFTFwd_RToCCS_S32_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CCSToR_S32_Sfs_s.S',
            # Complex 16-bit fixed-point FFT
            'sp/src/arm/neon/armSP_FFTInv_CCSToR_S16_preTwiddleRadix2_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix2_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix2_ls_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix2_ps_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix2_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix4_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix4_ls_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix4_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_SC16_Radix8_fs_unsafe_s.S',
            'sp/src/arm/neon/omxSP_FFTFwd_CToC_SC16_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CToC_SC16_Sfs_s.S',
            # Real 16-bit fixed-point FFT
            'sp/src/arm/neon/omxSP_FFTFwd_RToCCS_S16_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CCSToR_S16_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTFwd_RToCCS_S16S32_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CCSToR_S32S16_Sfs_s.S',
            # Complex floating-point FFT
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix2_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix2_ls_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix2_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix4_fs_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix4_ls_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix2_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix4_unsafe_s.S',
            'sp/src/arm/neon/armSP_FFT_CToC_FC32_Radix8_fs_unsafe_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CToC_FC32_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTFwd_CToC_FC32_Sfs_s.S',
            # Real floating-point FFT
            'sp/src/arm/neon/armSP_FFTInv_CCSToR_F32_preTwiddleRadix2_unsafe_s.S',
            'sp/src/arm/neon/omxSP_FFTFwd_RToCCS_F32_Sfs_s.S',
            'sp/src/arm/neon/omxSP_FFTInv_CCSToR_F32_Sfs_s.S',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'cflags': [
            '-msse2',
          ],
          'sources': [
            # Real 32-bit floating-point FFT.
            'sp/api/x86SP.h',
            'sp/src/x86/omxSP_FFTFwd_RToCCS_F32_Sfs.c',
            'sp/src/x86/omxSP_FFTGetBufSize_R_F32.c',
            'sp/src/x86/omxSP_FFTInit_R_F32.c',
            'sp/src/x86/omxSP_FFTInv_CCSToR_F32_Sfs.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix2_fs.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix2_ls.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix2_ls_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix2_ms.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix4_fs.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix4_fs_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix4_ls.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix4_ls_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix4_ms.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Fwd_Radix4_ms_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix2_fs.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix2_ls.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix2_ls_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix2_ms.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix4_fs.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix4_fs_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix4_ls.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix4_ls_sse.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix4_ms.c',
            'sp/src/x86/x86SP_FFT_CToC_FC32_Inv_Radix4_ms_sse.c',
            'sp/src/x86/x86SP_FFT_F32_radix2_kernel.c',
            'sp/src/x86/x86SP_FFT_F32_radix4_kernel.c',
            'sp/src/x86/x86SP_SSE_Math.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_arch=="arm"', {
      'targets': [
        {
          # Non-NEON implementation of FFT. This library is NOT
          # standalone. Applications must link with openmax_dl.
          'target_name': 'openmax_dl_armv7',
          'type': 'static_library',
          'include_dirs': [
            '../',
          ],
          'cflags!': [
            '-mfpu=neon',
          ],
          'sources': [
            # Complex floating-point FFT
            'sp/src/arm/armv7/armSP_FFT_CToC_FC32_Radix2_fs_unsafe_s.S',
            'sp/src/arm/armv7/armSP_FFT_CToC_FC32_Radix2_fs_unsafe_s.S',
            'sp/src/arm/armv7/armSP_FFT_CToC_FC32_Radix4_fs_unsafe_s.S',
            'sp/src/arm/armv7/armSP_FFT_CToC_FC32_Radix4_unsafe_s.S',
            'sp/src/arm/armv7/armSP_FFT_CToC_FC32_Radix8_fs_unsafe_s.S',
            'sp/src/arm/armv7/omxSP_FFTInv_CToC_FC32_Sfs_s.S',
            'sp/src/arm/armv7/omxSP_FFTFwd_CToC_FC32_Sfs_s.S',
            # Real floating-point FFT
            'sp/src/arm/armv7/armSP_FFTInv_CCSToR_F32_preTwiddleRadix2_unsafe_s.S',
            'sp/src/arm/armv7/omxSP_FFTFwd_RToCCS_F32_Sfs_s.S',
            'sp/src/arm/armv7/omxSP_FFTInv_CCSToR_F32_Sfs_s.S',
          ],
        },
      ],
    }],
  ],
}
