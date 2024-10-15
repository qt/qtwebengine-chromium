# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import logging
from typing import TYPE_CHECKING, Optional, Union

from crossbench import cli_helper
from crossbench.flags.base import Flags
from crossbench.helper.path_finder import WprGoToolFinder
from crossbench.network.replay.base import GS_PREFIX, ReplayNetwork
from crossbench.network.replay.web_page_replay import WprReplayServer
from crossbench.plt import PLATFORM, Platform

if TYPE_CHECKING:
  from crossbench.browsers.browser import Browser
  from crossbench.network.base import TrafficShaper
  from crossbench.path import LocalPath
  from crossbench.runner.groups.session import BrowserSessionRunGroup


class WprReplayNetwork(ReplayNetwork):

  def __init__(self,
               archive: Union[LocalPath, str],
               traffic_shaper: Optional[TrafficShaper] = None,
               wpr_go_bin: Optional[LocalPath] = None,
               browser_platform: Platform = PLATFORM):
    super().__init__(archive, traffic_shaper, browser_platform)
    if not wpr_go_bin:
      if local_wpr_go := WprGoToolFinder(self.runner_platform).path:
        wpr_go_bin = self.runner_platform.local_path(local_wpr_go)
    if not wpr_go_bin:
      raise RuntimeError(
          f"Could not find wpr.go binary on {self.runner_platform}")
    if not self.runner_platform.which("go"):
      raise ValueError(f"'go' binary not found on {self.runner_platform}")
    self._wpr_go_bin: LocalPath = self.runner_platform.local_path(
        cli_helper.parse_binary_path(wpr_go_bin, "wpr.go source"))
    self._server: Optional[WprReplayServer] = None

  def extra_flags(self, browser: Browser) -> Flags:
    assert self.is_running, "Extra network flags are not valid"
    assert self._server
    if not browser.attributes.is_chromium_based:
      raise ValueError(
          "Only chromium-based browsers are supported for wpr replay.")
    if browser.platform.is_remote:
      self._setup_remote_port_forwarding(browser)
    # TODO: make ports configurable.
    # TODO: use traffic shaper settings.
    return Flags({
        "--host-resolver-rules":
            (f"MAP *:80 127.0.0.1:{self._server.http_port},"
             f"MAP *:443 127.0.0.1:{self._server.https_port},"
             "EXCLUDE localhost"),
        # TODO: read this from wpr_public_hash.txt like in the recorder probe
        "--ignore-certificate-errors-spki-list":
            ("PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,"
             "2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=")
    })

  def _setup_remote_port_forwarding(self, browser: Browser) -> None:
    logging.info("REMOTE PORT FORWARDING: %s <= %s", self.runner_platform,
                 browser.platform)
    platform = browser.platform
    # TODO: create port-forwarder service that is shut down properly.
    # TODO: make ports configurable
    platform.reverse_port_forward(8080, 8080)
    platform.reverse_port_forward(8081, 8081)

  @contextlib.contextmanager
  def _open_replay_server(self, session: BrowserSessionRunGroup):
    self._server = WprReplayServer(
        self.archive_path,
        self._wpr_go_bin,
        http_port=8080,
        https_port=8081,
        log_path=session.out_dir / "network.wpr.log")
    logging.debug("Starting WPR server")
    try:
      self._server.start()
      yield self
    finally:
      self._server.stop()

  def __str__(self) -> str:
    return f"WPR(archive={self.archive_path}, speed={self.traffic_shaper})"
