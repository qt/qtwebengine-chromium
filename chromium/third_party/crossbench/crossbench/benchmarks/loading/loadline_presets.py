# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import argparse
import logging
from typing import TYPE_CHECKING, Optional, Sequence, Tuple, Type

import numpy as np
import pandas as pd
from tabulate import tabulate
from typing_extensions import override

from crossbench import config
from crossbench import path as pth
from crossbench.benchmarks.benchmark_probe import BenchmarkProbeMixin
from crossbench.benchmarks.loading.config.pages import PagesConfig
from crossbench.benchmarks.loading.loading_benchmark import (LoadingBenchmark,
                                                             LoadingPageFilter)
from crossbench.flags.base import Flags
from crossbench.probes.perfetto.trace_processor.trace_processor import \
    TraceProcessorProbe
from crossbench.probes.probe import Probe, ProbeContext
from crossbench.probes.results import LocalProbeResult

if TYPE_CHECKING:
  from crossbench.benchmarks.loading.page.base import Page
  from crossbench.browsers.attributes import BrowserAttributes
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.groups.browsers import BrowsersRunGroup

CONFIG_DIR: pth.LocalPath = config.config_dir()
LOADLINE_DIR: pth.LocalPath = CONFIG_DIR / "benchmark" / "loadline"

# We should increase the minor version number every time there are any changes
# that might affect the benchmark score.
VERSION_STRING = "1.1.0"


class LoadLinePageFilter(LoadingPageFilter):
  """LoadLine benchmark for phone/tablet."""
  CAN_COMBINE_STORIES: bool = False

  @classmethod
  def add_page_config_parser(cls, parser: argparse.ArgumentParser) -> None:
    pass

  @classmethod
  @override
  def default_stories(cls) -> Tuple[Page, ...]:
    return cls.all_stories()

  @classmethod
  @override
  def all_stories(cls) -> Tuple[Page, ...]:
    return ()


class LoadLineProbe(BenchmarkProbeMixin, Probe):
  IS_GENERAL_PURPOSE = False
  NAME = "loadline_probe"

  @override
  def get_context_cls(self,) -> Type[LoadLineProbeContext]:
    return LoadLineProbeContext

  @override
  def log_browsers_result(self, group: BrowsersRunGroup) -> None:
    logging.info("-" * 80)
    logging.critical("LoadLine Benchmark (%s)", VERSION_STRING)
    logging.critical("LoadLine results:")
    logging.info("- " * 40)
    logging.critical(
        tabulate(
            pd.read_csv(
                group.get_local_probe_result_path(self).with_suffix(".csv")),
            headers="keys",
            tablefmt="plain"))

  @override
  def merge_browsers(self, group: BrowsersRunGroup) -> ProbeResult:
    csv_file = group.get_local_probe_result_path(self).with_suffix(".csv")
    self._compute_score(group).to_csv(csv_file)
    return LocalProbeResult(csv=(csv_file,))

  def _compute_score(self, group: BrowsersRunGroup) -> pd.DataFrame:
    all_results = group.results.get_by_name(TraceProcessorProbe.NAME).csv_list
    loadline_result: pth.LocalPath | None = None
    for result in all_results:
      # Look for the "loadline/benchmark_score" trace processor query result.
      if result.name == "loadline_benchmark_score.csv":
        loadline_result = result
        break
    assert loadline_result is not None, "LoadLine: query result not found"

    df = pd.read_csv(loadline_result)
    df = df.groupby(["cb_browser",
                     "cb_story"])["score"].mean().reset_index().pivot(
                         columns=["cb_story"],
                         index=["cb_browser"],
                         values=["score"])
    df = df.droplevel(0, axis=1)
    df["TOTAL_SCORE"] = np.exp(np.log(df).mean(axis=1))
    df.index.rename("browser", inplace=True)
    return df.reindex(
        columns=(["TOTAL_SCORE"] +
                 sorted(list(c for c in df.columns if c != "TOTAL_SCORE"))))


class LoadLineProbeContext(ProbeContext[LoadLineProbe]):

  def start(self) -> None:
    pass

  @override
  def start_story_run(self) -> None:
    benchmark_type = ("loadline-phone" if "phone" in self.probe.benchmark.NAME
                      else "loadline-tablet")
    self.browser.performance_mark(
        f"LoadLine/{benchmark_type}/{self.run.story.name}")

  def stop(self) -> None:
    pass

  def teardown(self) -> ProbeResult:
    return self.empty_result()


