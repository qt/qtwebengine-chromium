# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import argparse
from typing import TYPE_CHECKING, Final

from immutabledict import immutabledict
from typing_extensions import override

from crossbench import path as pth
from crossbench.cli.parser import CrossBenchArgumentParser
from crossbench.cli.subcommand.base import CrossbenchSubcommand
from crossbench.parse import NumberParser
from crossbench.pinpoint.cancel_job import cancel_job
from crossbench.pinpoint.config import PinpointTryJobConfig
from crossbench.pinpoint.job_config import print_job_config
from crossbench.pinpoint.job_parser import parse_job_id
from crossbench.pinpoint.job_results import download_results
from crossbench.pinpoint.list_benchmarks import fetch_benchmarks
from crossbench.pinpoint.list_bots import fetch_bots
from crossbench.pinpoint.list_builds import list_builds
from crossbench.pinpoint.list_format import ListFormatEnum
from crossbench.pinpoint.list_jobs import list_jobs
from crossbench.pinpoint.list_stories import fetch_stories
from crossbench.pinpoint.start_job import start_job
from crossbench.pinpoint.user import UserEnum, list_user

if TYPE_CHECKING:
  from crossbench.cli.cli import BenchmarkClass, CrossBenchCLI
  from crossbench.cli.types import Subparsers

# Crossbench benchmarks without corresponding pinpoint benchmarks are None.
_PINPOINT_BENCHMARK_BY_CROSSBENCH_NAME: Final[immutabledict[
    str, str | None]] = immutabledict({
        "devtools_frontend": None,
        "embedder": "embedder.crossbench",
        "jetstream_1.1": "jetstream",
        "jetstream_2.0": "jetstream2.0.crossbench",
        "jetstream_2.1": "jetstream2.1.crossbench",
        "jetstream_2.2": "jetstream2.2.crossbench",
        "jetstream_main": "jetstream-main.crossbench",
        "loading": "loading.crossbench",
        "loadline-phone": "loadline_phone.crossbench",
        "loadline-phone-debug": None,
        "loadline-phone-fast": None,
        "loadline-tablet": "loadline_tablet.crossbench",
        "loadline-tablet-debug": None,
        "loadline-tablet-fast": None,
        "loadline2-phone": None,
        "loadline2-phone-debug": None,
        "loadline2-tablet": None,
        "loadline2-tablet-debug": None,
        "loadline2-webapi-phone": None,
        "loadline2-webapi-phone-debug": None,
        "manual": None,
        "memory": "memory.desktop",
        "motionmark_1.0": "motionmark1.0.crossbench",
        "motionmark_1.1": "motionmark1.1.crossbench",
        "motionmark_1.2": "motionmark1.2.crossbench",
        "motionmark_1.3": "motionmark1.3.crossbench",
        "motionmark_1.3.1": "motionmark1.3.1.crossbench",
        "motionmark_main": None,
        "powerline": None,
        "speedometer_1.0": "speedometer",
        "speedometer_2.0": "speedometer2.0.crossbench",
        "speedometer_2.1": "speedometer2.1.crossbench",
        "speedometer_3.0": "speedometer3.0.crossbench",
        "speedometer_3.1": "speedometer3.1.crossbench",
        "speedometer_main": "speedometer-main.crossbench",
    })


class PinpointBaseSubcommand(abc.ABC):

  def __init__(self, parent: PinpointSubcommand) -> None:
    self._parent = parent
    self._parser = self.add_cli_parser()
    self._parser.set_defaults(pinpoint_subcommand=self)

  @abc.abstractmethod
  def add_cli_parser(self) -> argparse.ArgumentParser:
    pass

  @abc.abstractmethod
  def run(self, args: argparse.Namespace) -> None:
    pass


