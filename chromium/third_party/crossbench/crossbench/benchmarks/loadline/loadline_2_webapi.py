# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import json
from typing import TYPE_CHECKING, ClassVar, Final, Sequence, Type

import numpy as np
import pandas as pd
from typing_extensions import override

from crossbench import config
from crossbench import path as pth
from crossbench.benchmarks.loading.page.combined import CombinedPage
from crossbench.benchmarks.loadline.loadline import LoadLineBenchmark, \
    LoadLineProbe
from crossbench.probes.js import JSProbe
from crossbench.probes.probe_context import ProbeContext

if TYPE_CHECKING:
  import argparse

  from crossbench.benchmarks.loading.page.base import Page
  from crossbench.browsers.attributes import BrowserAttributes
  from crossbench.flags.base import Flags
  from crossbench.probes.results import ProbeResult
  from crossbench.runner.groups.browsers import BrowsersRunGroup

# We should increase the minor version number every time there are any changes
# that might affect the benchmark score.
VERSION_STRING: Final[str] = "experimental"


class LoadLine2WebApiProbe(LoadLineProbe):
  NAME: ClassVar = "loadline2_webapi_probe"
  BENCHMARK_NAME: ClassVar = "LoadLine2_WebApi"
  BENCHMARK_VERSION: ClassVar[str] = VERSION_STRING

  @override
  def get_context_cls(self,) -> Type[LoadLine2WebApiProbeContext]:
    return LoadLine2WebApiProbeContext

  @override
  def _compute_score(self, group: BrowsersRunGroup) -> pd.DataFrame:
    js_result = group.results.get_by_name(JSProbe.NAME)
    assert js_result, f"{group} has no JSProbe result"

    with js_result.json.open() as file:
      j = json.load(file)

    timings: dict[str, list] = {
        "browser": [],
        "metric": [],
        "run": [],
        "value": []
    }
    for browser in j:
      runs = j[browser]["info"]["runs"]
      for metric in j[browser]["data"]:
        values = j[browser]["data"][metric]["values"]
        assert len(values) == runs
        for run, value in enumerate(values):
          timings["browser"].append(browser)
          timings["metric"].append(metric)
          timings["run"].append(run)
          timings["value"].append(value)

    df = pd.DataFrame.from_dict(timings).pivot(
        columns="metric", index=["browser", "run"], values="value")

    scores: dict[str, pd.Series] = {}
    if "amazonNavigation" in df:
      scores["amazon_product_visual"] = 60e3 / (
          df["amazonVisual"] - df["amazonNavigation"])
      scores["amazon_product_interactive"] = 60e3 / (
          df["amazonInteractive"] - df["amazonNavigation"])
    if "cnnNavigation" in df:
      scores["cnn_article_visual"] = 60e3 / (
          df["cnnVisual"] - df["cnnNavigation"])
      scores["cnn_article_interactive"] = 60e3 / (
          df["cnnInteractive"] - df["cnnNavigation"])
    if "wikiNavigation" in df:
      scores["wikipedia_article_visual"] = 60e3 / (
          df["wikiVisual"] - df["wikiNavigation"])
      scores["wikipedia_article_interactive"] = 60e3 / (
          df["wikiInteractive"] - df["wikiNavigation"])
    if "globoNavigation" in df:
      scores["globo_homepage_visual"] = 60e3 / (
          df["globoVisual"] - df["globoNavigation"])
      scores["globo_homepage_interactive"] = 60e3 / (
          df["globoInteractive"] - df["globoNavigation"])
    if "googleNavigation" in df:
      scores["google_search_result_visual"] = 60e3 / (
          df["googleVisual"] - df["googleNavigation"])
      scores["google_search_result_interactive"] = 60e3 / (
          df["googleInteractive"] - df["googleNavigation"])

    total = pd.DataFrame(scores)
    total["TOTAL_SCORE"] = np.exp(np.log(total).mean(axis=1))
    total = total.groupby("browser").mean().T
    total.index.name = "Metric"
    return total

  @override
  def _compute_breakdown(self, group: BrowsersRunGroup) -> pd.DataFrame:
    return pd.DataFrame(index=pd.Index([], name="Not implemented"))


class LoadLine2WebApiProbeContext(ProbeContext[LoadLine2WebApiProbe]):

  def setup(self) -> None:
    # TODO: setup network/dns?
    pass

  def start(self) -> None:
    pass

  def stop(self) -> None:
    pass

  def teardown(self) -> ProbeResult:
    return self.empty_result()


class LoadLine2WebApiBenchmark(LoadLineBenchmark):
  PROBES: ClassVar = (LoadLine2WebApiProbe,)
  DEFAULT_REPETITIONS: ClassVar = 50

  @classmethod
  def _base_dir(cls) -> pth.LocalPath:
    return config.config_dir() / "benchmark" / "loadline2"

  @classmethod
  @override
  def default_probe_config_path(cls) -> pth.LocalPath:
    return cls._base_dir() / "probe_config_webapi.hjson"

  @classmethod
  @override
  def stories_from_cli_args(cls, args: argparse.Namespace) -> Sequence[Page]:
    pages = super().stories_from_cli_args(args)
    return (CombinedPage(pages, playback=args.playback),)

  @classmethod
  @override
  def extra_flags(cls, browser_attributes: BrowserAttributes) -> Flags:
    flags: Flags = super().extra_flags(browser_attributes)
    if browser_attributes.is_chromium_based:
      # By design, Loadline2 wants some stories to always use a new renderer
      # process and some to use an existing renderer, therefore covering both
      # cases. The flag here forces a navigation to a new website to create a
      # new renderer, except when navigating from about:blank. So we can
      # achieve the goal by passing the flag and navigating to about:blank
      # before stories that must use an existing renderer.
      flags.set("--site-per-process")
    return flags


class LoadLine2WebApiPhoneBenchmark(LoadLine2WebApiBenchmark):
  """A version of LoadLine 2 benchmark that uses pure Web API (no Chromium-only
   features) to collect metrics.
  """
  NAME: ClassVar = "loadline2-webapi-phone"

  @classmethod
  @override
  def default_pages_config_path(cls) -> pth.LocalPath:
    return cls._base_dir() / "page_config_webapi_phone.hjson"

  @classmethod
  @override
  def default_network_config_path(cls) -> pth.LocalPath:
    return cls._base_dir() / "network_config_phone.hjson"

  @classmethod
  @override
  def aliases(cls) -> tuple[str, ...]:
    return ("ld2-webapi-phone",)


class LoadLine2WebApiPhoneDebugBenchmark(LoadLine2WebApiPhoneBenchmark):
  """LoadLine 2 WebAPI benchmark, with perfetto tracing for debugging.
  """
  NAME: ClassVar = "loadline2-webapi-phone-debug"
  DEFAULT_REPETITIONS: ClassVar = 1

  @classmethod
  @override
  def default_probe_config_path(cls) -> pth.LocalPath:
    return cls._base_dir() / "probe_config_webapi_debug.hjson"

  @classmethod
  @override
  def aliases(cls) -> tuple[str, ...]:
    return ("ld2-webapi-phone-debug",)
