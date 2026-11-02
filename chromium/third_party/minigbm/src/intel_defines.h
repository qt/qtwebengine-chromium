/*
 * Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define ARRAY_SIZE(A) (sizeof(A) / sizeof(*(A)))

static const uint32_t scanout_render_formats[] = {
	DRM_FORMAT_ABGR2101010, DRM_FORMAT_ABGR8888,	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ARGB8888,	DRM_FORMAT_RGB565,	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_XBGR8888,	DRM_FORMAT_XRGB2101010, DRM_FORMAT_XRGB8888,
};

static const uint32_t render_formats[] = {
	DRM_FORMAT_ABGR16161616F,
};

static const uint32_t texture_only_formats[] = {
	DRM_FORMAT_R8,	   DRM_FORMAT_NV12,	      DRM_FORMAT_P010,
	DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID,
};

static const uint32_t image_storage_formats[] = {
	DRM_FORMAT_ABGR2101010,	  DRM_FORMAT_ABGR8888,	  DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ARGB8888,	  DRM_FORMAT_RGB565,	  DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_XBGR8888,	  DRM_FORMAT_XRGB2101010, DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ABGR16161616F, DRM_FORMAT_R8,
};

static const uint32_t external_resolve_formats[] = {
	DRM_FORMAT_YVU420_ANDROID, // AHARDWAREBUFFER_FORMAT_YV12
	DRM_FORMAT_NV12,	   // AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420
	DRM_FORMAT_P010,	   // AHARDWAREBUFFER_FORMAT_YCbCr_P010
};

static const uint64_t gen12_modifier_order[] = {
	I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS,
	I915_FORMAT_MOD_Y_TILED,
	I915_FORMAT_MOD_X_TILED,
	DRM_FORMAT_MOD_LINEAR,
};

static const uint64_t xe_lpdp_modifier_order[] = {
	/* TODO(ryanneph): I915_FORMAT_MOD_4_TILED_MTL_RC_CCS, */
	I915_FORMAT_MOD_4_TILED,
	I915_FORMAT_MOD_4_TILED_LNL_CCS,
	I915_FORMAT_MOD_X_TILED,
	DRM_FORMAT_MOD_LINEAR,
};

const uint16_t gen12_ids[] = {
	0x4c8a, 0x4c8b, 0x4c8c, 0x4c90, 0x4c9a, 0x4680, 0x4681, 0x4682, 0x4683, 0x4688,
	0x4689, 0x4690, 0x4691, 0x4692, 0x4693, 0x4698, 0x4699, 0x4626, 0x4628, 0x462a,
	0x46a0, 0x46a1, 0x46a2, 0x46a3, 0x46a6, 0x46a8, 0x46aa, 0x46b0, 0x46b1, 0x46b2,
	0x46b3, 0x46c0, 0x46c1, 0x46c2, 0x46c3, 0x9A40, 0x9A49, 0x9A59, 0x9A60, 0x9A68,
	0x9A70, 0x9A78, 0x9AC0, 0x9AC9, 0x9AD9, 0x9AF8, 0x4905, 0x4906, 0x4907, 0x4908,
};

const uint16_t adlp_ids[] = {
	0x46A0, 0x46A1, 0x46A2, 0x46A3, 0x46A6, 0x46A8, 0x46AA, 0x462A,
	0x4626, 0x4628, 0x46B0, 0x46B1, 0x46B2, 0x46B3, 0x46C0, 0x46C1,
	0x46C2, 0x46C3, 0x46D0, 0x46D1, 0x46D2, 0x46D3, 0x46D4,
};

const uint16_t rplp_ids[] = {
	0xA720, 0xA721, 0xA7A0, 0xA7A1, 0xA7A8, 0xA7A9, 0xA7AA, 0xA7AB, 0xA7AC, 0xA7AD,
};

const uint16_t mtl_ids[] = {
	0x7D40, 0x7D60, 0x7D45, 0x7D55, 0x7DD5,
};

const uint16_t lnl_ids[] = {
	0x6420,
	0x64A0,
	0x64B0,
};

const uint16_t ptl_ids[] = { 0xB080, 0xB081, 0xB082, 0xB083, 0xB08F,
			     0xB090, 0xB0A0, 0xB0B0, 0xFD80, 0xFD81 };
