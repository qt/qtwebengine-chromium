# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import dataclasses
import argparse
import dataclasses
import pathlib
import re
from typing import (TYPE_CHECKING, Any, Dict, Final, Iterable, List, Optional,
                    TextIO, Tuple, Type, Union, cast)

import hjson

from crossbench import cli_helper, exception, plt

import crossbench.browsers.all as browsers
from crossbench.browsers.chrome.downloader import ChromeDownloader
from crossbench.browsers.firefox.downloader import FirefoxDownloader
from crossbench.cli.config.network import NetworkConfig, NetworkSpeedPreset
from crossbench.config import ConfigObject, ConfigParser

from .driver import DriverConfig, BrowserDriverType

SUPPORTED_BROWSER = ("chromium", "chrome", "safari", "edge", "firefox")

# Split inputs like:
# - "/out/x64.release/chrome"
# - "/out/x64.release/chrome:4G"
# - "C:\out\x64.release\chrome"
# - "C:\out\x64.release\chrome:4G"
# - "applescript:/out/x64.release/chrome"
# - "applescript:/out/x64.release/chrome:4G"
# - "selenium:C:\out\x64.release\chrome"
# - "selenium:C:\out\x64.release\chrome:4G"
NETWORK_PRESETS: str = "|".join(
    re.escape(preset.value) for preset in NetworkSpeedPreset)  # pytype: disable=missing-parameter
SHORT_FORM_RE = re.compile(r"((?P<driver>\w{3,}):)??"
                           r"(?P<path>([A-Z]:[/\\])?[^:]+)"
                           f"(:(?P<network>{NETWORK_PRESETS}))?")

