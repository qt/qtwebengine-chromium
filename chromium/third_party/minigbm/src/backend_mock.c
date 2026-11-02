/*
 * Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drv_priv.h"

static int backend_mock_init(struct driver *drv)
{
	return 0;
}

const struct backend backend_mock = {
	.name = "Mock Backend",
	.init = backend_mock_init,
};
