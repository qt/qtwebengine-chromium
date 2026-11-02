/*
 * Copyright 2017 Advanced Micro Devices. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_AMDGPU

#include "drv.h"

struct dri_driver;

void *dri_dlopen(const char *dri_so_path);
void dri_dlclose(void *dri_so_handle);

struct dri_driver *dri_init(struct driver *drv, const char *dri_so_path, const char *driver_suffix);
void dri_close(struct dri_driver *dri);

int dri_bo_create(struct dri_driver *dri, struct bo *bo, uint32_t width, uint32_t height,
		  uint32_t format, uint64_t use_flags);
int dri_bo_create_with_modifiers(struct dri_driver *dri, struct bo *bo, uint32_t width,
				 uint32_t height, uint32_t format, uint64_t use_flags,
				 const uint64_t *modifiers, uint32_t modifier_count);
int dri_bo_import(struct dri_driver *dri, struct bo *bo, struct drv_import_fd_data *data);
int dri_bo_release(struct dri_driver *dri, struct bo *bo);
int dri_bo_destroy(struct dri_driver *dri, struct bo *bo);
void *dri_bo_map(struct dri_driver *dri, struct bo *bo, struct vma *vma, size_t plane,
		 uint32_t map_flags);
int dri_bo_unmap(struct dri_driver *dri, struct bo *bo, struct vma *vma);

size_t dri_num_planes_from_modifier(struct dri_driver *dri, uint32_t format, uint64_t modifier);
bool dri_query_modifiers(struct dri_driver *dri, uint32_t format, int max, uint64_t *modifiers,
			 int *count);
#endif
