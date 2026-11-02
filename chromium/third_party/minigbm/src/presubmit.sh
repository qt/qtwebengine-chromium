#!/bin/sh
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cros format \
	--exclude 'external/*' \
	--exclude 'gbm.h' \
	--include '*.[ch]' \
	--include '*.cc' \
	--include '*.cpp' \
	.
