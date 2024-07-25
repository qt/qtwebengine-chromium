# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations
import abc

import contextlib
import pathlib

from typing import Iterator, TYPE_CHECKING

from crossbench.network.replay.base import ReplayNetwork, ReplayNetworkServer

if TYPE_CHECKING:
  from crossbench.browsers.browser import Browser


class WprReplayNetworkServer(ReplayNetworkServer):

  @contextlib.contextmanager
  def open(self, network: ReplayNetwork,
           browser: Browser) -> Iterator[ReplayNetworkServer]:
    del network, browser
    # TODO: implement starting and stopping the WprServer
    yield self


class WprReplayNetwork(ReplayNetwork):

  @abc.abstractmethod
  def _init_server(self, archive_path: pathlib.Path) -> WprReplayNetworkServer:
    return WprReplayNetworkServer(archive_path)
