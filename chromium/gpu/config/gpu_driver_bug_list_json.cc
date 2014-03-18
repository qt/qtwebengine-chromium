// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Determines whether a certain driver bug exists in the current system.
// The format of a valid gpu_driver_bug_list.json file is defined in
// <gpu/config/gpu_control_list_format.txt>.
// The supported "features" can be found in
// <gpu/config/gpu_driver_bug_workaround_type.h>.

#include "gpu/config/gpu_control_list_jsons.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace gpu {

const char kGpuDriverBugListJson[] = LONG_STRING_CONST(

{
  "name": "gpu driver bug list",
  // Please update the version number whenever you change this file.
  "version": "3.10",
  "entries": [
    {
      "id": 1,
      "description": "Imagination driver doesn't like uploading lots of buffer data constantly",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "use_client_side_arrays_for_stream_buffers"
      ]
    },
    {
      "id": 2,
      "description": "ARM driver doesn't like uploading lots of buffer data constantly",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "features": [
        "use_client_side_arrays_for_stream_buffers"
      ]
    },
    {
      "id": 3,
      "features": [
        "set_texture_filter_before_generating_mipmap"
      ]
    },
    {
      "id": 4,
      "description": "Need to set the alpha to 255",
      "features": [
        "clear_alpha_in_readpixels"
      ]
    },
    {
      "id": 5,
      "vendor_id": "0x10de",
      "features": [
        "use_current_program_after_successful_link"
      ]
    },
    {
      "id": 6,
      "cr_bugs": [165493, 222018],
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "restore_scissor_on_fbo_change"
      ]
    },
    {
      "id": 7,
      "cr_bugs": [89557],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "features": [
        "needs_offscreen_buffer_workaround"
      ]
    },
    {
      "id": 8,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "features": [
        "needs_glsl_built_in_function_emulation"
      ]
    },
    {
      "id": 9,
      "description": "Mac AMD drivers get gl_PointCoord backward on OSX 10.8 or earlier",
      "cr_bugs": [256349],
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "value": "10.9"
        }
      },
      "vendor_id": "0x1002",
      "features": [
        "reverse_point_sprite_coord_origin"
      ]
    },
    {
      "id": 10,
      "description": "Mac Intel drivers get gl_PointCoord backward on OSX 10.8 or earlier",
      "cr_bugs": [256349],
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "value": "10.9"
        }
      },
      "vendor_id": "0x8086",
      "features": [
        "reverse_point_sprite_coord_origin"
      ]
    },
    {
      "id": 11,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "features": [
        "max_texture_size_limit_4096"
      ]
    },
    {
      "id": 12,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "features": [
        "max_cube_map_texture_size_limit_1024"
      ]
    },
    {
      "id": 13,
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "value": "10.7.3"
        }
      },
      "vendor_id": "0x8086",
      "features": [
        "max_cube_map_texture_size_limit_512"
      ]
    },
    {
      "id": 14,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "features": [
        "max_texture_size_limit_4096",
        "max_cube_map_texture_size_limit_4096"
      ]
    },
    {
      "id": 16,
      "description": "Intel drivers on Linux appear to be buggy",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x8086",
      "features": [
        "disable_ext_occlusion_query"
      ]
    },
    {
      "id": 17,
      "description": "Some drivers are unable to reset the D3D device in the GPU process sandbox",
      "os": {
        "type": "win"
      },
      "features": [
        "exit_on_context_lost"
      ]
    },
    {
      "id": 18,
      "description": "Everything except async + NPOT + multiple-of-8 textures are brutally slow for Imagination drivers",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "enable_chromium_fast_npot_mo8_textures"
      ]
    },
    {
      "id": 19,
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "disable_depth_texture"
      ]
    },
    {
      "id": 20,
      "description": "Disable EXT_draw_buffers on GeForce GT 650M on Mac OS X due to driver bugs.",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0fd5"],
      "multi_gpu_category": "any",
      "features": [
        "disable_ext_draw_buffers"
      ]
    },
    {
      "id": 21,
      "description": "Vivante GPUs are buggy with context switching.",
      "cr_bugs": [179250, 235935],
      "os": {
        "type": "android"
      },
      "gl_extensions": {
        "op": "contains",
        "value": "GL_VIV_shader_binary"
      },
      "features": [
        "unbind_fbo_on_context_switch"
      ]
    },
    {
      "id": 22,
      "description": "Imagination drivers are buggy with context switching.",
      "cr_bugs": [230896],
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "unbind_fbo_on_context_switch"
      ]
    },
    {
      "id": 23,
      "cr_bugs": [243038],
      "description": "Disable OES_standard_derivative on Intel Pineview M Gallium drivers.",
      "os": {
        "type": "chromeos"
      },
      "vendor_id": "0x8086",
      "device_id": ["0xa011", "0xa012"],
      "features": [
        "disable_oes_standard_derivatives"
      ]
    },
    {
      "id": 24,
      "cr_bugs": [231082],
      "description": "Mali-400 drivers throw an error when a buffer object's size is set to 0.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "gl_renderer": {
        "op": "contains",
        "value": "Mali-400"
      },
      "features": [
        "use_non_zero_size_for_client_side_stream_buffers"
      ]
    },
    {
      "id": 25,
      "cr_bugs": [152225],
      "description":
          "PBO + Readpixels + intel gpu doesn't work on OSX 10.7.",
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "value": "10.8"
        }
      },
      "vendor_id": "0x8086",
      "features": [
        "disable_async_readpixels"
      ]
    },
    {
      "id": 26,
      "description": "Disable use of Direct3D 11 on Windows Vista and lower.",
      "os": {
        "type": "win",
        "version": {
          "op": "<=",
          "value": "6.0"
        }
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 27,
      "cr_bugs": [265115],
      "description": "Async Readpixels with GL_BGRA format is broken on Haswell chipset on Mac.",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0402", "0x0406", "0x040a", "0x0412", "0x0416", "0x041a",
                    "0x0a04", "0x0a16", "0x0a22", "0x0a26", "0x0a2a"],
      "features": [
        "swizzle_rgba_for_async_readpixels"
      ]
    },
    {
      "id": 29,
      "cr_bugs": [278606],
      "description": "Testing fences is broken on QualComm.",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.4"
        }
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "disable_async_readpixels"
      ]
    },
    {
      "id": 30,
      "cr_bugs": [237931],
      "description": "Multisampling is buggy on OSX when multiple monitors are connected",
      "os": {
        "type": "macosx"
      },
      "features": [
        "disable_multimonitor_multisampling"
      ]
    },
    {
      "id": 31,
      "cr_bugs": [154715, 10068, 269829, 294779, 285292],
      "description": "The Mali T-6xx driver does not guarantee flush ordering.",
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "gl_renderer": {
        "op": "beginwith",
        "value": "Mali-T6"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 32,
      "cr_bugs": [179815],
      "description": "Share groups are not working on (older?) Broadcom drivers.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Broadcom"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 33,
      "description": "Share group-related crashes and poor context switching perf on Galaxy Nexus.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 34,
      "cr_bugs": [179250, 229643, 230896],
      "description": "Share groups are not working on (older?) Vivante drivers.",
      "os": {
        "type": "android"
      },
      "gl_extensions": {
        "op": "contains",
        "value": "GL_VIV_shader_binary"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 35,
      "cr_bugs": [163464],
      "description": "Share-group related crashes on older NVIDIA drivers.",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "NVIDIA"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 36,
      "cr_bugs": [163464, 233612],
      "description": "Share-group related crashes on Qualcomm drivers.",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.3"
        }
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 37,
      "cr_bugs": [286468],
      "description": "Program link fails in NVIDIA Linux if gl_Position is not set",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x10de",
      "features": [
        "init_gl_position_in_vertex_shader"
      ]
    },
    {
      "id": 38,
      "cr_bugs": [289461],
      "description": "Non-virtual contexts on Qualcomm sometimes cause out-of-order frames",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "use_virtualized_gl_contexts"
      ]
    },
    {
      "id": 39,
      "cr_bugs": [290391],
      "description": "Multisampled renderbuffer allocation must be validated on some Macs",
      "os": {
        "type": "macosx"
      },
      "features": [
        "validate_multisample_buffer_allocation"
      ]
    },
    {
      "id": 40,
      "cr_bugs": [290876],
      "description": "Framebuffer discarding causes flickering on old ARM drivers",
      "os": {
        "type": "android",
        "version": {
          "op": "<",
          "value": "4.4"
        }
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "features": [
        "disable_ext_discard_framebuffer"
      ]
    },
    {
      "id": 42,
      "cr_bugs": [290876],
      "description": "Framebuffer discarding causes flickering on older IMG drivers.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "gl_renderer": {
        "op": "=",
        "value": "PowerVR SGX 540"
      },
      "features": [
        "disable_ext_discard_framebuffer"
      ]
    },
    {
      "id": 43,
      "cr_bugs": [299494],
      "description": "Framebuffer discarding doesn't accept trivial attachments on Vivante.",
      "os": {
        "type": "android"
      },
      "gl_extensions": {
        "op": "contains",
        "value": "GL_VIV_shader_binary"
      },
      "features": [
        "disable_ext_discard_framebuffer"
      ]
    },
    {
      "id": 44,
      "cr_bugs": [301988],
      "description": "Framebuffer discarding causes jumpy scrolling on Mali drivers",
      "os": {
        "type": "chromeos"
      },
      "features": [
        "disable_ext_discard_framebuffer"
      ]
    },
    {
      "id": 45,
      "cr_bugs": [307751],
      "description": "Unfold short circuit on MacOSX.",
      "os": {
        "type": "macosx"
      },
      "features": [
        "unfold_short_circuit_as_ternary_operation"
      ]
    },
    {
      "id": 46,
      "description": "Using D3D11 causes browser crashes on certain Intel GPUs.",
      "cr_bugs": [310808],
      "os": {
        "type": "win"
      },
      "vendor_id": "0x8086",
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 48,
      "description": "Force to use discrete GPU on older MacBookPro models.",
      "cr_bugs": [113703],
      "os": {
        "type": "macosx",
        "version": {
          "op": ">=",
          "value": "10.7"
        }
      },
      "machine_model": {
        "name": {
          "op": "=",
          "value": "MacBookPro"
        },
        "version": {
          "op": "<",
          "value": "8"
        }
      },
      "gpu_count": {
        "op": "=",
        "value": "2"
      },
      "features": [
        "force_discrete_gpu"
      ]
    },
    {
      "id": 49,
      "cr_bugs": [309734],
      "description": "The first draw operation from an idle state is slow.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "wake_up_gpu_before_drawing"
      ]
    },
    {
      "id": 50,
      "description": "NVIDIA driver requires unbinding a GpuMemoryBuffer from the texture before mapping it to main memory",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "NVIDIA"
      },
      "features": [
        "release_image_after_use"
      ]
    },
    {
      "id": 51,
      "description": "TexSubImage2D() is faster for full uploads on ANGLE.",
      "os": {
        "type": "win"
      },
      "gl_renderer": {
        "op": "beginwith",
        "value": "ANGLE"
      },
      "features": [
        "texsubimage2d_faster_than_teximage2d"
      ]
    },
    {
      "id": 52,
      "description": "ES3 MSAA is broken on Qualcomm.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "disable_framebuffer_multisample"
      ]
    },
    {
      "id": 53,
      "cr_bugs": [321701],
      "description": "ES3 multisampling is too slow to be usable on Mali.",
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "gl_renderer": {
        "op": "beginwith",
        "value": "Mali"
      },
      "features": [
        "disable_framebuffer_multisample"
      ]
    },
    {
      "id": 54,
      "cr_bugs": [124764],
      "description": "Clear uniforms before first program use on all platforms",
      "features": [
        "clear_uniforms_before_first_program_use"
      ]
    },
    {
      "id": 55,
      "cr_bugs": [333885],
      "description": "Mesa drivers in Linux handle varyings without static use incorrectly",
      "os": {
        "type": "linux"
      },
      "driver_vendor": {
        "op": "=",
        "value": "Mesa"
      },
      "features": [
        "count_all_in_varyings_packing"
      ]
    },
    {
      "id": 56,
      "cr_bugs": [333885],
      "description": "Mesa drivers in ChromeOS handle varyings without static use incorrectly",
      "os": {
        "type": "chromeos"
      },
      "driver_vendor": {
        "op": "=",
        "value": "Mesa"
      },
      "features": [
        "count_all_in_varyings_packing"
      ]
    }
  ]
}

);  // LONG_STRING_CONST macro

}  // namespace gpu
