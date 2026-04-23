# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import contextlib
import json
import logging
from typing import TYPE_CHECKING, Any, Final, Iterator, Optional, Self, TypeVar

from typing_extensions import override

from crossbench.flags.base import Flags
from crossbench.helper.path_finder import WprCloudBinary, WprGoFinder
from crossbench.network.replay.base import GS_PREFIX, ReplayNetwork
from crossbench.network.replay.web_page_replay import WprReplayServer
from crossbench.path import check_hash

if TYPE_CHECKING:
  from crossbench.browsers.attributes import BrowserAttributes
  from crossbench.network.base import TrafficShaper
  from crossbench.path import AnyPath, LocalPath
  from crossbench.plt import Platform
  from crossbench.runner.groups.session import BrowserSessionRunGroup

  WprReplayNetworkT = TypeVar("WprReplayNetworkT", bound="WprReplayNetwork")

# use value for pylint
assert GS_PREFIX


class WprReplayNetwork(ReplayNetwork):

  def __init__(self, archive: LocalPath | str,
               traffic_shaper: Optional[TrafficShaper],
               wpr_go_bin: Optional[LocalPath], browser_platform: Platform,
               persist_server: bool, inject_deterministic_script: bool,
               no_archive_certificates: bool,
               response_transformations_file: LocalPath | None,
               cross_platform_mode: bool, host: str | None) -> None:
    super().__init__(archive, traffic_shaper, browser_platform)
    self._server: WprReplayServer | None = None
    self._tmp_dir: AnyPath | None = None
    self._persist_server: Final[bool] = persist_server
    self._inject_deterministic_script: Final[bool] = inject_deterministic_script
    self._no_archive_certificates: Final[bool] = no_archive_certificates
    self._response_transformations_file: Final[
        LocalPath | None] = response_transformations_file
    self._cross_platform_mode: Final[bool] = cross_platform_mode
    self._wpr_go_bin: Final[LocalPath] = self._ensure_wpr_go(wpr_go_bin)
    self._host: Final[str | None] = host

  @override
  def extra_flags(self, browser_attributes: BrowserAttributes) -> Flags:
    if self._cross_platform_mode:
      return Flags()

    assert self.is_running, "Extra network flags are not valid"
    assert self._server
    if not browser_attributes.is_chromium_based:
      raise ValueError(
          "Only chromium-based browsers are supported for wpr replay.")
    # TODO: make ports configurable.
    extra_flags = super().extra_flags(browser_attributes)
    # TODO: read this from wpr_public_hash.txt like in the recorder probe
    extra_flags["--ignore-certificate-errors-spki-list"] = (
        "PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,"
        "2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=")
    if self._traffic_shaper.is_live:
      # Only remap ports if we're not using the SOCKS proxy from the traffic
      # shaper.
      extra_flags["--host-resolver-rules"] = (
          f"MAP *:80 {self.host}:{self.http_port},"
          f"MAP *:443 {self.host}:{self.https_port},"
          "EXCLUDE localhost")

    return extra_flags

  @abc.abstractmethod
  def _ensure_wpr_go(self, wpr_go_bin: Optional[LocalPath] = None) -> LocalPath:
    pass

  @abc.abstractmethod
  def _create_server(self, log_dir: LocalPath) -> WprReplayServer:
    pass

  @contextlib.contextmanager
  @override
  def open(self, session: BrowserSessionRunGroup) -> Iterator[Self]:
    with super().open(session):
      yield self

  def _ensure_server_started(self, session: BrowserSessionRunGroup) -> None:
    log_dir = session.browser_dir if self._persist_server else session.out_dir
    if not self._server or not self._persist_server:
      self._server = self._create_server(log_dir)
      logging.debug("Starting WPR server")
      self._server.start()
    else:
      # TODO: reset wpr server state for reuse
      logging.debug("WPR server already started")

  @contextlib.contextmanager
  @override
  def _open_replay_server(self,
                          session: BrowserSessionRunGroup) -> Iterator[None]:
    self._ensure_server_started(session)
    try:
      yield
    finally:
      if not self._persist_server and self._server:
        self._server.stop()

  @property
  @override
  def http_port(self) -> int:
    assert self._server, "WPR is not running"
    return self._server.http_port

  @property
  @override
  def https_port(self) -> int:
    assert self._server, "WPR is not running"
    return self._server.https_port

  @property
  @override
  def host(self) -> str:
    assert self._server, "WPR is not running"
    return self._server.host

  @override
  def __str__(self) -> str:
    return f"WPR(archive={self.archive_path}, speed={self.traffic_shaper})"


