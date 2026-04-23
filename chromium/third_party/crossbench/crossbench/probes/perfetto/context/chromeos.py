# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import Final, cast

from typing_extensions import override

from crossbench import path as pth
from crossbench.plt.chromeos_ssh import ChromeOsSshPlatform
from crossbench.probes.perfetto.constants import PERFETTO_CONFIG_NAME, \
    PERFETTO_TRACE_NAME
from crossbench.probes.perfetto.context.base import PerfettoProbeContext

PERFETTO_REMOTE_DIR_CROS: Final = pth.AnyPath("/usr/local/tmp")


class ChromeOsPerfettoProbeContext(PerfettoProbeContext):

  @property
  @override
  def browser_platform(self) -> ChromeOsSshPlatform:
    browser_platform = super().browser_platform
    isinstance(browser_platform, ChromeOsSshPlatform)
    return cast(ChromeOsSshPlatform, browser_platform)

  @override
  def get_browser_config_path(self) -> pth.AnyPath:
    return PERFETTO_REMOTE_DIR_CROS / (
        f"{self._file_prefix}_{PERFETTO_CONFIG_NAME}")

  @override
  def get_default_result_path(self) -> pth.AnyPath:
    return PERFETTO_REMOTE_DIR_CROS / (
        f"{self._file_prefix}_{PERFETTO_TRACE_NAME}")
