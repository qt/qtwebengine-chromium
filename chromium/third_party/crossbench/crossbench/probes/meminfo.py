# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import dataclasses
import datetime as dt
import json
from typing import TYPE_CHECKING, Any, ClassVar, Self, Type

from typing_extensions import override

from crossbench.path import AnyPath, LocalPath, safe_filename
from crossbench.probes.probe import Probe, ProbeConfigParser, ProbeContext

if TYPE_CHECKING:
  from crossbench import exception
  from crossbench.plt.process_meminfo import ProcessMeminfo
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.run import Run


class MeminfoProbe(Probe):
  """
    General-purpose Probe that records the specified meminfo.
  """
  NAME: ClassVar = "meminfo"

  @classmethod
  @override
  def config_parser(cls) -> ProbeConfigParser[Self]:
    parser = super().config_parser()
    return parser

  @override
  def get_context_cls(self) -> Type[MeminfoProbeContext]:
    return MeminfoProbeContext


class MeminfoProbeContext(ProbeContext[MeminfoProbe]):

  def __init__(self, probe: MeminfoProbe, run: Run) -> None:
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

  def _dump_file(self, title: str | None,
                 info_stack: exception.TInfoStack) -> LocalPath:
    name = "_".join(info_stack)
    if title:
      name = f"{title}.{name}"
    name = safe_filename(name).lower() + ".json"
    return self.local_result_path / name

  def _timeout_from_deadline(self, deadline: dt.datetime) -> dt.timedelta:
    timeout = deadline - dt.datetime.now()
    if timeout <= dt.timedelta(0):
      raise TimeoutError("dump_meminfo timed out")
    return timeout

  @override
  def invoke(self, info_stack: exception.TInfoStack, timeout: dt.timedelta,
             **kwargs) -> None:
    self._dump_meminfo(info_stack, timeout, **kwargs)

  def _dump_meminfo(self,
                    info_stack: exception.TInfoStack,
                    timeout: dt.timedelta,
                    browser: bool = True,
                    system: bool = False,
                    packages: tuple[str, ...] | None = None,
                    title: str | None = None,
                    **kwargs) -> None:
    self.expect_no_extra_kwargs(kwargs)

    deadline = dt.datetime.now() + timeout
    process_meminfos: list[ProcessMeminfo] = []

    if packages is None:
      packages = ()
    for package in packages:
      process_meminfos += self.browser_platform.process_meminfo(
          package, self._timeout_from_deadline(deadline))

    if browser:
      process_meminfos += self.browser.meminfo(
          self._timeout_from_deadline(deadline))

    meminfo_json: dict[str, Any] = {
        "info_stack": list(info_stack),
        "processes": []
    }

    if title is not None:
      meminfo_json["title"] = title

    if system:
      meminfo_json["system"] = self.browser_platform.system_meminfo(
          self._timeout_from_deadline(deadline))

    for process_meminfo in process_meminfos:
      meminfo_json["processes"].append(dataclasses.asdict(process_meminfo))

    self.browser.performance_mark("meminfo", detail=meminfo_json)
    with self._dump_file(title, info_stack).open("x", encoding="utf-8") as f:
      json.dump(meminfo_json, f)

  @override
  def teardown(self) -> ProbeResult:
    if not self.browser_platform.is_dir(self.result_path):
      return self.empty_result()
    return self.browser_result(file=tuple(self._results))
