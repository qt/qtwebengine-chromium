#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Wrapper script named "ld" that either executes gold or the system linker.

# The gold binaries are built on 64-bit Ubuntu Lucid and 32-bit Ubuntu Lucid,
# for x86_64 and i686, respectively.
# Note that we do not use "uname -m" because it prints the kernel architecture,
# which can cause failures on 64-bit kernels with 32-bit userlands.

base_dir=$(dirname "$0")
machine=$(getconf LONG_BIT)
if [ "$machine" = "64" ]; then
    exec $base_dir/gold64 "$@"
elif [ "$machine" = "32" ]; then
    exec $base_dir/gold32 "$@"
else
    echo Unknown architecture
    exit 1
fi
