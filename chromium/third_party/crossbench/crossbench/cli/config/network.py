# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import dataclasses
import enum
from typing import TYPE_CHECKING, Any, Dict, Optional

from crossbench import cli_helper, exception
from crossbench import path as pth
from crossbench.config import ConfigEnum, ConfigObject, ConfigParser
from crossbench.network.base import Network
from crossbench.network.live import LiveNetwork
from crossbench.network.local_fileserver import LocalFileNetwork
from crossbench.network.replay.wpr import GS_PREFIX, WprReplayNetwork
from crossbench.network.traffic_shaping import ts_proxy
from crossbench.network.traffic_shaping.base import (NoTrafficShaper,
                                                     TrafficShaper)
from crossbench.plt.base import Platform


@enum.unique
class NetworkType(ConfigEnum):
  LIVE: "NetworkType" = ("live", "Live network.")
  WPR: "NetworkType" = ("wpr", "Replayed network from a wpr.go archive.")
  LOCAL: "NetworkType" = ("local",
                          "Serve content from a local http file server.")


def _settings_str(name: str) -> str:
  settings = ts_proxy.TRAFFIC_SETTINGS[name]
  return (f"rtt={settings['rtt_ms']}ms, "
          f"in={settings['in_kbps']} kbps,"
          f"out={settings['out_kbps']} kbps")


@enum.unique
class NetworkSpeedPreset(ConfigEnum):
  """Presets that match ts_proxy settings."""
  LIVE: "NetworkSpeedPreset" = ("live", "Untroubled default network settings")
  MOBILE_3G_SLOW: "NetworkSpeedPreset" = (
      "3G-slow", f"Slow 3G network settings: {_settings_str('3G-slow')}")
  MOBILE_3G_REGULAR: "NetworkSpeedPreset" = (
      "3G-regular",
      f"Regular 3G network settings: {_settings_str('3G-regular')}")
  MOBILE_3G_FAST: "NetworkSpeedPreset" = (
      "3G-fast", f"Slow 3G network settings: {_settings_str('3G-fast')}")
  MOBILE_4G: "NetworkSpeedPreset" = (
      "4G", f"Regular 4G network settings: {_settings_str('4G')}")


@dataclasses.dataclass(frozen=True)
class NetworkSpeedConfig(ConfigObject):
  ts_proxy: Optional[pth.RemotePath] = None
  rtt_ms: Optional[int] = None
  in_kbps: Optional[int] = None
  out_kbps: Optional[int] = None
  window: Optional[int] = None

  @classmethod
  def default(cls) -> NetworkSpeedConfig:
    return NetworkSpeedConfig()

  @classmethod
  def parse(cls, value: Any) -> NetworkSpeedConfig:
    if isinstance(value, NetworkSpeedPreset):
      return cls.load_preset(value)
    return super().parse(value)

  @classmethod
  def loads(cls, value: str) -> NetworkSpeedConfig:
    if not value:
      raise argparse.ArgumentTypeError("Cannot parse empty string")
    if value == "default":
      return cls.default()
    preset = NetworkSpeedPreset.parse(value)  # pytype: disable=wrong-arg-types
    return cls.load_preset(preset)

  @classmethod
  def load_preset(cls, preset: NetworkSpeedPreset) -> NetworkSpeedConfig:
    if preset == NetworkSpeedPreset.LIVE:
      return cls.default()
    preset_kwargs = ts_proxy.TRAFFIC_SETTINGS[str(preset)]
    return cls(**preset_kwargs)

  @classmethod
  def load_dict(cls, config: Dict[str, Any]) -> NetworkSpeedConfig:
    return cls.config_parser().parse(config)

  @classmethod
  def config_parser(cls) -> ConfigParser[NetworkSpeedConfig]:
    parser = ConfigParser(
        "NetworkSpeedConfig parser", cls, default=NetworkSpeedConfig.default())
    parser.add_argument(
        "ts_proxy", type=cli_helper.parse_existing_file_path, required=False)
    # See tsproxy.py --help
    parser.add_argument(
        "rtt_ms",
        type=cli_helper.parse_positive_int,
        help="Round Trip Time Latency (in ms).")
    parser.add_argument(
        "in_kbps",
        type=cli_helper.parse_positive_int,
        help="Download Bandwidth (in 1000 bits/s - Kbps).")
    parser.add_argument(
        "out_kbps",
        type=cli_helper.parse_positive_int,
        help="Upload Bandwidth (in 1000 bits/s - Kbps).")
    parser.add_argument(
        "window",
        default=10,
        type=cli_helper.parse_positive_int,
        help="Emulated TCP initial congestion window (defaults to 10).")
    return parser

  @classmethod
  def help(cls) -> str:
    return cls.config_parser().help

  @property
  def is_live(self):
    return self == self.default()


