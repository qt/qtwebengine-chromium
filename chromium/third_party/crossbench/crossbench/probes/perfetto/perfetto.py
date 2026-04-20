# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import atexit
import dataclasses
import datetime as dt
import functools
import logging
import subprocess
from typing import TYPE_CHECKING, ClassVar, Final, Iterable, Self, cast

import google.protobuf.text_format as proto_text_format
from typing_extensions import override

from crossbench import path as pth
from crossbench.config import ConfigObject, config_dir
from crossbench.helper import fs_helper
from crossbench.helper.collection_helper import close_matches_message
from crossbench.helper.wait import WaitRange
from crossbench.parse import NumberParser, ObjectParser, PathParser
from crossbench.plt.android_adb import AndroidAdbPlatform
from crossbench.plt.chromeos_ssh import ChromeOsSshPlatform
from crossbench.probes.perfetto.downloader import PerfettoToolDownloader
from crossbench.probes.probe import (Probe, ProbeConfigParser, ProbeContext,
                                     ProbeKeyT)
from crossbench.probes.result_location import ResultLocation
from crossbench.probes.results import LocalProbeResult, ProbeResult
from protoc import trace_config_pb2

if TYPE_CHECKING:
  from crossbench.browsers.browser import Browser
  from crossbench.plt.types import TupleCmdArgs
  from crossbench.runner.groups.browsers import BrowsersRunGroup
  from crossbench.runner.run import Run

_PERFETTO_CONFIG_REMOTE_DIR_ANDROID: Final = pth.AnyPath(
    "/data/misc/perfetto-configs/")
_PERFETTO_TRACE_REMOTE_DIR_ANDROID: Final = pth.AnyPath(
    "/data/misc/perfetto-traces/")