class PinpointListSubcommand(PinpointBaseSubcommand):
  """A subcommand for interacting with the Pinpoint service."""

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    list_parser = self._parent.subparsers.add_parser(
        "list", aliases=("ls",), help="Displays recent Pinpoint jobs.")
    list_parser.add_argument(
        "--user",
        "-u",
        type=list_user,
        default=UserEnum.ME,
        help=("Filter jobs by user. 'me' (default) shows jobs for your "
              "@google.com and @chromium.org accounts, derived from your "
              "authenticated username. 'all' shows jobs from all users. "
              "An email address can also be specified. Note: 'me' might not "
              "work correctly if your usernames differ across domains."))
    list_parser.add_argument(
        "--number",
        "-n",
        type=NumberParser.positive_int,
        default=20,
        help="The maximum number of jobs to fetch and display. (default: 20)")
    list_parser.add_argument(
        "--format",
        "-f",
        choices=[
            ListFormatEnum.TABLE, ListFormatEnum.JSON, ListFormatEnum.YAML,
            ListFormatEnum.CSV, ListFormatEnum.TSV
        ],
        default=ListFormatEnum.TABLE,
        help="The output format for the list of jobs. (default: table)")
    list_parser.add_argument(
        "--truncate",
        "-t",
        type=NumberParser.positive_int,
        default=None,
        help=("Truncate cell content to the specified maximum length. "
              "Only applies to the 'table' format."))
    return list_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    list_jobs(args.user, args.number, args.truncate, args.format)


class PinpointJobSubcommand(PinpointBaseSubcommand):
  """Base class for subcommands that operate on a Pinpoint job."""

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    parser = self.create_parser()
    parser.add_argument(
        "--job",
        required=True,
        type=parse_job_id,
        help="The ID of the job. Can be a full URL, a part of a URL with a job "
        "ID, or just the ID.")
    return parser

  @abc.abstractmethod
  def create_parser(self) -> argparse.ArgumentParser:
    raise NotImplementedError


class PinpointConfigSubcommand(PinpointJobSubcommand):
  """Get the configuration of a specific Pinpoint job."""

  @override
  def create_parser(self) -> argparse.ArgumentParser:
    config_parser = self._parent.subparsers.add_parser(
        "config",
        aliases=("cfg",),
        help="Get the configuration of a specific Pinpoint job.")
    config_parser.add_argument(
        "--raw",
        action="store_true",
        help="Display the job configuration as raw JSON response from the "
        "server.")
    config_parser.add_argument(
        "--full",
        action="store_true",
        help="Display the full job configuration including all attemps. "
        "Works only if the `--raw` flag is set.")
    return config_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    print_job_config(job_id=args.job, raw=args.raw, full=args.full)