@dataclasses.dataclass(frozen=True)
class NetworkConfig(ConfigObject):
  type: NetworkType = NetworkType.LIVE
  speed: NetworkSpeedConfig = NetworkSpeedConfig.default()
  path: Optional[pth.LocalPath] = None
  url: Optional[str] = None
  wpr_go_bin: Optional[pth.LocalPath] = None

  ARCHIVE_EXTENSIONS = (".archive", ".wprgo")
  VALID_EXTENSIONS = ConfigObject.VALID_EXTENSIONS + ARCHIVE_EXTENSIONS

  @classmethod
  def default(cls) -> NetworkConfig:
    return NetworkConfig()

  @classmethod
  def config_parser(cls) -> ConfigParser[NetworkConfig]:
    parser = ConfigParser(
        "NetworkConfig parser", cls, default=NetworkConfig.default())
    parser.add_argument("type", type=NetworkType, default=NetworkType.LIVE)
    parser.add_argument(
        "speed", type=NetworkSpeedConfig, default=NetworkSpeedConfig.default())
    parser.add_argument(
        "path", type=cli_helper.parse_existing_file_path, required=False)
    parser.add_argument("url", type=str, required=False)
    parser.add_argument(
        "wpr_go_bin",
        type=cli_helper.parse_existing_file_path,
        required=False,
        help=("Location of the wpr.go binary or source, "
              "used for WPR replay network. "
              "If not specified, a default lookup in known locations is used."))
    return parser

  @classmethod
  def help(cls) -> str:
    return cls.config_parser().help

  @classmethod
  def parse_wpr(cls, value: Any) -> NetworkConfig:
    config: NetworkConfig = cls.parse(value)
    if config.type != NetworkType.WPR:
      raise argparse.ArgumentTypeError(f"Expected wpr, but got {config.type}")
    return config

  @classmethod
  def parse_local(cls, value: Any) -> NetworkConfig:
    local_server_dir: pth.LocalPath = cli_helper.parse_dir_path(value)
    config: NetworkConfig = cls.load_path(local_server_dir)
    if config.type != NetworkType.LOCAL:
      raise argparse.ArgumentTypeError(
          f"Expected local file server, but got {config.type}. ")
    return config

  @classmethod
  def loads(cls, value: str) -> NetworkConfig:
    if not value:
      raise argparse.ArgumentTypeError("Network: Cannot parse empty string")
    if value == "default":
      return cls.default()
    if value[0] == "{":
      return cls.load_inline_hjson(value)
    # TODO(346197734): Move to load_url once available.
    if value.startswith(GS_PREFIX):
      return cls.load_wpr_archive_url(value)
    return cls.load_live(value)

  @classmethod
  def load_live(cls, value: Any) -> NetworkConfig:
    with exception.annotate_argparsing("Live network with speed config"):
      speed = NetworkSpeedConfig.parse(value)
      return cls(NetworkType.LIVE, speed)
    raise exception.UnreachableError()

  @classmethod
  def is_valid_path(cls, path: pth.LocalPath) -> bool:
    if path.suffix in cls.ARCHIVE_EXTENSIONS:
      return True
    # for local file server
    if path.is_dir():
      return True
    return super().is_valid_path(path)

  @classmethod
  def load_path(cls, path: pth.LocalPath) -> NetworkConfig:
    if path.suffix in cls.ARCHIVE_EXTENSIONS:
      return cls.load_wpr_archive_path(path)
    if path.is_dir():
      return NetworkConfig(NetworkType.LOCAL, path=path)
    return super().load_path(path)

  @classmethod
  def load_wpr_archive_path(cls, path: pth.LocalPath) -> NetworkConfig:
    path = cli_helper.parse_non_empty_file_path(path, "wpr.go archive")
    return NetworkConfig(type=NetworkType.WPR, path=path)

  @classmethod
  def load_wpr_archive_url(cls, url: str) -> NetworkConfig:
    return NetworkConfig(type=NetworkType.WPR, url=url)

  @classmethod
  def load_dict(cls, config: Dict[str, Any]) -> NetworkConfig:
    return cls.config_parser().parse(config)

  def validate(self) -> None:
    if not self.type:
      raise argparse.ArgumentTypeError("Missing NetworkConfig.type.")
    if not self.speed and isinstance(self.speed, NetworkSpeedConfig):
      raise argparse.ArgumentTypeError("Missing NetworkConfig.speed.")
    if self.type == NetworkType.LIVE:
      if self.path:
        raise argparse.ArgumentTypeError(
            "NetworkConfig path cannot be used with type=live")
    elif self.type is NetworkType.WPR:
      if not self.path and not self.url:
        raise argparse.ArgumentTypeError(
            "NetworkConfig with type=replay requires "
            "a valid wpr.go archive path or download url.")
      if self.path and self.url:
        raise argparse.ArgumentTypeError(
            "NetworkConfig with type=replay requires "
            "either archive path or download url but not both.")
    elif self.type is NetworkType.LOCAL:
      if not self.path:
        raise argparse.ArgumentTypeError(
            "NetworkConfig with type=local requires "
            "a valid local dir path to serve files.")
      cli_helper.parse_non_empty_dir_path(self.path, "local-serve dir")
    if self.wpr_go_bin and self.type is not NetworkType.WPR:
      raise argparse.ArgumentTypeError(
          "wpr_go_bin can only be used for the WPR replay network")

  def create(self, browser_platform: Platform) -> Network:
    with exception.annotate_argparsing(
        f"Setting up {self.type} network for {browser_platform}"):
      runner_platform: Platform = browser_platform.host_platform
      traffic_shaper = self._create_traffic_shaper(browser_platform)
      if self.type is NetworkType.LIVE:
        return LiveNetwork(traffic_shaper, runner_platform)
      if self.type is NetworkType.LOCAL:
        assert self.path
        return LocalFileNetwork(self.path, traffic_shaper, runner_platform)
      if self.type is NetworkType.WPR:
        return WprReplayNetwork(
            self.url or str(self.path), traffic_shaper, self.wpr_go_bin,
            runner_platform)
    raise ValueError(f"Unknown network type {self.type}")

  def _create_traffic_shaper(self, browser_platform: Platform) -> TrafficShaper:
    if self.speed.is_live:
      return NoTrafficShaper(browser_platform)
    return ts_proxy.TsProxyTrafficShaper(browser_platform, self.speed.ts_proxy,
                                         self.speed.rtt_ms, self.speed.in_kbps,
                                         self.speed.out_kbps, self.speed.window)
