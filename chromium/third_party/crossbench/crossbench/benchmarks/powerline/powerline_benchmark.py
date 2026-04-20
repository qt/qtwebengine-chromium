# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
from typing import TYPE_CHECKING, Any, ClassVar, Final, Optional, Sequence

from typing_extensions import override

from crossbench import config
from crossbench.action_runner.action.enums import ReadyState
from crossbench.benchmarks.base import Benchmark
from crossbench.cli.ui import timer
from crossbench.flags.base import Flags
from crossbench.parse import DurationParser
from crossbench.stories.story import Story

if TYPE_CHECKING:
  import argparse

  from crossbench import path as pth
  from crossbench.action_runner.config import ActionRunnerConfig
  from crossbench.browsers.attributes import BrowserAttributes
  from crossbench.cli.parser import CrossBenchArgumentParser
  from crossbench.cli.types import Subparsers
  from crossbench.runner.run import Run


UNMUTE_AUDIO_SCRIPT: Final[str] = """
  document.getElementById('unmuteButton').click();
"""

REMUTE_AUDIO_SCRIPT: Final[str] = """
  document.getElementById('audio').muted = true;
"""


class PowerlineStory(Story):
  STORY_NAME = "podcast_vorbis_remute"
  URL_BASE = "https://chromium-workloads.web.app/web-tests/main/synthetic/powerline/"
  STORY_URLS = {
      "podcast_vorbis": "podcast-vorbis.html",
      "podcast_vorbis_muted": "podcast-vorbis.html",
      "podcast_opus": "podcast-opus.html",
      "podcast_mp3": "podcast-mp3.html",
      "podcast_aac": "podcast-aac.html"
  }

  def __init__(self,
               story_name: str,
               duration: Optional[dt.timedelta] = dt.timedelta()):
    duration = (duration or dt.timedelta(seconds=60))
    super().__init__(story_name, duration)

  def run(self, run: Run) -> None:
    with timer():
      with run.actions("Show URL") as actions:
        actions.show_url(self.url_from_story(run.story.name))
      with run.actions("Autoplay") as actions:
        actions.wait_for_ready_state(
          ReadyState.COMPLETE, timeout=dt.timedelta(seconds=5)
        )
        actions.js(UNMUTE_AUDIO_SCRIPT)
      if self.should_remute(run.story.name):
        with run.actions("Remute") as actions:
          actions.wait(dt.timedelta(seconds=5))
          actions.js(REMUTE_AUDIO_SCRIPT)
      with run.actions("Screen") as actions:
        actions.wait(dt.timedelta(seconds=5))
        with actions.platform.low_power_mode():
          # Put the screen to sleep and enter simulated Doze
          actions.wait(self.duration)
      logging.info("Stopping benchmark...")

  @classmethod
  def url_from_story(cls, name: str) -> str:
    return cls.URL_BASE + cls.STORY_URLS[name]

  @classmethod
  def should_remute(cls, name: str) -> bool:
    return name.endswith("_muted")

  @classmethod
  @override
  def all_story_names(cls) -> Sequence[str]:
    return sorted(cls.STORY_URLS)


class PowerlineBenchmark(Benchmark):
  """
  Benchmark runner for the Powerline background power-consumption test.

  This test opens up an HTML5 page which plays an audio, intended to simulate
  listening to a podcast with the screen off. The test measures the CPU power
  consumption on the Pixel power rails via Perfetto.
  """
  NAME: ClassVar = "powerline"
  DEFAULT_STORY_CLS: ClassVar = PowerlineStory

  # TODO: we may want to check somehow that the device is a Pixel and therefore
  # has meaningful power rails we can read.

  def __init__(self,
               action_runner_config: Optional[ActionRunnerConfig] = None,
               run_for: Optional[dt.timedelta] = None) -> None:
    stories = [
        PowerlineStory(x, run_for) for x in PowerlineStory.all_story_names()
    ]
    super().__init__(stories, action_runner_config)

  @classmethod
  def _base_dir(cls) -> pth.LocalPath:
    return config.config_dir() / "benchmark" / "powerline"

  @classmethod
  @override
  def default_probe_config_path(cls) -> pth.LocalPath:
    return cls._base_dir() / "probe_config.hjson"

  @classmethod
  @override
  def extra_flags(cls, browser_attributes: BrowserAttributes) -> Flags:
    # This flag is required because Chrome a) does not autoplay based on the
    #  HTML5 tag and b) it will not play from JavaScript if the user does not
    # interact with the page first. https://developer.chrome.com/blog/autoplay
    assert browser_attributes.is_chromium_based
    return Flags({
      "--autoplay-policy": "no-user-gesture-required",
      "--enable-renderer-backgrounding": None,
      "--enable-background-timer-throttling": None
    })

  @classmethod
  @override
  def add_cli_parser(cls, subparsers: Subparsers) -> CrossBenchArgumentParser:
    parser = super().add_cli_parser(subparsers)
    parser.add_argument(
        "--run-for",
        "--stop-after",
        "--duration",
        type=DurationParser.positive_duration,
        help="How long to run the power measurements for")
    return parser

  @classmethod
  @override
  def kwargs_from_cli(cls, args: argparse.Namespace) -> dict[str, Any]:
    kwargs = super().kwargs_from_cli(args)
    kwargs["run_for"] = args.run_for
    return kwargs
