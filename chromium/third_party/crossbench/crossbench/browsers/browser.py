# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import logging
import shlex
from typing import TYPE_CHECKING, Any, Iterable, Optional, Sequence, Tuple

from ordered_set import OrderedSet

from crossbench import path as pth
from crossbench import plt
from crossbench.browsers.splash_screen import SplashScreen
from crossbench.browsers.viewport import Viewport
from crossbench.flags.base import Flags, FlagsT
from crossbench.network.live import LiveNetwork

if TYPE_CHECKING:
  import datetime as dt

  from crossbench.browsers.attributes import BrowserAttributes
  from crossbench.env import HostEnvironment
  from crossbench.flags.chrome import ChromeFeatures
  from crossbench.flags.js_flags import JSFlags
  from crossbench.network.base import Network
  from crossbench.probes.probe import Probe
  from crossbench.runner.groups import BrowserSessionRunGroup
  from crossbench.runner.runner import Runner
  from crossbench.types import JsonDict


class Browser(abc.ABC):

  @classmethod
  def default_flags(cls, initial_data: Flags.InitialDataType = None) -> Flags:
    return Flags(initial_data)

  def __init__(self,
               label: str,
               path: Optional[pth.RemotePath] = None,
               flags: Optional[Flags.InitialDataType] = None,
               js_flags: Optional[Flags.InitialDataType] = None,
               cache_dir: Optional[pth.RemotePath] = None,
               network: Optional[Network] = None,
               driver_path: Optional[pth.RemotePath] = None,
               viewport: Optional[Viewport] = None,
               splash_screen: Optional[SplashScreen] = None,
               platform: Optional[plt.Platform] = None):
    self._platform = platform or plt.PLATFORM
    assert not driver_path, "driver_path not supported by base Browser"
    self.label: str = label
    self._unique_name: str = ""
    self.app_name: str = self.type_name
    self.version: str = "custom"
    self.major_version: int = 0
    self.app_path: pth.RemotePath = pth.RemotePath()
    if path:
      self.path = self._resolve_binary(path)
      # TODO clean up
      if not self.platform.is_android:
        assert self.path.is_absolute()
      self.version = self._extract_version()
      self.major_version = int(self.version.split(".")[0])
      self.unique_name = f"{self.type_name}_v{self.major_version}_{self.label}"
    else:
      # TODO: separate class for remote browser (selenium) without an explicit
      # binary path.
      self.path = pth.RemotePath()
      self.unique_name = f"{self.type_name}_{self.label}".lower()
    self._network: Network = network or LiveNetwork()
    self._viewport: Viewport = viewport or Viewport.DEFAULT
    self._splash_screen: SplashScreen = splash_screen or SplashScreen.DEFAULT
    self._is_running: bool = False
    self.cache_dir: Optional[pth.RemotePath] = cache_dir
    self.clear_cache_dir: bool = True
    self._pid: Optional[int] = None
    self._probes: OrderedSet[Probe] = OrderedSet()
    self._flags: Flags = self.default_flags(flags)
    assert not js_flags, "Base Browser doesn't support js_flags directly"
    self.log_file: Optional[pth.RemotePath] = None

  @property
  @abc.abstractmethod
  def type_name(self) -> str:
    pass

  @property
  @abc.abstractmethod
  def attributes(self) -> BrowserAttributes:
    pass

  @property
  def platform(self) -> plt.Platform:
    return self._platform

  @property
  def unique_name(self) -> str:
    return self._unique_name

  @unique_name.setter
  def unique_name(self, name: str) -> None:
    assert name
    # Replace any potentially unsafe chars in the name
    self._unique_name = pth.safe_filename(name).lower()

  @property
  def network(self) -> Network:
    return self._network

  @property
  def splash_screen(self) -> SplashScreen:
    return self._splash_screen

  @property
  def viewport(self) -> Viewport:
    return self._viewport

  @viewport.setter
  def viewport(self, value: Viewport) -> None:
    assert self._viewport.is_default
    self._viewport = value

  @property
  def probes(self) -> Iterable[Probe]:
    return iter(self._probes)

  @property
  def flags(self) -> Flags:
    return self._flags

  @property
  def features(self) -> ChromeFeatures:
    raise NotImplementedError(f"Unsupported feature flags on {self}.")

  @property
  def js_flags(self) -> JSFlags:
    raise NotImplementedError(f"Unsupported feature flags on {self}.")

  def user_agent(self, runner: Runner) -> str:
    return str(self.js(runner, "return window.navigator.userAgent"))

  @property
  def pid(self) -> Optional[int]:
    return self._pid

  @property
  def is_running(self) -> Optional[bool]:
    if self.pid is None:
      return None
    info = self.platform.process_info(self.pid)
    if info is None:
      return None
    if status := info.get("status"):
      return status in ("running", "sleeping")
    # TODO(cbruni): fix posix process_info for remote platforms where
    # we don't get the status back.
    return False

  @property
  def is_running(self) -> bool:
    return self._is_running

  def validate_env(self, env: HostEnvironment) -> None:
    """Called before starting a browser / browser session to perform
    a pre-run checklist."""

  @property
  def is_local(self) -> bool:
    return self.platform.is_local

  @property
  def is_remote(self) -> bool:
    return self.platform.is_remote

  def set_log_file(self, path: pth.RemotePath) -> None:
    self.log_file = path

  @property
  def stdout_log_file(self) -> pth.RemotePath:
    assert self.log_file
    return self.log_file.with_suffix(".stdout.log")

  def _resolve_binary(self, path: pth.RemotePath) -> pth.RemotePath:
    path = self.platform.absolute(path)
    assert self.platform.exists(path), f"Binary at path={path} does not exist."
    self.app_path = path
    self.app_name = self.app_path.stem
    if self.platform.is_macos:
      path = self._resolve_macos_binary(path)
    assert self.platform.is_file(path), (
        f"Binary at path={path} is not a file.")
    return path

  def _resolve_macos_binary(self, path: pth.RemotePath) -> pth.RemotePath:
    assert self.platform.is_macos
    candidate = self.platform.search_binary(path)
    if not candidate or not self.platform.is_file(candidate):
      raise ValueError(f"Could not find browser executable in {path}")
    return candidate

  def attach_probe(self, probe: Probe) -> None:
    if probe in self._probes:
      raise ValueError(f"Cannot attach same probe twice: {probe}")
    self._probes.add(probe)
    probe.attach(self)

  def details_json(self) -> JsonDict:
    return {
        "label": self.label,
        "browser": self.type_name,
        "unique_name": self.unique_name,
        "app_name": self.app_name,
        "version": self.version,
        "flags": tuple(self.flags),
        "js_flags": [],
        "path": str(self.path),
        "clear_cache_dir": self.clear_cache_dir,
        "major_version": self.major_version,
        "log": {}
    }

  def setup_binary(self, runner: Runner) -> None:
    pass

  def setup(self, session: BrowserSessionRunGroup) -> None:
    assert not self._is_running, (
        "Previously used browser was not correctly stopped.")
    runner = session.runner
    self.clear_cache(runner)
    self.start(session)
    assert self._is_running

  @abc.abstractmethod
  def _extract_version(self) -> str:
    pass

  def clear_cache(self, runner: Runner) -> None:
    del runner
    if self.clear_cache_dir and self.cache_dir:
      self.platform.rm(self.cache_dir, missing_ok=True, dir=True)

  @abc.abstractmethod
  def start(self, session: BrowserSessionRunGroup) -> None:
    pass

  def _log_browser_start(self,
                         args: Tuple[str, ...],
                         driver_path: Optional[pth.RemotePath] = None) -> None:
    logging.info("STARTING BROWSER Binary:  %s", self.path)
    logging.info("STARTING BROWSER Version: %s", self.version)
    if driver_path:
      logging.info("STARTING BROWSER Driver:  %s", driver_path)
    logging.info("STARTING BROWSER Network: %s", self.network)
    logging.info("STARTING BROWSER Probes:  %s",
                 ", ".join(p.NAME for p in self.probes))
    logging.info("STARTING BROWSER Flags:   %s", shlex.join(args))

  def _get_browser_flags_for_session(
      self, session: BrowserSessionRunGroup) -> Tuple[str, ...]:
    flags_copy: Flags = self.flags.copy()
    flags_copy.update(session.extra_flags)
    flags_copy.update(self.network.extra_flags(self))
    flags_copy = self._filter_flags_for_run(flags_copy)
    return tuple(flags_copy)

  def _filter_flags_for_run(self, flags: FlagsT) -> FlagsT:
    return flags

  def quit(self, runner: Runner) -> None:
    del runner
    assert self._is_running, "Browser is already stopped"
    try:
      self.force_quit()
    finally:
      self._pid = None

  def force_quit(self) -> None:
    if not self._is_running:
      return
    logging.info("Browser.force_quit()")
    if self.platform.is_macos:
      self.platform.exec_apple_script(f"""
  tell application "{self.app_path}"
    quit
  end tell
      """)
    elif self._pid:
      self.platform.terminate(self._pid)
    self._is_running = False

  @abc.abstractmethod
  def js(
      self,
      runner: Runner,
      script: str,
      timeout: Optional[dt.timedelta] = None,
      arguments: Sequence[object] = ()
  ) -> Any:
    pass

  def run_script_on_new_document(self, script: str) -> None:
    del script
    raise NotImplementedError(
        f"New document script injection is not supported by {self}")

  @abc.abstractmethod
  def show_url(self,
               runner: Runner,
               url: str,
               target: Optional[str] = None) -> None:
    pass

  def _sync_viewport_flag(self, flags: Flags, flag: str,
                          is_requested_by_viewport: bool,
                          replacement: Viewport) -> None:
    if is_requested_by_viewport:
      flags.set(flag)
    elif flag in flags:
      if self.viewport.is_default:
        self.viewport = replacement
      else:
        raise ValueError(
            f"{flag} conflicts with requested --viewport={self.viewport}")

  def __str__(self) -> str:
    platform_prefix = ""
    if self.platform.is_remote:
      platform_prefix = str(self.platform)
    return f"{platform_prefix}{self.type_name.capitalize()}:{self.label}"

  def __hash__(self) -> int:
    # Poor-man's hash, browsers should be unique.
    return hash(id(self))

  def performance_mark(self, runner: Runner, name: str):
    self.js(runner, "performance.mark(arguments[0]);", arguments=[name])