@dataclasses.dataclass(frozen=True)
class BrowserConfig(ConfigObject):
  browser: Union[pathlib.Path, str]
  driver: DriverConfig = DriverConfig.default()
  network: NetworkConfig = NetworkConfig.default()

  def __post_init__(self) -> None:
    if not self.browser:
      raise ValueError(f"{type(self).__name__}.browser cannot be None.")
    if not self.driver:
      raise ValueError(f"{type(self).__name__}.driver cannot be None.")
    if not self.network:
      raise ValueError(f"{type(self).__name__}.driver cannot be None.")

  @classmethod
  def default(cls) -> BrowserConfig:
    return cls(browsers.Chrome.stable_path(), DriverConfig.default())

  @classmethod
  def loads(cls, value: str) -> BrowserConfig:
    if not value:
      raise argparse.ArgumentTypeError("Cannot parse empty string")
    network = NetworkConfig.default()
    driver = DriverConfig.default()
    path: Optional[Union[pathlib.Path, str]] = None
    if ":" not in value or cls.value_has_path_prefix(value):
      # Variant 1: $PATH_OR_IDENTIFIER
      path = cls._parse_path_or_identifier(value)
    elif value[0] != "{":
      # Variant 2: ${DRIVER_TYPE}:${PATH_OR_IDENTIFIER}:${NETWORK}
      driver, path, network = cls._parse_inline_short_form(value)
    else:
      # Variant 3: Full inline hjson
      config = cli_helper.parse_inline_hjson(value)
      with exception.annotate(f"Parsing inline {cls.__name__}"):
        return cls.load_dict(config)
    assert path, "Invalid path"
    assert network, "Invalid network"
    return cls(path, driver, network)


  @classmethod
  def _parse_path_or_identifier(
      cls,
      maybe_path_or_identifier: str,
      driver_type: Optional[BrowserDriverType] = None,
      driver: Optional[DriverConfig] = None) -> Union[str, pathlib.Path]:
    if not maybe_path_or_identifier:
      raise argparse.ArgumentTypeError("Got empty browser identifier.")
    if not driver_type:
      if driver:
        driver_type = driver.type
      else:
        driver_type = BrowserDriverType.default()
    identifier = maybe_path_or_identifier.lower()
    path = None
    if "/" in maybe_path_or_identifier or "\\" in maybe_path_or_identifier:
      # Assume a path since short-names never contain back-/slashes.
      if driver_type.is_remote:
        path = cli_helper.parse_path(maybe_path_or_identifier)
      else:
        path = cli_helper.parse_existing_path(maybe_path_or_identifier)
    else:
      if ":" in maybe_path_or_identifier:
        raise argparse.ArgumentTypeError(
            f"Got unexpected short-form string '{maybe_path_or_identifier}'. \n"
            "  Use it directly on the parent config attribute: \n"
            f"   {{my-browser: '{maybe_path_or_identifier}'}}")
      if maybe_path := cls._try_parse_short_name(identifier, driver_type):
        return maybe_path
      if ChromeDownloader.is_valid(maybe_path_or_identifier, plt.PLATFORM):
        return maybe_path_or_identifier
      if FirefoxDownloader.is_valid(maybe_path_or_identifier, plt.PLATFORM):
        return maybe_path_or_identifier
    if not path:
      path = cli_helper.try_resolve_existing_path(maybe_path_or_identifier)
      if not path:
        raise argparse.ArgumentTypeError(
            f"Unknown browser path or short name: '{maybe_path_or_identifier}'")
    if cls.is_supported_browser_path(path):
      return path
    raise argparse.ArgumentTypeError(f"Unsupported browser path='{path}'")

  @classmethod
  def _try_parse_short_name(
      cls, identifier: str,
      driver_type: BrowserDriverType) -> Optional[pathlib.Path]:
    # We're not using a dict-based lookup here, since not all browsers are
    # available on all platforms
    if identifier in ("chrome", "chrome-stable", "chr-stable", "chr"):
      if driver_type == BrowserDriverType.ANDROID:
        return pathlib.Path("com.android.chrome")
      return browsers.Chrome.stable_path()
    if identifier in ("chrome-beta", "chr-beta"):
      if driver_type == BrowserDriverType.ANDROID:
        return pathlib.Path("com.chrome.beta")
      return browsers.Chrome.beta_path()
    if identifier in ("chrome-dev", "chr-dev"):
      if driver_type == BrowserDriverType.ANDROID:
        return pathlib.Path("com.chrome.dev")
      return browsers.Chrome.dev_path()
    if identifier in ("chrome-canary", "chr-canary"):
      if driver_type == BrowserDriverType.ANDROID:
        return pathlib.Path("com.chrome.canary")
      return browsers.Chrome.canary_path()
    if identifier == "chromium":
      if driver_type == BrowserDriverType.ANDROID:
        return pathlib.Path("org.chromium.chrome")
      return browsers.Chromium.default_path()
    if identifier in ("edge", "edge-stable"):
      return browsers.Edge.stable_path()
    if identifier == "edge-beta":
      return browsers.Edge.beta_path()
    if identifier == "edge-dev":
      return browsers.Edge.dev_path()
    if identifier == "edge-canary":
      return browsers.Edge.canary_path()
    if identifier in ("safari", "sf"):
      return browsers.Safari.default_path()
    if identifier in ("safari-technology-preview", "safari-tp", "sf-tp", "tp"):
      return browsers.Safari.technology_preview_path()
    if identifier in ("firefox", "firefox-stable", "ff", "ff-stable"):
      return browsers.Firefox.default_path()
    if identifier in ("firefox-dev", "firefox-developer-edition", "ff-dev"):
      return browsers.Firefox.developer_edition_path()
    if identifier in ("firefox-nightly", "ff-nightly", "ff-trunk"):
      return browsers.Firefox.nightly_path()
    return None

  @classmethod
  def is_supported_browser_path(cls, path: pathlib.Path) -> bool:
    path_str = str(path).lower()
    for short_name in SUPPORTED_BROWSER:
      if short_name in path_str:
        return True
    return False

  @classmethod
  def _parse_inline_short_form(
      cls, value: str
  ) -> Tuple[DriverConfig, Union[str, pathlib.Path], NetworkConfig]:
    assert ":" in value
    match = SHORT_FORM_RE.fullmatch(value)
    if not match:
      raise argparse.ArgumentTypeError(
          f"Invalid browser short form: '{value}' \n"
          "A browser path/identifier and "
          "at least a driver or network preset have to be present")
    driver_identifier = match.group("driver")
    path_or_identifier = match.group("path")
    network_identifier = match.group("network")
    if not path_or_identifier:
      raise argparse.ArgumentTypeError(
          "Browser short form: missing path or browser identifier.")
    driver = DriverConfig.default()
    if driver_identifier is not None:
      driver = cast(DriverConfig, DriverConfig.parse(match.group("driver")))
    path: Union[str, pathlib.Path] = cls._parse_path_or_identifier(
        path_or_identifier, driver.type)
    network = NetworkConfig.default()
    if network_identifier is not None:
      network = NetworkConfig.loads(network_identifier)
    return (driver, path, network)

  @classmethod
  def load(cls, f: TextIO) -> BrowserConfig:
    with exception.annotate(f"Loading browser config file: {f.name}"):
      config = {}
      with exception.annotate(f"Parsing {hjson.__name__}"):
        config = hjson.load(f)
      with exception.annotate(f"Parsing config file: {f.name}"):
        return cls.load_dict(config)
    raise argparse.ArgumentTypeError(f"Could not parse : '{f.name}'")

  @classmethod
  def load_dict(cls, config: Dict[str, Any]) -> BrowserConfig:
    return cls.config_parse().parse(config)

  @classmethod
  def config_parse(cls) -> ConfigParser[BrowserConfig]:
    parser = ConfigParser("BrowserConfig parser", cls)
    parser.add_argument(
        "browser",
        aliases=("path",),
        type=cls._parse_path_or_identifier,
        required=True,
        depends_on=("driver",))
    parser.add_argument(
        "driver", type=DriverConfig, default=DriverConfig.default())
    parser.add_argument(
        "network",
        required=False,
        default=NetworkConfig.default(),
        type=NetworkConfig)
    return parser

  @property
  def path(self) -> pathlib.Path:
    assert isinstance(self.browser, pathlib.Path)
    return self.browser

  def get_platform(self) -> plt.Platform:
    return self.driver.get_platform()
