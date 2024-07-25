# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import dataclasses
import enum
import logging
import pathlib
import re
from typing import Any, Dict, List, Optional

from frozendict import frozendict

from crossbench import cli_helper, compat, plt
from crossbench.config import ConfigObject, ConfigParser


@enum.unique
class BrowserDriverType(compat.StrEnumWithHelp):
  WEB_DRIVER = ("WebDriver", "Use Selenium with webdriver, for local runs.")
  APPLE_SCRIPT = ("AppleScript", "Use AppleScript, for local macOS runs only")
  ANDROID = ("Android",
             "Use Webdriver for android. Allows to specify additional settings")
  IOS = ("iOS", "Placeholder, unsupported at the moment")
  LINUX_SSH = ("Remote Linux",
               "Use remote webdriver and execute commands via SSH")

  @classmethod
  def default(cls) -> BrowserDriverType:
    return cls.WEB_DRIVER

  @classmethod
  def parse(cls, value: str) -> BrowserDriverType:
    identifier = value.lower()
    if identifier == "":
      return BrowserDriverType.default()
    if identifier in ("", "selenium", "webdriver"):
      return BrowserDriverType.WEB_DRIVER
    if identifier in ("applescript", "osa"):
      return BrowserDriverType.APPLE_SCRIPT
    if identifier in ("android", "adb"):
      return BrowserDriverType.ANDROID
    if identifier in ("iphone", "ios"):
      return BrowserDriverType.IOS
    if identifier == "ssh":
      return BrowserDriverType.LINUX_SSH
    raise argparse.ArgumentTypeError(f"Unknown driver type: {value}")

  @property
  def is_remote(self):
    if self.name in ("ANDROID", "LINUX_SSH"):
      return True
    return False


class AmbiguousDriverIdentifier(argparse.ArgumentTypeError):
  pass


IOS_UUID_RE = re.compile(r"[0-9A-Z]+-[0-9A-Z-]+")


