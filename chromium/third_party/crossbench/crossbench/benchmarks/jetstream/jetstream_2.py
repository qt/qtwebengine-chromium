# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import datetime as dt
import logging
from typing import TYPE_CHECKING, Any, ClassVar, Final, Mapping, Optional, \
    Sequence, Type

from immutabledict import immutabledict
from typing_extensions import override

from crossbench.action_runner.action.enums import ReadyState
from crossbench.benchmarks.base import PressBenchmarkStoryFilter
from crossbench.benchmarks.jetstream.jetstream import JetStreamBenchmark, \
    JetStreamProbe, JetStreamProbeContext, JetStreamStory
from crossbench.helper import url_helper
from crossbench.parse import NumberParser

if TYPE_CHECKING:
  import argparse

  from crossbench.runner.actions import Actions
  from crossbench.runner.run import Run


class JetStream2Probe(JetStreamProbe, metaclass=abc.ABCMeta):
  """
  JetStream2-specific Probe.
  Extracts all JetStream2 times and scores.
  """


class JetStream2ProbeContext(JetStreamProbeContext):
  pass


class JetStream2Story(JetStreamStory, metaclass=abc.ABCMeta):
  SUBSTORIES: ClassVar[tuple[str, ...]] = (
      "WSL",
      "UniPoker",
      "uglify-js-wtb",
      "typescript",
      "tsf-wasm",
      "tagcloud-SP",
      "string-unpack-code-SP",
      "stanford-crypto-sha256",
      "stanford-crypto-pbkdf2",
      "stanford-crypto-aes",
      "splay",
      "segmentation",
      "richards-wasm",
      "richards",
      "regexp",
      "regex-dna-SP",
      "raytrace",
      "quicksort-wasm",
      "prepack-wtb",
      "pdfjs",
      "OfflineAssembler",
      "octane-zlib",
      "octane-code-load",
      "navier-stokes",
      "n-body-SP",
      "multi-inspector-code-load",
      "ML",
      "mandreel",
      "lebab-wtb",
      "json-stringify-inspector",
      "json-parse-inspector",
      "jshint-wtb",
      "HashSet-wasm",
      "hash-map",
      "gcc-loops-wasm",
      "gbemu",
      "gaussian-blur",
      "float-mm.c",
      "FlightPlanner",
      "first-inspector-code-load",
      "espree-wtb",
      "earley-boyer",
      "delta-blue",
      "date-format-xparb-SP",
      "date-format-tofte-SP",
      "crypto-sha1-SP",
      "crypto-md5-SP",
      "crypto-aes-SP",
      "crypto",
      "coffeescript-wtb",
      "chai-wtb",
      "cdjs",
      "Box2D",
      "bomb-workers",
      "Basic",
      "base64-SP",
      "babylon-wtb",
      "Babylon",
      "async-fs",
      "Air",
      "ai-astar",
      "acorn-wtb",
      "3d-raytrace-SP",
      "3d-cube-SP",
  )

  def __init__(self,
               substories: Sequence[str] = (),
               url: Optional[str] = None,
               url_params: Optional[Mapping[str, str]] = None) -> None:
    self._url_params: Final[Mapping[str, str]] = immutabledict(url_params or {})
    super().__init__(substories=substories, url=url)

  @property
  def url_params(self) -> dict[str, str]:
    params: dict[str, str] = dict(self._url_params)
    return params

  @override
  def get_run_url(self, run: Run) -> str:
    url = super().get_run_url(run)
    url = url_helper.update_url_query(url, self.url_params)
    if url != self.url:
      logging.info("CUSTOM URL: %s", url)
    return url

  @override
  def setup(self, run: Run) -> None:
    with run.actions("Setup") as actions:
      actions.show_url(
          url=self.get_run_url(run),
          ready_state=ReadyState.COMPLETE,
          timeout=dt.timedelta(seconds=10))
      self.setup_stories(actions)
      actions.wait_js_condition(
          'return document.querySelectorAll("#results>.benchmark").length > 0;',
          1,
          timeout=self.duration + dt.timedelta(seconds=30))

  def setup_stories(self, actions: Actions) -> None:
    if self.substories == self.SUBSTORIES:
      return
    actions.wait_js_condition(
        "return globalThis?.JetStream?.benchmarks?.length > 0;",
        0.1,
        timeout=10)
    actions.js(
        """
        let benchmarks = arguments[0];
        JetStream.benchmarks = JetStream.benchmarks.filter(
            benchmark => benchmarks.includes(benchmark.name));
        """,
        arguments=[self.substories])


ProbeClsTupleT = tuple[Type[JetStream2Probe], ...]


class JetStream2BenchmarkStoryFilter(PressBenchmarkStoryFilter):
  __doc__ = PressBenchmarkStoryFilter.__doc__

  @classmethod
  @override
  def add_cli_arguments(
      cls, parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    parser = super().add_cli_arguments(parser)
    parser.add_argument(
        "--iteration-count",
        "--iterations",
        default=None,
        type=NumberParser.positive_int,
        help="Number of iterations each JetStream subtest is run "
        "within the same session. \n"
        "Note: --repetitions restarts the whole benchmark, --iterations runs "
        "the same test tests n-times within the same session without the setup "
        "overhead of starting up a whole new browser. \n"
        "This option is not supported on the official benchmark "
        "before version 3.0.")
    parser.add_argument(
        "--worst-case-count",
        "--worst",
        default=None,
        type=NumberParser.positive_int,
        help="Number of iterations each JetStream subtest uses to calculate "
        "the worst score. \n"
        "This option is not supported on the official benchmark "
        "before version 3.0.")
    return parser

  @classmethod
  @override
  def kwargs_from_cli(cls, args: argparse.Namespace) -> dict[str, Any]:
    kwargs = super().kwargs_from_cli(args)
    kwargs["url_params"] = cls.url_params_from_cli(args)
    return kwargs

  @classmethod
  def url_params_from_cli(cls, args: argparse.Namespace) -> dict[str, str]:
    url_params: dict[str, str] = {}
    if worst_case_count := args.worst_case_count:
      url_params["worstCaseCount"] = str(worst_case_count)
    if iteration_count := args.iteration_count:
      url_params["iterationCount"] = str(iteration_count)
    return url_params

  def __init__(self,
               story_cls: Type[JetStream2Story],
               patterns: Sequence[str],
               args: Optional[argparse.Namespace] = None,
               separate: bool = False,
               url: Optional[str] = None,
               url_params: Optional[Mapping[str, str]] = None) -> None:
    self._url_params: Final[Mapping[str, str]] = immutabledict(url_params or {})
    assert issubclass(story_cls, JetStream2Story)
    super().__init__(story_cls, patterns, args, separate, url)

  @override
  def create_stories_from_names(self, names: list[str],
                                separate: bool) -> Sequence[JetStream2Story]:
    return self.story_cls.from_names(
        names, separate=separate, url=self.url, url_params=self._url_params)


class JetStream2Benchmark(JetStreamBenchmark):
  STORY_FILTER_CLS: ClassVar = JetStream2BenchmarkStoryFilter
