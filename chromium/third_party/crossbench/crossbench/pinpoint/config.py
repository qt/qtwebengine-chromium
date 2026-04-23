# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import dataclasses
import re
from typing import Any

from crossbench.cli import ui
from crossbench.cli.config.flags import FlagsConfig
from crossbench.config import ConfigObject, ConfigParser
from crossbench.parse import NumberParser
from crossbench.pinpoint import patch_resolver
from crossbench.pinpoint.helper import annotate
from crossbench.pinpoint.list_benchmarks import fetch_benchmarks
from crossbench.pinpoint.list_bots import fetch_bots
from crossbench.pinpoint.list_builds import fetch_builds
from crossbench.pinpoint.list_stories import fetch_stories


@dataclasses.dataclass()
class VariantConfig(ConfigObject):
  """Represents one arm of an A/B test (e.g., base or experiment)."""

  commit: str = "HEAD"
  patch: str | None = None
  flags: FlagsConfig = dataclasses.field(default_factory=FlagsConfig)

  @classmethod
  def config_parser(cls) -> ConfigParser[VariantConfig]:
    parser = ConfigParser(cls)
    parser.add_argument(
        "commit",
        default="HEAD",
        type=cls.parse_commit,
        help="Git commit hash for the build. Accepts a full commit hash, "
        "'HEAD' (latest commit), or 'recent' (the most recent build).")
    parser.add_argument(
        "patch",
        type=cls.parse_patch,
        help="Gerrit patch to apply to the commit. Supported formats: "
        "'12345' (optional patchset: '12345/6'), 'c/12345', "
        "'crrev/c/12345', 'crrev/12345', 'crrev.com/c/12345' "
        "'crrev.com/12345', or a full URL. "
        "Note: All patches must be for chromium-review; "
        "chrome-internal-review is not supported.")
    parser.add_argument(
        "flags",
        type=FlagsConfig,
        default=FlagsConfig(),
        help="Chrome flags forwarded to the browser.")
    return parser

  @classmethod
  def parse_commit(cls, value: str) -> str:
    if value.upper() in ("HEAD", "-HEAD", ""):
      return "HEAD"
    if value.lower() == "recent":
      return "recent"
    if re.match(r"^[0-9a-fA-F]{8,40}$", value):
      return value.lower()
    raise ValueError(f"Invalid commit value: {value}")

  @classmethod
  def parse_patch(cls, value: str) -> str:
    return patch_resolver.resolve_patch(value)

  @classmethod
  def parse_str(cls, value: str) -> VariantConfig:
    raise NotImplementedError

  def override_commit(self, commit: str | None, bot: str) -> None:
    self.commit = self.parse_commit(commit or self.commit)
    if self.commit == "recent":
      self.commit = fetch_builds(bot)[0].commit

  def override_patch(self, patch: str | None) -> None:
    if patch:
      self.patch = self.parse_patch(patch)

  def override_flags(self,
                     js_flags: str | None = None,
                     enable_features: str | None = None,
                     disable_features: str | None = None) -> None:
    input_flags = {
        "--js-flags": js_flags,
        "--enable-features": enable_features,
        "--disable-features": disable_features,
    }
    filtered_flags = {k: v for k, v in input_flags.items() if v is not None}
    combined_flags = self.flags_as_dict() | filtered_flags
    if combined_flags_str := self.flags_dict_to_str(combined_flags):
      self.flags = FlagsConfig.parse(combined_flags_str)

  def flags_as_dict(self) -> dict[str, str | None]:
    if not self.flags.get("default"):
      return {}
    return dict(self.flags["default"][0].flags.items())

  def flags_as_str(self) -> str | None:
    flags = self.flags_as_dict()
    return self.flags_dict_to_str(flags)

  @classmethod
  def flags_dict_to_str(cls, flags: dict[str, str | None]) -> str | None:
    if not flags:
      return None
    parts = []
    for flag, value in flags.items():
      parts.append(f"{flag}={value}" if value is not None else flag)
    return " ".join(parts)

  def extra_browser_flags(self) -> str | None:
    flags = self.flags_as_str()
    if not flags:
      return None
    return f'--extra-browser-args="{flags}"'


