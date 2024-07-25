# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations
import abc
import contextlib
import pathlib
from typing import Iterator, Optional

from crossbench import cli_helper, plt
from crossbench.browsers.browser import Browser
from crossbench.network.base import Network, TrafficShaper


class ReplayNetworkServer(abc.ABC):

  def __init__(self, archive_path: pathlib.Path) -> None:
    self._archive_path = cli_helper.parse_existing_file_path(archive_path)

  @contextlib.contextmanager
  @abc.abstractmethod
  def open(self, network: ReplayNetwork,
           browser: Browser) -> Iterator[ReplayNetworkServer]:
    del network, browser
    # TODO: implement
    yield self


class ReplayNetwork(Network):
  """ A network implementation that can be used to replay requests
  from a an archive."""
  def __init__(self,
               archive_path: pathlib.Path,
               traffic_shaper: Optional[TrafficShaper] = None,
               platform: plt.Platform = plt.PLATFORM,):
    super().__init__(traffic_shaper, platform)
    self._server : ReplayNetworkServer = self._init_server(archive_path)

  @abc.abstractmethod
  def _init_server(self, archive_path:pathlib.Path) ->  ReplayNetworkServer:
    pass

  @contextlib.contextmanager
  def open(self, browser: Browser) -> Iterator[Network]:
    with self._server.open(self, browser):
      with self._traffic_shaper.open(self, browser):
        yield self