@dataclasses.dataclass(frozen=True)
class DriverConfig(ConfigObject):
  type: BrowserDriverType = BrowserDriverType.default()
  path: Optional[pathlib.Path] = None
  settings: Optional[frozendict] = None

  def __post_init__(self):
    if not self.type:
      raise ValueError(f"{type(self).__name__}.type cannot be None.")
    try:
      hash(self.settings)
    except ValueError as e:
      raise ValueError(
          f"settings must be hashable but got: {self.settings}") from e
    self.validate()

  def validate(self) -> None:
    if self.type == BrowserDriverType.ANDROID:
      self.validate_android()
    if self.type == BrowserDriverType.IOS:
      self.validate_ios()

  def validate_android(self) -> None:
    devices = plt.adb_devices(plt.PLATFORM)
    names = list(devices.keys())
    if not devices:
      raise argparse.ArgumentTypeError("No ADB devices attached.")
    if not self.settings:
      if len(devices) == 1:
        # Default device "adb" (no settings) with exactly one device is ok.
        return
      raise AmbiguousDriverIdentifier(
          f"{len(devices)} ADB devices connected: {names}. "
          "Please explicitly specify a device ID.")
    if serial := self.settings.get("serial"):
      if serial not in devices:
        raise argparse.ArgumentTypeError(
            f"Could not find ADB device with serial={serial}. "
            f"Choices are {names}.")

  def validate_ios(self) -> None:
    devices = plt.ios_devices(plt.PLATFORM)
    if not devices:
      raise argparse.ArgumentTypeError("No iOS devices attached.")
    names = list(map(str, devices))
    if not self.settings:
      if len(devices) == 1:
        # Default device "ios" (no settings) with exactly one device is ok.
        return
      raise AmbiguousDriverIdentifier(
          f"{len(devices)} ios devices connected: {names}. "
          "Please explicitly specify a device UUID.")
    if uuid := self.settings.get("uuid"):
      if uuid not in devices:
        raise argparse.ArgumentTypeError(
            f"Could not find ios device with serial={uuid}. "
            f"Choices are {names}.")

  @classmethod
  def default(cls) -> DriverConfig:
    return cls(BrowserDriverType.default())

  @classmethod
  def loads(cls, value: str) -> DriverConfig:
    if not value:
      raise argparse.ArgumentTypeError("Cannot parse empty string")
    # Variant 1: $PATH
    path: Optional[pathlib.Path] = cli_helper.try_resolve_existing_path(value)
    driver_type: BrowserDriverType = BrowserDriverType.default()
    if not path:
      if cls.value_has_path_prefix(value):
        raise argparse.ArgumentTypeError(f"Driver path does not exist: {value}")
      # Variant 2: $DRIVER_TYPE
      if "{" != value[0]:
        try:
          driver_type = BrowserDriverType.parse(value)
        except argparse.ArgumentTypeError as original_error:
          try:
            return cls.load_short_settings(value, plt.PLATFORM)
          except AmbiguousDriverIdentifier:  # pylint: disable=try-except-raise
            raise
          except ValueError as e:
            logging.debug("Parsing short inline driver config failed: %s", e)
            raise original_error from e
      else:
        # Variant 2: full hjson config
        data = cli_helper.parse_inline_hjson(value)
        return cls.load_dict(data)
    if path and path.stat().st_size == 0:
      raise argparse.ArgumentTypeError(f"Driver path is empty file: {path}")
    return DriverConfig(driver_type, path)

  @classmethod
  def load_short_settings(cls, value: str,
                          platform: plt.Platform) -> DriverConfig:
    """Check for short versions and multiple candidates"""
    logging.debug("Looking for driver candidates: %s", value)
    candidate: Optional[DriverConfig]
    if candidate := cls.try_load_adb_settings(value, platform):
      return candidate
    if platform.is_macos:
      if candiate := cls.try_load_ios_settings(value, platform):
        return candiate
    # TODO: add more custom parsing here
    raise ValueError("Unknown setting")

  @classmethod
  def try_load_adb_settings(cls, value: str,
                            platform: plt.Platform) -> Optional[DriverConfig]:
    candidate_serials: List[str] = []
    pattern: re.Pattern = cls.compile_search_pattern(value)
    for serial, info in plt.adb_devices(platform).items():
      if pattern.fullmatch(serial):
        candidate_serials.append(serial)
        continue
      for key, info_value in info.items():
        if (pattern.fullmatch(f"{key}:{info_value}") or
            pattern.fullmatch(info_value)):
          candidate_serials.append(serial)
          break
    if len(candidate_serials) > 1:
      raise AmbiguousDriverIdentifier(
          "Found more than one adb devices matching "
          f"'{value}': {candidate_serials}")
    if len(candidate_serials) == 0:
      logging.debug("No matching adb devices found.")
      return None
    assert len(candidate_serials) == 1
    return DriverConfig(
        BrowserDriverType.ANDROID,
        settings=frozendict(serial=candidate_serials[0]))

  @classmethod
  def try_load_ios_settings(cls, value: str,
                            platform: plt.Platform) -> Optional[DriverConfig]:
    candidate_serials: List[str] = []
    pattern: re.Pattern = cls.compile_search_pattern(value)
    for uuid, device_info in plt.ios_devices(platform).items():
      if pattern.fullmatch(uuid):
        candidate_serials.append(uuid)
        continue
      if pattern.fullmatch(device_info.name):
        candidate_serials.append(uuid)
        continue
    if len(candidate_serials) > 1:
      raise AmbiguousDriverIdentifier(
          "Found more than one ios devices matching "
          f"'{value}': {candidate_serials}")
    if len(candidate_serials) == 0:
      logging.debug("No matching ios devices found.")
      return None
    assert len(candidate_serials) == 1
    return DriverConfig(
        BrowserDriverType.IOS, settings=frozendict(uuid=candidate_serials[0]))

  @classmethod
  def compile_search_pattern(cls, maybe_pattern: str) -> re.Pattern:
    try:
      return re.compile(maybe_pattern)
    except Exception as e:
      logging.debug(
          "Falling back to full string match for "
          "invalid regexp search pattern: %s %s", maybe_pattern, e)
      return re.compile(re.escape(maybe_pattern))

  @classmethod
  def load_dict(cls, config: Dict[str, Any]) -> DriverConfig:
    return cls.config_parser().parse(config)

  @classmethod
  def config_parser(cls) -> ConfigParser[DriverConfig]:
    parser = ConfigParser("DriverConfig parser", cls)
    parser.add_argument(
        "type",
        type=BrowserDriverType.parse,
        default=BrowserDriverType.default())
    parser.add_argument(
        "settings",
        type=frozendict,
        help="Additional driver settings (Driver dependent).")
    return parser

  def get_platform(self) -> plt.Platform:
    if self.type == BrowserDriverType.ANDROID:
      device_identifier = None
      if self.settings:
        device_identifier = self.settings.get("serial", None)
      return plt.AndroidAdbPlatform(plt.PLATFORM, device_identifier)
    if self.type == BrowserDriverType.IOS:
      # TODO(cbruni): use `xcrun xctrace list devices` to find the UDID
      # for attached simulators or devices. Currently only a single device
      # is supported
      pass
    if self.type == BrowserDriverType.LINUX_SSH:
      assert self.settings
      host = cli_helper.parse_non_empty_str(self.settings.get("host"), "host")
      port = cli_helper.parse_port(self.settings.get("port"), "port")
      ssh_port = cli_helper.parse_port(
          self.settings.get("ssh_port"), "ssh port")
      ssh_user = cli_helper.parse_non_empty_str(
          self.settings.get("ssh_user"), "ssh user")
      return plt.LinuxSshPlatform(
          plt.PLATFORM,
          host=host,
          port=port,
          ssh_port=ssh_port,
          ssh_user=ssh_user)
    return plt.PLATFORM
