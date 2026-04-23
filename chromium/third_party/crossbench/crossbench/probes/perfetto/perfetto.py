# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import dataclasses
import functools
import logging
from typing import TYPE_CHECKING, ClassVar, Final, Iterable, Self

from typing_extensions import override

from crossbench import path as pth
from crossbench.config import ConfigObject, config_dir
from crossbench.helper import fs_helper
from crossbench.helper.collection_helper import close_matches_message
from crossbench.parse import ObjectParser, PathParser
from crossbench.probes.perfetto.context.android import \
    AndroidPerfettoProbeContext
from crossbench.probes.perfetto.context.chromeos import \
    ChromeOsPerfettoProbeContext
from crossbench.probes.perfetto.context.desktop import \
    DesktopPerfettoProbeContext
from crossbench.probes.probe import Probe, ProbeConfigParser, ProbeKeyT
from crossbench.probes.result_location import ResultLocation
from protoc import trace_config_pb2

if TYPE_CHECKING:
  from crossbench.browsers.browser import Browser
  from crossbench.probes.perfetto.context.base import PerfettoProbeContext
  from crossbench.runner.groups.browsers import BrowsersRunGroup
  from crossbench.runner.run import Run


@dataclasses.dataclass
class TraceConfig(ConfigObject):
  """ See https://perfetto.dev/docs/reference/trace-config-proto for more
  details."""
  VALID_EXTENSIONS: ClassVar[tuple[str, ...]] = (".pbtxt", ".proto",
                                                 ".textproto", ".txtpb")
  trace_config: trace_config_pb2.TraceConfig

  @classmethod
  @override
  def parse_str(cls, value: str) -> Self:
    if ":" in value:
      return cls.parse_textproto(value)
    presets = cls.presets()
    if preset_file := presets.get(value):
      return cls.parse_path(preset_file)
    error_message, alternative = close_matches_message(value, presets.keys(),
                                                       "TraceConfig preset")
    if not alternative:
      raise ValueError(error_message)
    logging.error(error_message)
    preset_file = presets[alternative]
    return cls.parse_path(preset_file)

  @classmethod
  def parse_textproto(cls, value: str) -> Self:
    trace_config = trace_config_pb2.TraceConfig()
    ObjectParser.parse_text_or_binary_proto(trace_config, value.encode("utf-8"))
    return cls(trace_config)

  @classmethod
  @override
  def parse_path(cls, path: pth.LocalPath, **kwargs) -> Self:
    trace_config = trace_config_pb2.TraceConfig()
    ObjectParser.parse_text_or_binary_proto_file(trace_config, path)
    return cls(trace_config, **kwargs)

  @classmethod
  def preset_dir(cls) -> pth.LocalPath:
    return config_dir() / "probe/perfetto/trace_config"

  @classmethod
  @functools.cache
  def presets(cls) -> dict[str, pth.LocalPath]:
    result: dict[str, pth.LocalPath] = {}
    for preset_config in cls.preset_dir().glob("*.pbtxt"):
      result[preset_config.stem] = preset_config
    assert result, f"No trace_config presets found {cls.preset_dir()}"
    return result

  @override
  def to_argument_value(self) -> trace_config_pb2.TraceConfig:
    return self.trace_config

  @classmethod
  @override
  def help_text_items(cls) -> list[tuple[str, str]]:
    help_items = super().help_text_items()
    help_items.append(("presets", ",".join(cls.presets().keys())))
    return help_items


def has_v8_code_data_source(trace_config: trace_config_pb2.TraceConfig) -> bool:
  return has_data_source(trace_config, "dev.v8.code")


def has_data_source(trace_config: trace_config_pb2.TraceConfig,
                    name: str) -> bool:
  return any(data_source.config.name == name
             for data_source in trace_config.data_sources)