class LoadLineBenchmark(LoadingBenchmark, metaclass=abc.ABCMeta):
  STORY_FILTER_CLS = LoadLinePageFilter
  PROBES = (LoadLineProbe,)
  DEFAULT_REPETITIONS = 100

  _page_config: PagesConfig | None = None

  @classmethod
  @override
  def requires_separate(cls, args: argparse.Namespace) -> bool:
    # Perfetto metrics used in the benchmark require a separate Perfetto
    # session for each run.
    return True

  @classmethod
  def default_probe_config_path(cls) -> pth.LocalPath:
    return pth.LocalPath(LOADLINE_DIR) / "probe_config.hjson"

  @classmethod
  @abc.abstractmethod
  @override
  def default_network_config_path(cls) -> pth.LocalPath:
    pass

  @classmethod
  @abc.abstractmethod
  def default_pages_config_path(cls) -> pth.LocalPath:
    pass

  @classmethod
  @override
  def get_pages_config(
      cls, args: Optional[argparse.Namespace] = None) -> PagesConfig:
    # Use manual caching, since args is not hashable.
    if cls._page_config is None:
      cls._page_config = PagesConfig.parse(cls.default_pages_config_path())
    return cls._page_config

  @classmethod
  @override
  def all_story_names(cls) -> Sequence[str]:
    return tuple(page.any_label for page in cls.get_pages_config().pages)


class LoadLinePhoneBenchmark(LoadLineBenchmark):
  """LoadLine benchmark for phones.
  """
  NAME = "loadline-phone"

  @classmethod
  @override
  def default_pages_config_path(cls) -> pth.LocalPath:
    return pth.LocalPath(LOADLINE_DIR) / "page_config_phone.hjson"

  @classmethod
  @override
  def default_network_config_path(cls) -> pth.LocalPath:
    return pth.LocalPath(LOADLINE_DIR) / "network_config_phone.hjson"

  @classmethod
  @override
  def aliases(cls) -> Tuple[str, ...]:
    return ("loading-phone", "load-phone", "ld-phone")


class LoadLineTabletBenchmark(LoadLineBenchmark):
  """LoadLine benchmark for tablets.
  """
  NAME = "loadline-tablet"

  @classmethod
  @override
  def default_pages_config_path(cls) -> pth.LocalPath:
    return pth.LocalPath(LOADLINE_DIR) / "page_config_tablet.hjson"

  @classmethod
  @override
  def default_network_config_path(cls) -> pth.LocalPath:
    return pth.LocalPath(LOADLINE_DIR) / "network_config_tablet.hjson"

  @classmethod
  @override
  def aliases(cls) -> Tuple[str, ...]:
    return ("loading-tablet", "load-tablet", "ld-tablet")

  @classmethod
  @override
  def extra_flags(cls, browser_attributes: BrowserAttributes) -> Flags:
    assert browser_attributes.is_chromium_based
    return Flags(["--request-desktop-sites"])


class LoadLinePhoneDebugBenchmark(LoadLinePhoneBenchmark):
  """LoadLine benchmark for phones, with more tracing categories, for easier
  performance analysis.
  """
  NAME = "loadline-phone-debug"
  DEFAULT_REPETITIONS = 1

  @classmethod
  @override
  def default_probe_config_path(cls) -> pth.LocalPath:
    return (pth.LocalPath(LOADLINE_DIR) /
                "probe_config_experimental_lightweight.hjson")

  @classmethod
  @override
  def aliases(cls) -> Tuple[str, ...]:
    return ("loading-phone-debug", "load-phone-debug", "ld-phone-debug")


class LoadLineTabletDebugBenchmark(LoadLineTabletBenchmark):
  """LoadLine benchmark for tablets, with more tracing categories, for easier
  performance analysis.
  """
  NAME = "loadline-tablet-debug"
  DEFAULT_REPETITIONS = 1

  @classmethod
  @override
  def default_probe_config_path(cls) -> pth.LocalPath:
    return (pth.LocalPath(LOADLINE_DIR) /
                "probe_config_experimental_lightweight.hjson")

  @classmethod
  @override
  def aliases(cls) -> Tuple[str, ...]:
    return ("loading-tablet-debug", "load-tablet-debug", "ld-tablet-debug")


class LoadLinePhoneFastBenchmark(LoadLinePhoneBenchmark):
  """LoadLine benchmark for phones, with less repetitions, for faster local
  experiments.
  """
  NAME = "loadline-phone-fast"
  DEFAULT_REPETITIONS = 10

  @classmethod
  @override
  def aliases(cls) -> Tuple[str, ...]:
    return ("loading-phone-fast", "load-phone-fast", "ld-phone-fast")


class LoadLineTabletFastBenchmark(LoadLineTabletBenchmark):
  """LoadLine benchmark for tablets, with less repetitions, for faster local
  experiments.
  """
  NAME = "loadline-tablet-fast"
  DEFAULT_REPETITIONS = 10

  @classmethod
  @override
  def aliases(cls) -> Tuple[str, ...]:
    return ("loading-tablet-fast", "load-tablet-fast", "ld-tablet-fast")
