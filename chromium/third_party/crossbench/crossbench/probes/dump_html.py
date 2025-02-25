# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt

import os
from typing import List, Optional

from crossbench.path import AnyPath
from crossbench.probes.probe import Probe, ProbeConfigParser
from crossbench.probes.probe_context import ProbeContext
from crossbench.probes.result_location import ResultLocation
from crossbench.probes.results import EmptyProbeResult, ProbeResult
from crossbench.runner.groups.browsers import BrowsersRunGroup
from crossbench.runner.groups.repetitions import RepetitionsRunGroup
from crossbench.runner.run import Run


class DumpHtmlProbe(Probe):
  """
  General-purpose Probe that collects HTML dumps.
  """
  NAME = "dump_html"
  RESULT_LOCATION = ResultLocation.LOCAL

  @classmethod
  def config_parser(cls) -> ProbeConfigParser:
    parser = super().config_parser()
    # TODO: support stop dumps
    return parser

  def get_context(self, run: Run) -> DumpHtmlProbeContext:
    return DumpHtmlProbeContext(self, run)

  def merge_repetitions(self, group: RepetitionsRunGroup) -> ProbeResult:
    # TODO: implement
    return EmptyProbeResult()

  def merge_browsers(self, group: BrowsersRunGroup) -> ProbeResult:
    # TODO: implement
    return EmptyProbeResult()


class DumpHtmlProbeContext(ProbeContext[DumpHtmlProbe]):

  def __init__(self, probe: DumpHtmlProbe, run: Run) -> None:
    super().__init__(probe, run)
    self._results: List[AnyPath] = []

  def get_default_result_path(self) -> AnyPath:
    dump_dir = super().get_default_result_path()
    os.mkdir(dump_dir)
    return dump_dir

  def start(self) -> None:
    pass

  def stop(self) -> None:
    pass

  def dump_html(self, label: Optional[str] = None) -> None:
    if not label:
      label = str(dt.datetime.now().strftime("%Y-%m-%d_%H%M%S"))
    path = self.result_path / f"{label}.html"
    html = self.browser.js("return document.children[0].outerHTML",
                           dt.timedelta(seconds=10))
    with open(path, "w", encoding="utf-8") as dump_file:
      dump_file.write(html)
    self._results.append(path)

  def teardown(self) -> ProbeResult:
    if not self.browser_platform.is_dir(self.result_path):
      return EmptyProbeResult()
    return self.browser_result(file=tuple(self._results))