class PerfettoProbe(Probe):
  """
  A probe to collect Perfetto system traces that can be viewed on
  https://ui.perfetto.dev/. The probe supports Android and ChromeOS targets.

  Recommended way to use:
  1. Go to https://ui.perfetto.dev/, click "Record new trace" and set up your
     preferred tracing options.
  2. Click "Recording command" and copy the textproto config part of the
     command.
  3. Paste it into the textproto field of the probe config. An example probe
     config can be found at config/doc/probe/perfetto.config.hjson.
  4. Specify the config via the --probe-config command-line flag.

  After the run, the trace will be found among the results as
  "perfetto.trace.pb.gz".
  """
  NAME: ClassVar = "perfetto"
  RESULT_LOCATION: ClassVar = ResultLocation.BROWSER

  @classmethod
  @override
  def config_parser(cls) -> ProbeConfigParser[Self]:
    parser = super().config_parser()
    parser.add_default_argument(
        "trace_config",
        aliases=("config", "textproto"),
        type=TraceConfig,
        help=("Serialized perfetto configuration. "
              "See probe instructions for more details"))
    parser.add_argument(
        "perfetto_bin",
        type=PathParser.any_path,
        default=pth.AnyPath("perfetto"),
        help="Perfetto binary on the browser device (android, chrome-os)")
    parser.add_argument(
        "tracebox_bin",
        type=PathParser.any_path,
        default=pth.AnyPath("tracebox"),
        help="Tracebox binary on the browser device (linux, macos). "
        "Auto downloaded on local devices.")
    parser.add_argument(
        "trace_browser_startup",
        type=bool,
        default=False,
        help="Start perfetto tracing before launching the browser.")
    parser.add_argument(
        "config_via_stdin",
        type=bool,
        default=False,
        help="Pass perfetto tracing config via stdin.")
    return parser

  def __init__(self,
               trace_config: trace_config_pb2.TraceConfig,
               perfetto_bin: pth.AnyPath,
               tracebox_bin: pth.AnyPath,
               trace_browser_startup: bool = False,
               config_via_stdin: bool = False) -> None:
    super().__init__()
    if not trace_config:
      raise ValueError("Please specify a tracing config")
    self._trace_config: Final[trace_config_pb2.TraceConfig] = trace_config
    self._perfetto_bin: Final[pth.AnyPath] = perfetto_bin
    self._tracebox_bin: Final[pth.AnyPath] = tracebox_bin
    self._trace_browser_startup: Final[bool] = trace_browser_startup
    self._config_via_stdin: Final[bool] = config_via_stdin
    self._needs_v8_code_logger: Final[bool] = has_v8_code_data_source(
        trace_config)

  @property
  @override
  def key(self) -> ProbeKeyT:
    return super().key + (
        ("textproto", str(self.trace_config)),
        ("perfetto_bin", str(self.perfetto_bin)),
        ("tracebox_bin", str(self.tracebox_bin)),
        ("trace_browser_startup", str(self.trace_browser_startup)),
        ("config_via_stdin", str(self.config_via_stdin)),
    )

  @property
  def trace_config(self) -> trace_config_pb2.TraceConfig:
    return self._trace_config

  @property
  def perfetto_bin(self) -> pth.AnyPath:
    return self._perfetto_bin

  @property
  def tracebox_bin(self) -> pth.AnyPath:
    return self._tracebox_bin

  @property
  def trace_browser_startup(self) -> bool:
    return self._trace_browser_startup

  @property
  def config_via_stdin(self) -> bool:
    return self._config_via_stdin

  @property
  @override
  def result_path_name(self) -> str:
    return "perfetto.trace.pb"

  @override
  def attach(self, browser: Browser) -> None:
    assert browser.attributes().is_chromium_based
    browser.features.enable("EnablePerfettoSystemTracing")
    if self._needs_v8_code_logger:
      logging.debug("Auto-enabling --perfetto-code-logger on %s", browser)
      browser.js_flags.set("--perfetto-code-logger")
    super().attach(browser)

  @override
  def log_run_result(self, run: Run) -> None:
    self._log_results([run])

  @override
  def log_browsers_result(self, group: BrowsersRunGroup) -> None:
    self._log_results(group.runs)

  def _log_results(self, runs: Iterable[Run]) -> None:
    logging.info("-" * 80)
    logging.critical("Perfetto trace results:")
    for run in runs:
      result_file = run.results[self].file
      logging.critical("  - %s : %s", result_file,
                       fs_helper.get_file_size(result_file))

  @override
  def create_context(self, run: Run) -> PerfettoProbeContext:
    # TODO: support more platforms
    if run.browser_platform.is_chromeos:
      return ChromeOsPerfettoProbeContext(self, run)
    if run.browser_platform.is_android:
      return AndroidPerfettoProbeContext(self, run)
    return DesktopPerfettoProbeContext(self, run)