@dataclasses.dataclass()
class PinpointTryJobConfig(ConfigObject):
  """Representation of a Pinpoint "try job" configuration."""

  benchmark: str
  bot: str
  story: str | None = None
  story_tags: str | None = None
  base: VariantConfig = dataclasses.field(default_factory=VariantConfig)
  experiment: VariantConfig = dataclasses.field(default_factory=VariantConfig)
  repeat: int = 100
  bug: int | None = None

  @classmethod
  def config_parser(cls) -> ConfigParser[PinpointTryJobConfig]:
    parser = ConfigParser(cls)
    parser.add_argument(
        "benchmark",
        type=str,
        help="Benchmark name (e.g., 'speedometer3').",
    )
    parser.add_argument(
        "bot",
        type=str,
        help="The bot configuration to run on (e.g., 'linux-perf')")
    parser.add_argument(
        "story",
        type=str,
        help="Optional story to run within the benchmark. "
        "Obtained automatically for the given benchmark if not specified.",
    )
    parser.add_argument(
        "story_tags",
        type=str,
        help="Optional story tags to filter stories. "
        "Required if no story can be obtained automatically.",
    )
    parser.add_argument(
        "base",
        type=VariantConfig,
        default=VariantConfig(),
        help="Configuration for the base variant of the A/B test.")
    parser.add_argument(
        "experiment",
        type=VariantConfig,
        default=VariantConfig(),
        help="Configuration for the experiment variant of the A/B test.")
    parser.add_argument(
        "repeat",
        type=NumberParser.positive_int,
        default=100,
        help="The number of times to repeat the experiment.")
    parser.add_argument(
        "bug",
        type=NumberParser.positive_int,
        help="Optional bug ID associated with the job.")
    return parser

  @classmethod
  def parse_str(cls, value: str) -> PinpointTryJobConfig:
    raise NotImplementedError

  @classmethod
  def parse_and_override(
      cls,
      config: str | None = None,
      benchmark: str | None = None,
      bot: str | None = None,
      story: str | None = None,
      story_tags: str | None = None,
      repeat: int | None = None,
      bug: int | None = None,
      base_commit: str | None = None,
      exp_commit: str | None = None,
      base_patch: str | None = None,
      exp_patch: str | None = None,
      base_js_flags: str | None = None,
      exp_js_flags: str | None = None,
      base_enable_features: str | None = None,
      exp_enable_features: str | None = None,
      base_disable_features: str | None = None,
      exp_disable_features: str | None = None,
  ) -> PinpointTryJobConfig:
    """Create a new valid PinpointTryJobConfig instance for new jobs."""
    with annotate("Parsing job configuration"):
      if config:
        parsed = super().parse(config)
      else:
        parsed = PinpointTryJobConfig(benchmark="", bot="")

      warnings = [
          parsed.override_benchmark(benchmark),
          parsed.override_bot(bot),
          parsed.override_story(story),
      ]

      parsed.story_tags = story_tags or parsed.story_tags
      if not parsed.story and not parsed.story_tags:
        raise ValueError("Story or story_tags must be specified.")

      if repeat is not None:
        parsed.repeat = repeat
      if bug is not None:
        parsed.bug = bug

      parsed.base.override_commit(base_commit, bot=parsed.bot)
      parsed.base.override_patch(base_patch)

      parsed.experiment.override_commit(exp_commit, bot=parsed.bot)
      parsed.experiment.override_patch(exp_patch)

      parsed.base.override_flags(
          js_flags=base_js_flags,
          enable_features=base_enable_features,
          disable_features=base_disable_features,
      )
      parsed.experiment.override_flags(
          js_flags=exp_js_flags,
          enable_features=exp_enable_features,
          disable_features=exp_disable_features,
      )

    show_warnings([w for w in warnings if w])

    return parsed

  def override_benchmark(self, benchmark: str | None) -> str | None:
    if benchmark:
      self.benchmark = benchmark
    if not self.benchmark:
      raise ValueError("Benchmark is required.")
    if self.benchmark not in fetch_benchmarks():
      return f"Unknown benchmark: {self.benchmark}"
    return None

  def override_bot(self, bot: str | None) -> str | None:
    if bot:
      self.bot = bot
    if not self.bot:
      raise ValueError("Bot is required.")
    if self.bot not in fetch_bots():
      return f"Unknown bot: {self.bot}"
    return None

  def override_story(self, story: str | None) -> str | None:
    if story:
      self.story = story

    stories = fetch_stories(self.benchmark)
    if not self.story and len(stories) == 1:
      self.story = stories[0]

    if self.story not in stories:
      return f"Unknown story: {self.story}"
    return None

  def to_request_dict(self) -> dict[str, Any]:
    return {
        "comparison_mode": "try",
        "benchmark": self.benchmark,
        "configuration": self.bot,
        "story": self.story,
        "story_tags": self.story_tags,
        "initial_attempt_count": self.repeat,
        "bug_id": self.bug,
        "base_git_hash": self.base.commit,
        "end_git_hash": self.experiment.commit,
        "base_patch": self.base.patch,
        "experiment_patch": self.experiment.patch,
        "base_extra_args": self.base.extra_browser_flags(),
        "experiment_extra_args": self.experiment.extra_browser_flags(),
    }

  @classmethod
  def from_response_dict(cls, raw_dict: dict[str, Any]) -> PinpointTryJobConfig:
    """Returns a valid PinpointTryJobConfig if the server response is valid."""
    comparison_mode = raw_dict.get("comparison_mode")
    if comparison_mode != "try":
      raise ValueError(
          'Invalid comparison mode {comparison_mode} expected "try".')
    arguments = raw_dict["arguments"]

    def value_or_none(value: Any) -> Any:
      return value if value else None

    # An empty field created from the Web UI is an empty string. Such empty
    # fields are replaced with None to make it possible to convert the result
    # config to JSON and back to PinpointTryJobConfig for creating new jobs.
    return PinpointTryJobConfig(
        benchmark=arguments["benchmark"],
        bot=arguments["configuration"],
        story=value_or_none(arguments.get("story")),
        story_tags=value_or_none(arguments.get("story_tags")),
        repeat=int(arguments["initial_attempt_count"]),
        bug=value_or_none(arguments.get("bug_id")),
        base=VariantConfig(
            commit=arguments.get("base_git_hash"),
            patch=value_or_none(arguments.get("base_patch")),
            flags=cls.parse_extra_browser_args(
                arguments.get("base_extra_args")),
        ),
        experiment=VariantConfig(
            commit=arguments.get("end_git_hash"),
            patch=value_or_none(arguments.get("experiment_patch")),
            flags=cls.parse_extra_browser_args(
                arguments.get("experiment_extra_args")),
        ),
    )

  @classmethod
  def parse_extra_browser_args(cls, extra_browser_args: str) -> FlagsConfig:
    if not extra_browser_args:
      return FlagsConfig()
    if match := re.search(r'--extra-browser-args="(.*?)"', extra_browser_args):
      return FlagsConfig.parse(match.group(1))
    return FlagsConfig.parse(extra_browser_args)

  def to_dict(self) -> dict[str, Any]:
    result = dataclasses.asdict(self)
    result["base"]["flags"] = self.base.flags_as_str()
    result["experiment"]["flags"] = self.experiment.flags_as_str()
    return result


def show_warnings(warnings: list[str]) -> None:
  if not warnings:
    return

  answer = ui.prompt("\n".join(["Warnings:", *warnings, "Continue?"]), "[Y/n] ")
  if answer.lower().strip() not in ["", "y", "yes"]:
    raise ValueError("Invalid job configuration.")