class PinpointBaseStartSubcommand(PinpointBaseSubcommand):
  """Base class for subcommands that start a new Pinpoint job."""

  def create_parser(self,
                    command: str,
                    aliases: tuple[str, ...] = (),
                    help_text: str | None = None) -> argparse.ArgumentParser:
    start_parser = self._parent.subparsers.add_parser(
        command,
        aliases=aliases,
        help=help_text,
        description="Starts a new Pinpoint A/B job. "
        "This command allows you to specify two configurations (base and "
        "experiment) to compare performance between them.",
        formatter_class=argparse.RawTextHelpFormatter)
    start_parser.add_argument(
        "--config",
        help="A/B job configuration in the JSON/HJSON format. "
        "Accepts a path to a configuration file, or configuration string. "
        "If the same argument is specified in the config and then provided "
        "as a command line argument, the latter overrides the former. "
        "Get more information by running `describe PinpointTryJobConfig`")
    self.add_benchmark_flag(start_parser)
    start_parser.add_argument(
        "--bot", help="The bot configuration to run on (e.g., 'linux-perf').")
    start_parser.add_argument("--story", help="The story to run.")
    start_parser.add_argument(
        "--story-tags", dest="story_tags", help="Story tags to filter stories.")
    start_parser.add_argument(
        "--repeat",
        type=NumberParser.positive_int,
        help="How many times to repeat the experiment.")
    start_parser.add_argument(
        "--bug",
        type=NumberParser.positive_int,
        help="The bug ID to associate with the job.")
    start_parser.add_argument(
        "--commit",
        help="Git commit hash for both base and experiment builds. "
        "Accepts a commit hash, 'HEAD' (latest commit), or 'recent' "
        "(the most recent build). Defaults to HEAD. "
        "Can be overridden by --base-commit or --exp-commit.")
    start_parser.add_argument(
        "--base-commit",
        help="Git commit hash for the base build. Accepts a commit hash, "
        "'HEAD' (latest commit), or 'recent' (the most recent build). "
        "Defaults to HEAD.")
    start_parser.add_argument(
        "--exp-commit",
        help="Git commit hash for the experiment build. Accepts a commit hash, "
        "'HEAD' (latest commit), or 'recent' (the most recent build).")
    start_parser.add_argument(
        "--base-patch",
        help="Gerrit patch to apply to the base commit. Supported formats: "
        "'12345' (optional patchset: '12345/6'), 'c/12345', "
        "'crrev/c/12345', 'crrev/12345', 'crrev.com/c/12345', "
        "'crrev.com/12345', or a full URL. "
        "Note: All patches must be for chromium-review; "
        "chrome-internal-review is not supported.")
    start_parser.add_argument(
        "--exp-patch",
        "--patch",
        help="Gerrit patch to apply to the experiment commit. Supported "
        "formats: '12345' (optional patchset: '12345/6'), 'c/12345', "
        "'crrev/c/12345', 'crrev/12345', 'crrev.com/c/12345', "
        "'crrev.com/12345', or a full URL. "
        "Note: All patches must be for chromium-review; "
        "chrome-internal-review is not supported.")

    # Extra browser args.
    start_parser.add_argument(
        "--js-flags",
        help="JavaScript flags to pass to V8 for both base and experiment "
        "commits. Can be overridden by --base-js-flags or --exp-js-flags.\n"
        "Example: --js-flags=--turbolev-future")
    start_parser.add_argument(
        "--base-js-flags",
        help="JavaScript flags to pass to V8 for the base commit.\n"
        "Example: --base-js-flags=--turbolev-future")
    start_parser.add_argument(
        "--exp-js-flags",
        help="JavaScript flags to pass to V8 for the experiment commit.\n"
        "Example: --exp-js-flags=--turbolev-future")
    start_parser.add_argument(
        "--enable-features",
        help="Chrome features to enable for both base and experiment commits. "
        "Can be overridden by --base-enable-features or --exp-enable-features."
        "\nExample: --enable-features=Feature1,Feature2")
    start_parser.add_argument(
        "--base-enable-features",
        help="Comma-separated list of Chrome features to enable for the base "
        "commit.\nExample: --base-enable-features=Feature1,Feature2")
    start_parser.add_argument(
        "--exp-enable-features",
        help="Comma-separated list of Chrome features to enable for the "
        "experiment commit.\nExample: --exp-enable-features=FeatureA,FeatureB")
    start_parser.add_argument(
        "--disable-features",
        help="Chrome features to disable for both base and experiment "
        "commits. Can be overridden by --base-disable-features or "
        "--exp-disable-features.\nExample: --disable-features=Feature1,Feature2"
    )
    start_parser.add_argument(
        "--base-disable-features",
        help="Comma-separated list of Chrome features to disable for the base "
        "commit.\nExample: --base-disable-features=Feature1,Feature2")
    start_parser.add_argument(
        "--exp-disable-features",
        help="Comma-separated list of Chrome features to disable for the "
        "experiment commit.\nExample: --exp-disable-features=FeatureA,FeatureB")

    return start_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    config = PinpointTryJobConfig.parse_and_override(
        config=args.config,
        benchmark=self.get_benchmark(args),
        bot=args.bot,
        story=args.story,
        story_tags=args.story_tags,
        repeat=args.repeat,
        bug=args.bug,
        base_commit=args.base_commit or args.commit,
        exp_commit=args.exp_commit or args.commit,
        base_patch=args.base_patch,
        exp_patch=args.exp_patch,
        base_js_flags=args.base_js_flags or args.js_flags,
        exp_js_flags=args.exp_js_flags or args.js_flags,
        base_enable_features=args.base_enable_features or args.enable_features,
        exp_enable_features=args.exp_enable_features or args.enable_features,
        base_disable_features=args.base_disable_features or
        args.disable_features,
        exp_disable_features=args.exp_disable_features or args.disable_features,
    )
    start_job(config)

  def add_benchmark_flag(self, parser: argparse.ArgumentParser) -> None:
    pass

  @abc.abstractmethod
  def get_benchmark(self, args: argparse.Namespace) -> str | None:
    pass


