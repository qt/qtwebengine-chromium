// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_

#include <string>

#include "gpu/gpu_export.h"

#define GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)                   \
  GPU_OP(CLEAR_ALPHA_IN_READPIXELS,                          \
         clear_alpha_in_readpixels)                          \
  GPU_OP(CLEAR_UNIFORMS_BEFORE_FIRST_PROGRAM_USE,            \
         clear_uniforms_before_first_program_use)            \
  GPU_OP(COUNT_ALL_IN_VARYINGS_PACKING,                      \
         count_all_in_varyings_packing)                      \
  GPU_OP(DISABLE_ANGLE_INSTANCED_ARRAYS,                     \
         disable_angle_instanced_arrays)                     \
  GPU_OP(DISABLE_ASYNC_READPIXELS,                           \
         disable_async_readpixels)                           \
  GPU_OP(DISABLE_D3D11,                                      \
         disable_d3d11)                                      \
  GPU_OP(DISABLE_DEPTH_TEXTURE,                              \
         disable_depth_texture)                              \
  GPU_OP(DISABLE_EXT_DISCARD_FRAMEBUFFER,                    \
         disable_ext_discard_framebuffer)                    \
  GPU_OP(DISABLE_EXT_DRAW_BUFFERS,                           \
         disable_ext_draw_buffers)                           \
  GPU_OP(DISABLE_EXT_OCCLUSION_QUERY,                        \
         disable_ext_occlusion_query)                        \
  GPU_OP(DISABLE_FRAMEBUFFER_MULTISAMPLE,                    \
         disable_framebuffer_multisample)                    \
  GPU_OP(DISABLE_MULTIMONITOR_MULTISAMPLING,                 \
         disable_multimonitor_multisampling)                 \
  GPU_OP(DISABLE_OES_STANDARD_DERIVATIVES,                   \
         disable_oes_standard_derivatives)                   \
  GPU_OP(ENABLE_CHROMIUM_FAST_NPOT_MO8_TEXTURES,             \
         enable_chromium_fast_npot_mo8_textures)             \
  GPU_OP(EXIT_ON_CONTEXT_LOST,                               \
         exit_on_context_lost)                               \
  GPU_OP(FORCE_DISCRETE_GPU,                                 \
         force_discrete_gpu)                                 \
  GPU_OP(FORCE_INTEGRATED_GPU,                               \
         force_integrated_gpu)                               \
  GPU_OP(INIT_GL_POSITION_IN_VERTEX_SHADER,                  \
         init_gl_position_in_vertex_shader)                  \
  GPU_OP(MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_1024,               \
         max_cube_map_texture_size_limit_1024)               \
  GPU_OP(MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_4096,               \
         max_cube_map_texture_size_limit_4096)               \
  GPU_OP(MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_512,                \
         max_cube_map_texture_size_limit_512)                \
  GPU_OP(MAX_TEXTURE_SIZE_LIMIT_4096,                        \
         max_texture_size_limit_4096)                        \
  GPU_OP(NEEDS_GLSL_BUILT_IN_FUNCTION_EMULATION,             \
         needs_glsl_built_in_function_emulation)             \
  GPU_OP(NEEDS_OFFSCREEN_BUFFER_WORKAROUND,                  \
         needs_offscreen_buffer_workaround)                  \
  GPU_OP(RELEASE_IMAGE_AFTER_USE,                            \
         release_image_after_use)                            \
  GPU_OP(RESTORE_SCISSOR_ON_FBO_CHANGE,                      \
         restore_scissor_on_fbo_change)                      \
  GPU_OP(REVERSE_POINT_SPRITE_COORD_ORIGIN,                  \
         reverse_point_sprite_coord_origin)                  \
  GPU_OP(SET_TEXTURE_FILTER_BEFORE_GENERATING_MIPMAP,        \
         set_texture_filter_before_generating_mipmap)        \
  GPU_OP(SWIZZLE_RGBA_FOR_ASYNC_READPIXELS,                  \
         swizzle_rgba_for_async_readpixels)                  \
  GPU_OP(TEXSUBIMAGE2D_FASTER_THAN_TEXIMAGE2D,               \
         texsubimage2d_faster_than_teximage2d)               \
  GPU_OP(UNBIND_FBO_ON_CONTEXT_SWITCH,                       \
         unbind_fbo_on_context_switch)                       \
  GPU_OP(UNFOLD_SHORT_CIRCUIT_AS_TERNARY_OPERATION,          \
         unfold_short_circuit_as_ternary_operation)          \
  GPU_OP(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,          \
         use_client_side_arrays_for_stream_buffers)          \
  GPU_OP(USE_CURRENT_PROGRAM_AFTER_SUCCESSFUL_LINK,          \
         use_current_program_after_successful_link)          \
  GPU_OP(USE_NON_ZERO_SIZE_FOR_CLIENT_SIDE_STREAM_BUFFERS,   \
         use_non_zero_size_for_client_side_stream_buffers)   \
  GPU_OP(USE_VIRTUALIZED_GL_CONTEXTS,                        \
         use_virtualized_gl_contexts)                        \
  GPU_OP(VALIDATE_MULTISAMPLE_BUFFER_ALLOCATION,             \
         validate_multisample_buffer_allocation)             \
  GPU_OP(WAKE_UP_GPU_BEFORE_DRAWING,                         \
         wake_up_gpu_before_drawing)                         \

namespace gpu {

// Provides all types of GPU driver bug workarounds.
enum GPU_EXPORT GpuDriverBugWorkaroundType {
#define GPU_OP(type, name) type,
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES
};

GPU_EXPORT std::string GpuDriverBugWorkaroundTypeToString(
    GpuDriverBugWorkaroundType type);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_
