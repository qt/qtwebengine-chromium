# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
from typing import TYPE_CHECKING, Iterable, Optional

from crossbench import exception
from crossbench.probes.results import ProbeResult, ProbeResultDict

if TYPE_CHECKING:
  import pathlib

  from crossbench.probes.probe import Probe
  from crossbench.runner.run import Run
  from crossbench.runner.runner import Runner
  from crossbench.types import JsonDict


class RunGroup(abc.ABC):

  def __init__(self, throw: bool = False) -> None:
    self._exceptions = exception.Annotator(throw)
    self._path: Optional[pathlib.Path] = None
    self._merged_probe_results: Optional[ProbeResultDict] = None

  def _set_path(self, path: pathlib.Path) -> None:
    assert self._path is None
    self._path = path
    self._merged_probe_results = ProbeResultDict(path)

  @property
  def results(self) -> ProbeResultDict:
    assert self._merged_probe_results
    return self._merged_probe_results

  @property
  def path(self) -> pathlib.Path:
    assert self._path
    return self._path

  @property
  def exceptions(self) -> exception.Annotator:
    return self._exceptions

  @property
  def is_success(self) -> bool:
    return self._exceptions.is_success

  @property
  @abc.abstractmethod
  def info_stack(self) -> exception.TInfoStack:
    pass

  @property
  @abc.abstractmethod
  def runs(self) -> Iterable[Run]:
    pass

  @property
  def failed_runs(self) -> Iterable[Run]:
    for run in self.runs:
      if not run.is_success:
        yield run

  @property
  def info(self) -> JsonDict:
    return {
        "runs": len(tuple(self.runs)),
        "failed runs": len(tuple(self.failed_runs))
    }

  def get_local_probe_result_path(self,
                                  probe: Probe,
                                  exists_ok: bool = False) -> pathlib.Path:
    new_file = self.path / probe.result_path_name
    if not exists_ok:
      assert not new_file.exists(), (
          f"Merged file {new_file} for {self.__class__} exists already.")
    return new_file

  def merge(self, runner: Runner) -> None:
    assert self._merged_probe_results
    with self._exceptions.info(*self.info_stack):
      for probe in reversed(tuple(runner.probes)):
        with self._exceptions.capture(f"Probe {probe.name} merge results"):
          results = self._merge_probe_results(probe)
          if results is None:
            continue
          self._merged_probe_results[probe] = results

  @abc.abstractmethod
  def _merge_probe_results(self, probe: Probe) -> ProbeResult:
    pass
