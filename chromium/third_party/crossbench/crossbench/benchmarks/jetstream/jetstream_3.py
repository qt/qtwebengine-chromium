# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.benchmarks.jetstream.jetstream_2 import JetStream2Benchmark, \
    JetStream2BenchmarkStoryFilter, JetStream2Probe, JetStream2ProbeContext, \
    JetStream2Story
from crossbench.helper import url_helper

if TYPE_CHECKING:
  import argparse

  from crossbench.runner.actions import Actions


# TODO: introduce JetStreamProbe
class JetStream3Probe(JetStream2Probe, metaclass=abc.ABCMeta):
  """
  JetStream3-specific Probe.
  Extracts all JetStream 3 times and scores.
  """


class JetStream3ProbeContext(JetStream2ProbeContext):
  pass


# TODO: introduce JetStreamStory
class JetStream3Story(JetStream2Story, metaclass=abc.ABCMeta):
  SUBSTORIES: ClassVar[tuple[str, ...]] = ()

  @property
  @override
  def url_params(self) -> dict[str, str]:
    params: dict[str, str] = super().url_params
    if self.substories != self.SUBSTORIES:
      params["test"] = ",".join(self.substories)
    return params

  @property
  @override
  def test_url(self) -> str:
    params: dict[str, str] = self.url_params
    params["developerMode"] = ""
    params["startAutomatically"] = ""
    official_test_url = url_helper.update_url_query(self.URL, params)
    return official_test_url

  @override
  def setup_stories(self, actions: Actions) -> None:
    pass


ProbeClsTupleT = tuple[Type[JetStream3Probe], ...]


class JetStream3BenchmarkStoryFilter(JetStream2BenchmarkStoryFilter):
  __doc__ = JetStream2BenchmarkStoryFilter.__doc__

  @classmethod
  @override
  def add_cli_arguments(
      cls, parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    parser = super().add_cli_arguments(parser)
    parser.add_argument(
        "--no-prefetch",
        dest="prefetch_resources",
        default=True,
        action="store_false",
        help=("Disable resources prefetching for better source positions. "
              "This might skew results as we do network request on the hot"
              "path"))
    return parser

  @classmethod
  def url_params_from_cli(cls, args: argparse.Namespace) -> dict[str, str]:
    url_params: dict[str, str] = super().url_params_from_cli(args)
    if not args.prefetch_resources:
      url_params["prefetchResources"] = "false"
    return url_params


# TODO: introduce JetStreamBenchmark
class JetStream3Benchmark(JetStream2Benchmark):
  STORY_FILTER_CLS: ClassVar = JetStream3BenchmarkStoryFilter
