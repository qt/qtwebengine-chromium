# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import logging
from typing import (TYPE_CHECKING, Any, ClassVar, Iterable, Mapping, Optional,
                    Sequence, cast)

from typing_extensions import override

from crossbench.action_runner.action.open_devtools import OpenDevToolsAction
from crossbench.benchmarks.base import Benchmark
from crossbench.benchmarks.benchmark_probe import BenchmarkProbeMixin
from crossbench.probes.metric import MetricsMerger
from crossbench.probes.metrics_internals import (
    ChromeMetricsInternalsProbe, ChromeMetricsInternalsProbeContext)
from crossbench.stories.story import Story

if TYPE_CHECKING:
  import argparse

  import Type

  from crossbench.action_runner.config import ActionRunnerConfig
  from crossbench.browsers.attributes import BrowserAttributes
  from crossbench.cli.parser import CrossBenchArgumentParser
  from crossbench.cli.types import Subparsers
  from crossbench.flags.base import Flags
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.actions import Actions
  from crossbench.runner.groups.browsers import BrowsersRunGroup
  from crossbench.runner.groups.repetitions import RepetitionsRunGroup
  from crossbench.runner.groups.stories import StoriesRunGroup
  from crossbench.runner.run import Run
  from crossbench.types import Json

class DevToolsFrontendLoadTimeProbe(ChromeMetricsInternalsProbe,
                                    BenchmarkProbeMixin):
  """
  Probe that collects DevTools Frontend load time from
  chrome://metrics-internals/structured.
  """
  NAME: ClassVar = "devtools_frontend_load_time"

  def __init__(self, *args, **kwargs) -> None:
    BenchmarkProbeMixin.__init__(self, *args, **kwargs)
    ChromeMetricsInternalsProbe.__init__(self, "DevTools", "Impression", -1,
                                         "TimeSinceSessionStart")

  @override
  def get_context_cls(self) -> Type[DevToolsFrontendLoadTimeProbeContext]:
    return DevToolsFrontendLoadTimeProbeContext

  @override
  def merge_stories(self, group: StoriesRunGroup) -> ProbeResult:
    merger = MetricsMerger.merge_json_list(
        (repetition_group.results[self].json
         for repetition_group in group.repetitions_groups),
        merge_duplicate_paths=True)
    self.preserver_high_resolution_data(merger, group.repetitions_groups)
    return self.write_group_result(
        group,
        merger,
    )

  def preserver_high_resolution_data(
      self, merger: MetricsMerger,
      repetitions_groups: Iterable[RepetitionsRunGroup]) -> None:
    for repetition_group in repetitions_groups:
      story: DevToolsFrontendStory = cast(DevToolsFrontendStory,
                                          repetition_group.story)
      for prefix_path in story.reporting_prefixes():
        merger.merge_json_file(
            repetition_group.results[self].json,
            prefix_path=prefix_path,
            merge_duplicate_paths=True)

  @override
  def merge_browsers(self, group: BrowsersRunGroup) -> ProbeResult:
    merger = MetricsMerger.merge_json_list(
        (browser_group.results[self].json
         for browser_group in group.repetitions_groups),
        merge_duplicate_paths=True)
    self.preserver_high_resolution_data(merger, group.repetitions_groups)
    return self.write_group_result(
        group,
        merger,
    )


class DevToolsFrontendLoadTimeProbeContext(ChromeMetricsInternalsProbeContext):

  @override
  def to_json(self, actions: Actions) -> Json:
    return {"load_time": self._metric_value}


class DevToolsFrontendStory(Story):

  def run(self, run: Run) -> None:
    site, panel = self.name.split("_")
    action_runner = run.action_runner
    with run.actions("Show URL") as actions:
      actions.show_url(DevToolsFrontendBenchmark.STORY_URLS[site])
      actions.wait(1.0)  # Wait for page load.
      action_runner.open_devtools(run, OpenDevToolsAction(panel_name=panel))
      actions.wait(1.5)  # Let DevTools settle.
    logging.info("Stopping benchmark...")

  @classmethod
  @override
  def all_story_names(cls) -> Sequence[str]:
    return ()

  def reporting_prefixes(self) -> Sequence[tuple[str, ...]]:
    site, panel = self.name.split("_")
    return [(site,), (panel,), (site, panel)]


class DevToolsFrontendBenchmark(Benchmark):
  """
  Benchmark runner for DevTools.
  """
  NAME: ClassVar = "devtools_frontend"
  DEFAULT_STORY_CLS: ClassVar = DevToolsFrontendStory
  STORY_URLS: ClassVar[Mapping[str, str]] = {
      "blank": "about:blank",
      "speedometertests":
          "https://chromium-workloads.web.app/speedometer/v3.1/"
          "?iterationCount=1&startAutomatically"
          "&suites=TodoMVC-Angular-Complex-DOM"
          ",TodoMVC-JavaScript-ES5-Complex-DOM,TodoMVC-React-Complex-DOM",
      "dailybroadcast": "https://browserben.ch/speedometer/v3.1/resources/"
                        "newssite/news-next/dist/index.html",
  }
  PANEL_NAMES: ClassVar[Sequence[str]] = ("elements", "console", "network",
                                          "sources", "resources")
  PROBES: ClassVar[tuple[Type[DevToolsFrontendLoadTimeProbe],
                         ...]] = (DevToolsFrontendLoadTimeProbe,)
  DEFAULT_REPETITIONS: ClassVar[int] = 2

  def __init__(
      self,
      sites: Sequence[str],
      panels: Sequence[str],
      action_runner_config: Optional[ActionRunnerConfig] = None,
  ) -> None:
    stories = tuple(
        DevToolsFrontendStory(f"{site}_{panel}")
        for site in sites
        for panel in panels)
    super().__init__(stories, action_runner_config)

  @classmethod
  @override
  def add_cli_parser(cls, subparsers: Subparsers) -> CrossBenchArgumentParser:
    parser = super().add_cli_parser(subparsers)
    parser.add_argument(
        "--sites",
        type=str,
        default=",".join(cls.STORY_URLS.keys()),
        help="The sites to test.",
    )
    parser.add_argument(
        "--panels",
        type=str,
        default=",".join(cls.PANEL_NAMES),
        help="The panels to test.",
    )
    return parser

  @classmethod
  @override
  def kwargs_from_cli(cls, args: argparse.Namespace) -> dict[str, Any]:
    kwargs = super().kwargs_from_cli(args)
    sites = [site for site in args.sites.split(",") if site in cls.STORY_URLS
            ] or cls.STORY_URLS.keys()
    panels = [
        panel for panel in args.panels.split(",") if panel in cls.PANEL_NAMES
    ] or cls.PANEL_NAMES
    if args.sites and len(args.sites.split(",")) != len(sites):
      logging.warning("Some specified sites are invalid. Using valid sites: %s",
                      sites)
    if args.panels and len(args.panels.split(",")) != len(panels):
      logging.warning(
          "Some specified panels are invalid. Using valid panels: %s", panels)
    kwargs["sites"] = sites
    kwargs["panels"] = panels
    return kwargs

  @classmethod
  @override
  def extra_flags(cls, browser_attributes: BrowserAttributes) -> Flags:
    flags: Flags = super().extra_flags(browser_attributes)
    if browser_attributes.is_chromium_based:
      flags.set("--remote-allow-origins", "*")
      flags.set("--force-enable-metrics-reporting")
    return flags
