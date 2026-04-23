# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import Final

from typing_extensions import override

from crossbench import path as pth
from crossbench.plt.android_adb import AndroidAdbPlatform
from crossbench.probes.perfetto.constants import PERFETTO_CONFIG_NAME, \
    PERFETTO_TRACE_NAME
from crossbench.probes.perfetto.context.base import PerfettoProbeContext

PERFETTO_CONFIG_REMOTE_DIR_ANDROID: Final = pth.AnyPath(
    "/data/misc/perfetto-configs/")
PERFETTO_TRACE_REMOTE_DIR_ANDROID: Final = pth.AnyPath(
    "/data/misc/perfetto-traces/")


class AndroidPerfettoProbeContext(PerfettoProbeContext):

  @override
  def get_browser_config_path(self) -> pth.AnyPath:
    return PERFETTO_CONFIG_REMOTE_DIR_ANDROID / (
        f"{self._file_prefix}_{PERFETTO_CONFIG_NAME}")

  @override
  def get_default_result_path(self) -> pth.AnyPath:
    return PERFETTO_TRACE_REMOTE_DIR_ANDROID / (
        f"{self._file_prefix}_{PERFETTO_TRACE_NAME}")

  @property
  @override
  def browser_platform(self) -> AndroidAdbPlatform:
    browser_platform = super().browser_platform
    assert isinstance(browser_platform, AndroidAdbPlatform)
    return browser_platform