class LocalWprReplayNetwork(WprReplayNetwork):

  @override
  def _ensure_wpr_go(self, wpr_go_bin: Optional[LocalPath] = None) -> LocalPath:
    if not wpr_go_bin:
      if local_wpr_go := WprGoFinder(self.host_platform).local_path:
        wpr_go_bin = local_wpr_go
    if not wpr_go_bin:
      raise RuntimeError(
          f"Could not find wpr.go binary on {self.host_platform}")
    if wpr_go_bin.suffix == ".go" and not self.host_platform.which("go"):
      raise ValueError(f"'go' binary not found on {self.host_platform}")
    return self.host_platform.parse_local_binary_path(wpr_go_bin,
                                                      "wpr.go source")

  @contextlib.contextmanager
  @override
  def open(self: LocalWprReplayNetwork,
           session: BrowserSessionRunGroup) -> Iterator[LocalWprReplayNetwork]:
    with super().open(session):
      with self._forward_ports(session):
        yield self

  @contextlib.contextmanager
  def _forward_ports(self, session: BrowserSessionRunGroup) -> Iterator:
    browser_platform = session.browser_platform
    need_forward_ports = (
        self._traffic_shaper.is_live and browser_platform.is_remote and
        not self._cross_platform_mode)
    if not need_forward_ports:
      yield
      return
    http_port: int = self.http_port
    https_port: int = self.https_port
    logging.info("REMOTE PORT FORWARDING: %s <= %s", self.host_platform,
                 browser_platform)
    # TODO: make ports configurable
    with browser_platform.ports.nested() as ports:
      ports.reverse_forward(http_port, http_port)
      ports.reverse_forward(https_port, https_port)
      yield
      # port cleanup happens automatically

  @override
  def _create_server(self, log_dir: LocalPath) -> WprReplayServer:
    extra_kwargs: dict[str, Any] = {}
    if not self._inject_deterministic_script:
      extra_kwargs["inject_scripts"] = []
    if self._cross_platform_mode:
      extra_kwargs["http_port"] = 80
      extra_kwargs["https_port"] = 443
      extra_kwargs["run_as_root"] = True
    if self._host:
      extra_kwargs["host"] = self._host

    return WprReplayServer(
        self.archive_path,
        self._wpr_go_bin,
        log_path=log_dir / "network.wpr.log",
        no_archive_certificates=self._no_archive_certificates,
        rules_file=self._response_transformations_file,
        platform=self.host_platform,
        **extra_kwargs)