class PinpointStartSubcommand(PinpointBaseStartSubcommand):
  """Starts a new Pinpoint A/B job."""

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    start_parser = self.create_parser(
        "start", help_text="Starts a new Pinpoint A/B job.")
    start_parser.epilog = """Example:
  pinpoint start \\
    --config=config.json \\
    --benchmark=speedometer3.crossbench \\
    --bot=linux-r350-perf \\
    --story=default \\
    --story-tags=mobile,desktop \\
    --repeat=20 \\
    --bug-id=123456 \\
    --base-commit=HEAD \\
    --exp-commit=recent \\
    --base-patch-url=https://chromium-review.googlesource.com/c/v8/v8/+/12345 \\
    --exp-patch-url=https://chromium-review.googlesource.com/c/v8/v8/+/67890 \\
    --base-js-flags=--flag1,--flag2 \\
    --exp-js-flags=--flag3,--flag4 \\
    --base-enable-features=feature1,feature2 \\
    --exp-enable-features=feature3,feature4 \\
    --base-disable-features=feature5,feature6 \\
    --exp-disable-features=feature7,feature8
"""
    return start_parser

  @override
  def add_benchmark_flag(self, parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--benchmark", help="The benchmark to run.")

  @override
  def get_benchmark(self, args: argparse.Namespace) -> str | None:
    return args.benchmark


class PinpointBenchmarkSubcommand(PinpointBaseStartSubcommand):
  """Starts a new Pinpoint A/B job for a given benchmark."""

  def __init__(self, parent: PinpointSubcommand,
               benchmark_cls: BenchmarkClass) -> None:
    self._benchmark_cls = benchmark_cls
    super().__init__(parent)

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    pinpoint_benchmark = _PINPOINT_BENCHMARK_BY_CROSSBENCH_NAME[
        self._benchmark_cls.NAME]
    start_parser = self.create_parser(
        command=self._benchmark_cls.NAME,
        aliases=self._benchmark_cls.aliases(),
        help_text=f'Starts a new "{pinpoint_benchmark}" Pinpoint A/B job.')
    start_parser.epilog = (
        f"Example:\npinpoint {self._benchmark_cls.NAME} --bot win-11-perf\n\n")
    return start_parser

  @override
  def get_benchmark(self, args: argparse.Namespace) -> str | None:
    return _PINPOINT_BENCHMARK_BY_CROSSBENCH_NAME[self._benchmark_cls.NAME]


class PinpointCancelSubcommand(PinpointJobSubcommand):
  """Cancel a specific Pinpoint job."""

  @override
  def create_parser(self) -> argparse.ArgumentParser:
    cancel_parser = self._parent.subparsers.add_parser(
        "cancel", help="Cancel a specific Pinpoint job.")
    cancel_parser.add_argument(
        "--reason",
        required=False,
        default="Cancelled via Pinpoint CLI.",
        help="Reason for cancellation.")
    return cancel_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    cancel_job(args.job, args.reason)


class PinpointBaseFilteredListSubcommand(PinpointBaseSubcommand):
  """Base subcommand class for displaying filtered string lists."""

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    parser = self.create_parser()
    parser.add_argument(
        "--filter",
        type=str,
        default=None,
        help=("Filter results by a case-insensitive substring match. "
              "Only items containing the filter string will be shown."))
    return parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    items = self.fetch_list(args)
    filter_str = (args.filter or "").lower().strip()
    filtered_items = [item for item in items if filter_str in item.lower()]
    print("\n".join(filtered_items))

  @abc.abstractmethod
  def create_parser(self) -> argparse.ArgumentParser:
    pass

  @abc.abstractmethod
  def fetch_list(self, args: argparse.Namespace) -> list[str]:
    pass


class PinpointBotsSubcommand(PinpointBaseFilteredListSubcommand):
  """A subcommand for displaying available Pinpoint bots."""

  @override
  def create_parser(self) -> argparse.ArgumentParser:
    bots_parser = self._parent.subparsers.add_parser(
        "bots", help="Displays all available Pinpoint bots.")
    return bots_parser

  @override
  def fetch_list(self, args: argparse.Namespace) -> list[str]:
    return fetch_bots()


class PinpointBenchmarksSubcommand(PinpointBaseFilteredListSubcommand):
  """A subcommand for displaying available Pinpoint benchmarks."""

  @override
  def create_parser(self) -> argparse.ArgumentParser:
    benchmarks_parser = self._parent.subparsers.add_parser(
        "benchmarks", help="Displays all available Pinpoint benchmarks.")
    return benchmarks_parser

  @override
  def fetch_list(self, args: argparse.Namespace) -> list[str]:
    return fetch_benchmarks()


class PinpointStoriesSubcommand(PinpointBaseFilteredListSubcommand):
  """A subcommand for displaying available stories for a Pinpoint benchmark."""

  @override
  def create_parser(self) -> argparse.ArgumentParser:
    stories_parser = self._parent.subparsers.add_parser(
        "stories",
        help="Displays all available stories for a Pinpoint benchmark.")
    stories_parser.add_argument(
        "benchmark", help="The benchmark for which to list stories.")
    return stories_parser

  @override
  def fetch_list(self, args: argparse.Namespace) -> list[str]:
    return fetch_stories(args.benchmark)


class PinpointBuildsSubcommand(PinpointBaseSubcommand):
  """Displays recent successful builds for a given bot."""

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    builds_parser = self._parent.subparsers.add_parser(
        "builds", help="Displays recent successful builds for a given bot.")
    builds_parser.add_argument(
        "bot", help="The bot configuration name (e.g., 'linux-r350-perf').")
    builds_parser.add_argument(
        "--limit",
        "-l",
        type=NumberParser.positive_int,
        default=10,
        help="Limits the number of recent builds to display. (default: 10)")
    return builds_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    list_builds(args.bot, args.limit)


class PinpointResultsSubcommand(PinpointJobSubcommand):
  """Downloads results of a Pinpoint job."""

  @override
  def create_parser(self) -> argparse.ArgumentParser:
    results_parser = self._parent.subparsers.add_parser(
        "results", help="Downloads results of the given Pinpoint job.")
    results_parser.add_argument(
        "--output-directory",
        "--out-dir",
        "-o",
        type=pth.LocalPath,
        help=("Results will be stored in this directory. "
              "Uses to the crossbench results directory by default."))
    return results_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    download_results(job_id=args.job, out_dir=args.output_directory)


class PinpointSubcommand(CrossbenchSubcommand):
  """A subcommand for interacting with the Pinpoint service."""

  def __init__(self, cli: CrossBenchCLI) -> None:
    super().__init__(cli)
    self._subparsers = self.parser.add_subparsers(
        parser_class=CrossBenchArgumentParser,
        dest="action",
        required=True,
        help="Pinpoint actions")
    self._list_subcommand = PinpointListSubcommand(self)
    self._config_subcommand = PinpointConfigSubcommand(self)
    self._start_subcommand = PinpointStartSubcommand(self)
    self._cancel_subcommand = PinpointCancelSubcommand(self)
    self._bots_subcommand = PinpointBotsSubcommand(self)
    self._benchmarks_subcommand = PinpointBenchmarksSubcommand(self)
    self._stories_subcommand = PinpointStoriesSubcommand(self)
    self._builds_subcommand = PinpointBuildsSubcommand(self)
    self._results_subcommand = PinpointResultsSubcommand(self)
    self._benchmark_subcommands: list[PinpointBenchmarkSubcommand] = []
    for benchmark_cls in cli.BENCHMARKS:
      if _PINPOINT_BENCHMARK_BY_CROSSBENCH_NAME[benchmark_cls.NAME]:
        self._benchmark_subcommands.append(
            PinpointBenchmarkSubcommand(self, benchmark_cls))

  @property
  def subparsers(self) -> Subparsers:
    return self._subparsers

  @override
  def add_cli_parser(self) -> argparse.ArgumentParser:
    pinpoint_parser = self.cli.subparsers.add_parser(
        "pinpoint",
        aliases=("pp",),
        help="Interact with the Pinpoint service.",
        formatter_class=PinpointHelpFormatter)
    assert isinstance(pinpoint_parser, CrossBenchArgumentParser)
    return pinpoint_parser

  @override
  def run(self, args: argparse.Namespace) -> None:
    args.pinpoint_subcommand.run(args)


class PinpointHelpFormatter(argparse.HelpFormatter):
  """Hacks HelpFormatter for displaying a pretty Pinpoint help message."""

  @override
  def _format_action(self,
                     action: argparse.Action,
                     custom_format: bool = True) -> str:
    if not custom_format or not hasattr(action, "_get_subactions"):
      return super()._format_action(action)

    subactions = action._get_subactions()  # noqa: SLF001
    assert subactions

    pinpoint_parts = []
    benchmark_parts = []
    for subaction in subactions:
      text = self._format_action(subaction, custom_format=False)
      if subaction.dest in _PINPOINT_BENCHMARK_BY_CROSSBENCH_NAME:
        benchmark_parts.append(text)
      else:
        pinpoint_parts.append(text)

    return "\n".join([
        "".join(pinpoint_parts),
        "\nBenchmarks:",
        "".join(benchmark_parts),
    ])
