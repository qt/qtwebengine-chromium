# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
from typing import TYPE_CHECKING, ClassVar, Self, Type

from typing_extensions import override

from crossbench.probes.probe import Probe, ProbeConfigParser, ProbeContext

if TYPE_CHECKING:
  from crossbench import exception
  from crossbench.path import AnyPath
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.run import Run


class DumpHtmlProbe(Probe):
  """
  General-purpose Probe that collects HTML dumps.
  """
  NAME: ClassVar = "dump_html"

  @classmethod
  @override
  def config_parser(cls) -> ProbeConfigParser[Self]:
    parser = super().config_parser()
    # TODO: support stop dumps
    return parser

  @override
  def get_context_cls(self) -> Type[DumpHtmlProbeContext]:
    return DumpHtmlProbeContext

  # TODO: implement merge_repetitions()
  # TODO: implement merge_browsers()


class DumpHtmlProbeContext(ProbeContext[DumpHtmlProbe]):

  def __init__(self, probe: DumpHtmlProbe, run: Run) -> None:
    super().__init__(probe, run)
    self._results: list[AnyPath] = []

  @override
  def get_default_result_path(self) -> AnyPath:
    dump_dir = super().get_default_result_path()
    self.host_platform.mkdir(dump_dir)
    return dump_dir

  def start(self) -> None:
    pass

  def stop(self) -> None:
    pass

  @override
  def invoke(self, info_stack: exception.TInfoStack, timeout: dt.timedelta,
             **kwargs) -> None:
    del timeout

    self._dump(info_stack, **kwargs)

  def _dump(self,
            info_stack: exception.TInfoStack,
            suffix: str | None = None,
            **kwargs) -> None:
    self.expect_no_extra_kwargs(kwargs)

    if not suffix:
      suffix = str(dt.datetime.now().strftime("%Y-%m-%d_%H%M%S"))
    label = "_".join(info_stack) + f"_{suffix}"
    path = self.result_path / f"{label}.html"
    html = self.browser.js("return document.children[0].outerHTML",
                           dt.timedelta(seconds=10))
    self.host_platform.write_text(path, html)
    self._results.append(path)

  @override
  def teardown(self) -> ProbeResult:
    if not self.browser_platform.is_dir(self.result_path):
      return self.empty_result()
    return self.browser_result(file=tuple(self._results))