class RemoteWprReplayNetwork(WprReplayNetwork):

  def __init__(self, archive: LocalPath | str,
               traffic_shaper: Optional[TrafficShaper],
               wpr_go_bin: Optional[LocalPath], browser_platform: Platform,
               persist_server: bool, inject_deterministic_script: bool,
               no_archive_certificates: bool,
               response_transformations_file: LocalPath | None,
               host: str | None) -> None:
    super().__init__(
        archive=archive,
        traffic_shaper=traffic_shaper,
        wpr_go_bin=wpr_go_bin,
        browser_platform=browser_platform,
        persist_server=persist_server,
        inject_deterministic_script=inject_deterministic_script,
        no_archive_certificates=no_archive_certificates,
        response_transformations_file=response_transformations_file,
        cross_platform_mode=False,
        host=host)

  @classmethod
  def is_compatible(cls, platform: Platform) -> bool:
    return platform.is_android or platform.is_chromeos

  @override
  def _ensure_wpr_go(self, wpr_go_bin: Optional[LocalPath] = None) -> LocalPath:
    assert RemoteWprReplayNetwork.is_compatible(self.browser_platform)
    if wpr_go_bin:
      if wpr_go_bin.suffix == ".go":
        raise ValueError(f"Can't run .go files on {self.browser_platform}")
    else:
      wpr_go_bin = self._download_prebuilt_wpr()
    return self.host_platform.parse_local_binary_path(wpr_go_bin,
                                                      "wpr.go binary")

  def _download_prebuilt_wpr(self) -> LocalPath:
    wpr_cloud_binary: WprCloudBinary = WprGoFinder(
        self.host_platform).cloud_binary(self.browser_platform)
    local_wpr_go_bin = (
        self.host_platform.local_cache_dir("wpr") /
        str(self.browser_platform.machine) / "wpr_go")
    if not check_hash(local_wpr_go_bin, wpr_cloud_binary.file_hash):
      self.host_platform.download_gcs_file(wpr_cloud_binary.url,
                                           local_wpr_go_bin)
    assert check_hash(local_wpr_go_bin, wpr_cloud_binary.file_hash)
    return local_wpr_go_bin

  @contextlib.contextmanager
  @override
  def open(self: RemoteWprReplayNetwork,
           session: BrowserSessionRunGroup) -> Iterator[RemoteWprReplayNetwork]:
    with self._remote_temp_dir(session):
      with super().open(session):
        yield self

  @contextlib.contextmanager
  def _remote_temp_dir(self, session: BrowserSessionRunGroup) -> Iterator:
    with session.browser_platform.TemporaryDirectory() as tmp_dir:
      try:
        self._tmp_dir = tmp_dir
        yield
      finally:
        self._tmp_dir = None

  def _push_file(self, path: LocalPath) -> AnyPath:
    assert self._tmp_dir is not None
    remote_path = self._tmp_dir / path.name
    self.browser_platform.push(path, remote_path)
    return remote_path

  @override
  def _create_server(self, log_dir: LocalPath) -> WprReplayServer:
    assert not self._cross_platform_mode

    host_platform = self.host_platform
    if local_wpr_go := WprGoFinder(host_platform).local_path:
      wpr_root = local_wpr_go.parents[1]
    else:
      raise RuntimeError(f"Could not fine local wpr.go on {host_platform}")

    wpr_go_bin = self._push_file(self._wpr_go_bin)
    self.browser_platform.sh("chmod", "+x", wpr_go_bin)
    archive: AnyPath = self._push_file(self._archive_path)
    key_file: AnyPath = self._push_file(wpr_root / "ecdsa_key.pem")
    cert_file: AnyPath = self._push_file(wpr_root / "ecdsa_cert.pem")
    inject_scripts: list[AnyPath] = []
    if self._inject_deterministic_script:
      inject_scripts = [self._push_file(wpr_root / "deterministic.js")]
    rules_file: AnyPath | None = None
    if file := self._response_transformations_file:
      rules_file = self._push_file(file)
    for script in self._get_injected_scripts():
      self._push_file(script)
    return WprReplayServer(
        archive_path=archive,
        bin_path=wpr_go_bin,
        key_file=key_file,
        cert_file=cert_file,
        inject_scripts=inject_scripts,
        log_path=log_dir / "network.wpr.log",
        platform=self.browser_platform,
        rules_file=rules_file)

  def _get_injected_scripts(self) -> list[LocalPath]:
    if not self._response_transformations_file:
      return []

    with self._response_transformations_file.open() as f:
      transformations = json.load(f)
    assert isinstance(transformations, list)
    assert all(isinstance(t, dict) for t in transformations)

    transformations_dir: LocalPath = self._response_transformations_file.parent
    scripts: list[LocalPath] = []
    for transformation in transformations:
      if injected_script := transformation.get("InjectedScript"):
        script: LocalPath = transformations_dir / injected_script
        if not script.exists():
          raise ValueError(
              f"{self._response_transformations_file} attempts to inject "
              f"{script} but the script was not found")
        scripts.append(script)
    return scripts