_PERFETTO_REMOTE_DIR_CROS: Final = pth.AnyPath("/usr/local/tmp")


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
    return parser

  def __init__(self,
               trace_config: trace_config_pb2.TraceConfig,
               perfetto_bin: pth.AnyPath,
               tracebox_bin: pth.AnyPath,
               trace_browser_startup: bool = False) -> None:
    super().__init__()
    if not trace_config:
      raise ValueError("Please specify a tracing config")
    self._trace_config: trace_config_pb2.TraceConfig = trace_config
    self._perfetto_bin = perfetto_bin
    self._tracebox_bin = tracebox_bin
    self._trace_browser_startup = trace_browser_startup

  @property
  @override
  def key(self) -> ProbeKeyT:
    return super().key + (
        ("textproto", str(self.trace_config)),
        ("perfetto_bin", str(self.perfetto_bin)),
        ("tracebox_bin", str(self.tracebox_bin)),
        ("trace_browser_startup", str(self.trace_browser_startup)),
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
  @override
  def result_path_name(self) -> str:
    return "perfetto.trace.pb"

  @override
  def attach(self, browser: Browser) -> None:
    assert browser.attributes().is_chromium_based
    browser.features.enable("EnablePerfettoSystemTracing")
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


PERFETTO_CONFIG_NAME: Final[str] = "perfetto_config.textproto"
PERFETTO_TRACE_NAME: Final[str] = "perfetto.trace.pb"

class PerfettoProbeContext(ProbeContext[PerfettoProbe], metaclass=abc.ABCMeta):
  def __init__(self, probe: PerfettoProbe, run: Run) -> None:
    self._file_prefix: Final[str] = dt.datetime.now().strftime(
        "%Y-%m-%d_%H%M%S")
    super().__init__(probe, run)
    self._host_config_file: Final[pth.LocalPath] = (
        run.out_dir / PERFETTO_CONFIG_NAME)
    self._perfetto_pid: int | None = None

  def setup(self) -> None:
    assert self._perfetto_pid is None
    for p in self.browser_platform.processes():
      if p["name"] == "perfetto":
        logging.warning("PERFETTO: killing existing session pid: %s", p["pid"])
        self.browser_platform.terminate(p["pid"])
    self._setup_validate_bin()
    self._setup_push_perfetto_config()
    if self.probe.trace_browser_startup:
      self._start_perfetto()

  def _setup_validate_bin(self) -> None:
    binary = self.perfetto_cmd[0]
    if not self.browser_platform.which(binary):
      raise ValueError(
          f"{repr(binary)} cannot be found on {self.browser_platform}")

  def _setup_push_perfetto_config(self) -> None:
    self.host_platform.write_text(
        self._host_config_file,
        proto_text_format.MessageToString(self.probe.trace_config))
    self.browser_platform.push(self._host_config_file,
                               self.get_browser_config_path())

  @abc.abstractmethod
  def get_browser_config_path(self) -> pth.AnyPath:
    pass

  @abc.abstractmethod
  def get_default_result_path(self) -> pth.AnyPath:
    pass

  @property
  def perfetto_cmd(self) -> TupleCmdArgs:
    return (self.probe.perfetto_bin,)

  def start(self) -> None:
    if self.probe.trace_browser_startup:
      if not self._perfetto_pid:
        raise RuntimeError("Perfetto was not started")
      return
    self._start_perfetto()
    self.browser.performance_mark("probe-perfetto-start")

  def stop(self) -> None:
    self.browser.performance_mark("probe-perfetto-stop")
    logging.info("PERFETTO: stopping")
    if not self._perfetto_pid:
      raise RuntimeError("Perfetto was not started")
    self._stop_perfetto()

  def _start_perfetto(self) -> None:
    logging.info("PERFETTO: starting")
    cmd: TupleCmdArgs = self.perfetto_cmd + (
        "--background",
        "--config",
        self.get_browser_config_path(),
        "--txt",
        "--out",
        self.result_path,
    )
    try:
      proc = self.browser_platform.sh(*cmd, capture_output=True)
    except subprocess.CalledProcessError as e:
      logging.error("perfetto command failed with stderr: %s",
                    e.stderr.decode(encoding="utf-8"))
      raise

    self._perfetto_pid = NumberParser.positive_int(
        proc.stdout.decode("utf-8").rstrip(), "perfetto pid")
    atexit.register(self._stop_perfetto)

  def _stop_perfetto(self) -> None:
    if not self._perfetto_pid:
      return
    atexit.unregister(self._stop_perfetto)
    # TODO(cbruni): replace with terminate_gracefully
    self.browser_platform.terminate(self._perfetto_pid)
    try:
      for _ in WaitRange(1, timeout=30).wait_with_backoff():
        if not self.browser_platform.process_info(self._perfetto_pid):
          break
    except TimeoutError:
      logging.error("perfetto process did not stop after 30s. "
                    "The trace might be incomplete.")
    self._perfetto_pid = None

  def teardown(self) -> ProbeResult:
    try:
      return self._transfer_results()
    finally:
      if self.browser_platform.is_remote:
        self._cleanup_remote_perfetto_files()

  def _transfer_results(self) -> ProbeResult:
    browser_result = self.browser_result(file=[self.result_path])
    local_result_file = browser_result.file
    assert local_result_file.is_file(), (
        f"Could not copy perfetto results: {local_result_file}")
    renamed_result_file = local_result_file.with_name(PERFETTO_TRACE_NAME)
    self.host_platform.rename(local_result_file, renamed_result_file)

    self.host_platform.sh("gzip", renamed_result_file)
    renamed_result_file = renamed_result_file.with_suffix(
        f"{local_result_file.suffix}.gz")
    assert renamed_result_file.is_file(), (
        f"Could not compress {renamed_result_file}")

    return LocalProbeResult(trace=(renamed_result_file,))

  def _cleanup_remote_perfetto_files(self) -> None:
    # Especially on android, the perfetto files are not in the default tmp dir.
    self.browser_platform.rm(self.result_path, missing_ok=True)
    self.browser_platform.rm(self.get_browser_config_path(), missing_ok=True)


class DesktopPerfettoProbeContext(PerfettoProbeContext):

  def __init__(self, probe: PerfettoProbe, run: Run) -> None:
    self._tracebox_proc: subprocess.Popen | None = None
    super().__init__(probe, run)
    self._tracebox_bin = self.probe.tracebox_bin

  @override
  def get_browser_config_path(self) -> pth.AnyPath:
    return self.result_path.with_name(PERFETTO_CONFIG_NAME)

  @override
  def get_default_result_path(self) -> pth.AnyPath:
    return self._run.get_default_probe_result_path(
        self._probe).with_name(PERFETTO_TRACE_NAME)

  @override
  def setup(self) -> None:
    super().setup()
    self._tracebox_proc = self._setup_tracebox()

  @override
  def _setup_validate_bin(self) -> None:
    if not self.browser_platform.which(self._tracebox_bin):
      self._tracebox_bin = PerfettoToolDownloader(
          "tracebox", platform=self.browser_platform).download()
    super()._setup_validate_bin()

  @override
  def teardown(self) -> ProbeResult:
    self._teardown_tracebox()
    return super().teardown()

  def _setup_tracebox(self) -> subprocess.Popen:
    tracebox_proc = self.browser_platform.popen(self._tracebox_bin, "traced",
                                                "traced_probes")
    atexit.register(self._teardown_tracebox)
    return tracebox_proc

  def _teardown_tracebox(self) -> None:
    if self._tracebox_proc:
      atexit.unregister(self._teardown_tracebox)
      self._tracebox_proc.terminate()
      self._tracebox_proc = None

  @property
  @override
  def perfetto_cmd(self) -> TupleCmdArgs:
    return (self._tracebox_bin, "perfetto")


class AndroidPerfettoProbeContext(PerfettoProbeContext):

  @override
  def get_browser_config_path(self) -> pth.AnyPath:
    return _PERFETTO_CONFIG_REMOTE_DIR_ANDROID / (
        f"{self._file_prefix}_{PERFETTO_CONFIG_NAME}")

  @override
  def get_default_result_path(self) -> pth.AnyPath:
    return _PERFETTO_TRACE_REMOTE_DIR_ANDROID / (
        f"{self._file_prefix}_{PERFETTO_TRACE_NAME}")

  @property
  @override
  def browser_platform(self) -> AndroidAdbPlatform:
    browser_platform = super().browser_platform
    assert isinstance(browser_platform, AndroidAdbPlatform)
    return browser_platform


class ChromeOsPerfettoProbeContext(PerfettoProbeContext):

  @property
  @override
  def browser_platform(self) -> ChromeOsSshPlatform:
    browser_platform = super().browser_platform
    isinstance(browser_platform, ChromeOsSshPlatform)
    return cast(ChromeOsSshPlatform, browser_platform)

  @override
  def get_browser_config_path(self) -> pth.AnyPath:
    return _PERFETTO_REMOTE_DIR_CROS / (
        f"{self._file_prefix}_{PERFETTO_CONFIG_NAME}")

  @override
  def get_default_result_path(self) -> pth.AnyPath:
    return _PERFETTO_REMOTE_DIR_CROS / (
        f"{self._file_prefix}_{PERFETTO_TRACE_NAME}")
