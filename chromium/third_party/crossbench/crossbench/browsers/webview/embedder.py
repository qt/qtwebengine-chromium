# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import logging
import os
import shlex
import sys
from typing import TYPE_CHECKING, Sequence, cast

from selenium import webdriver
from selenium.webdriver.chrome.options import Options as ChromeOptions
from typing_extensions import override

from crossbench.browsers.webview.webview import Webview
from crossbench.cli import ui

if TYPE_CHECKING:
  from selenium.webdriver.chromium.webdriver import ChromiumDriver

  from crossbench import path as pth
  from crossbench.benchmarks.embedder.embedder_benchmark import (
      EmbedderBenchmark)
  from crossbench.runner.groups.session import BrowserSessionRunGroup

EMBEDDER_SHORT_NAME_TO_PACKAGE = {
  "googlequicksearchbox": "com.google.android.googlequicksearchbox",
  "velvet": "com.google.android.googlequicksearchbox",
}


class WebviewEmbedder(Webview):
  @override
  def start(self, session: BrowserSessionRunGroup) -> None:
    # Start is a no-op. Embedder activity will be started by the Benchmark.
    # Webview will be started by the Embedder. Driver will be started
    # by the ProbeContext. We do, however, need to run custom setup commands,
    # set up browser flags, and kill any currently running Embedder app
    # instances to make sure it picks up the new flags when started by the
    # Benchmark.
    session_benchmark = cast("EmbedderBenchmark", session.benchmark)
    if setup_command_config := session_benchmark.embedder_setup_command_config:
      for command in setup_command_config.commands:
        self.platform.sh(*command.command)
    self._backup_chrome_flags()
    args = self._get_browser_flags_for_session(session)
    logging.debug("%s: setting flags file contents in %s", self,
                  self._chrome_command_line_path)
    self.platform.write_text(self._chrome_command_line_path,
                             shlex.join(("webview", *args)))
    self.platform.sh("pkill", "-f", self.android_package)
    self._log_browser_start(args)
    self._is_running = True

  @override
  def quit(self) -> None:
    # External code that started the driver is responsible for shutting it down.
    self._is_running = False
    self._restore_chrome_flags()
    self._teardown_cache_dir()

  @override
  def _start_driver(self, session: BrowserSessionRunGroup,
                    driver_path: pth.AnyPath) -> ChromiumDriver:
    options = self._create_options(session, [])
    service = webdriver.ChromeService(executable_path=os.fspath(driver_path))
    driver = webdriver.Chrome(options=options, service=service)
    return driver

  def start_driver(self, session: BrowserSessionRunGroup) -> ChromiumDriver:
    assert self._driver_path
    self._private_driver = self._start_driver(session, self._driver_path)
    self._set_driver_timeouts(session)
    return self._private_driver

  @override
  def _create_options(self, session: BrowserSessionRunGroup,
                      args: Sequence[str]) -> ChromeOptions:
    options = ChromeOptions()
    options.add_experimental_option("androidPackage", self.android_package)
    session_benchmark = cast("EmbedderBenchmark", session.benchmark)
    if process_name := session_benchmark.embedder_process_name:
      options.add_experimental_option(
          "androidProcess",
          f"{self.android_package}:{process_name}")
    options.add_experimental_option("androidUseRunningApp", True)
    return options

  @override
  def _log_browser_start(self,
                         args: tuple[str, ...],
                         driver_path: pth.AnyPath | None = None) -> None:
    super()._log_browser_start(args, driver_path)
    logging.info("📱 STARTING BROWSER Embedder: %s",
                 self.platform.app_version(self.android_package))

  @override
  def _lookup_android_package(self, path: pth.AnyPath) -> str:
    if path.suffix == ".apk":
      path_str = str(path).lower()
      for short_name, package_name in EMBEDDER_SHORT_NAME_TO_PACKAGE.items():
        if short_name in path_str:
          return package_name
    return super()._lookup_android_package(path)

  @override
  def _setup_binary(self) -> None:
    if self.path.suffix == ".apk":
      with ui.spinner():
        sys.stdout.write(f"   Installing {self.path.name} on {self.platform}\r")
        local_path = cast("pth.LocalPath", self.path)
        self.platform.adb.install(local_path)
    super()._setup_binary()
