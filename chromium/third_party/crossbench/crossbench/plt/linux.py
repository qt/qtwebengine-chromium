# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import dataclasses
import functools
import os
import re
from typing import (TYPE_CHECKING, Any, ClassVar, Dict, Iterator, List,
                    Optional, Tuple, Type)

from typing_extensions import override

from crossbench import path as pth
from crossbench.parse import NumberParser
from crossbench.plt.base import SubprocessError
from crossbench.plt.posix import PosixPlatform
from crossbench.plt.remote import RemotePlatformMixin
from crossbench.plt.signals import LinuxSignals

if TYPE_CHECKING:
  from crossbench.plt.display_info import DisplayInfo


@dataclasses.dataclass
class XrandrDisplayInfo:
  RESOLUTION_RE: ClassVar[re.Pattern] = re.compile(
      r"(?P<resX>[0-9]+)x(?P<resY>[0-9]+)")
  REFRESH_RATE_RE: ClassVar[re.Pattern] = re.compile(r"(?P<freq>[0-9.]+)\*")

  header: str
  resolutions: List[str] = dataclasses.field(default_factory=list)

  def is_connected(self) -> bool:
    return "disconnected" not in self.header

  def resolution(self) -> Tuple[int, int] | None:
    if match := self.RESOLUTION_RE.search(self.header):
      return (NumberParser.positive_int(match.group("resX")),
              NumberParser.positive_int(match.group("resY")))
    return None

  def refresh_rate(self) -> float:
    for resolution in self.resolutions:
      # The current refresh ret is marked with a `*`:
      if match := self.REFRESH_RATE_RE.search(resolution):
        return NumberParser.positive_float(match.group("freq"))
    return -1


def parse_display_xrandr(xrandr_str: str) -> Iterator[DisplayInfo]:
  """ Parse xrandr output:
  Screen 0: minimum 64 x 64, current 1728 x 946, maximum 32767 x 32767
  DUMMY0 connected primary 1728x946+0+0 456mm x 249mm
    1024x768      60.00  
    1024x576      59.90
    CRD_78       120.00* 
    ...
  DUMMY1 disconnected
    1600x1200_60  60.00
    ...
  """
  display_infos: List[XrandrDisplayInfo] = []
  current_info: XrandrDisplayInfo | None = None
  # Group display info and resolution entries:
  for line in xrandr_str.splitlines():
    if "connected" in line:
      current_info = XrandrDisplayInfo(line)
      display_infos.append(current_info)
    if current_info and line.startswith(" "):
      current_info.resolutions.append(line.strip())
  # Filter by connected displays and extract the resolution.
  for display_info in display_infos:
    if not display_info.is_connected():
      continue
    if resolution := display_info.resolution():
      yield {
          "resolution": resolution,
          "refresh_rate": display_info.refresh_rate(),
      }


class LinuxPlatform(PosixPlatform):
  SEARCH_PATHS: Tuple[pth.AnyPath, ...] = (
      pth.AnyPosixPath("."),
      pth.AnyPosixPath("/usr/local/sbin"),
      pth.AnyPosixPath("/usr/local/bin"),
      pth.AnyPosixPath("/usr/sbin"),
      pth.AnyPosixPath("/usr/bin"),
      pth.AnyPosixPath("/sbin"),
      pth.AnyPosixPath("/bin"),
      pth.AnyPosixPath("/opt/google"),
  )

  @property
  @override
  def is_linux(self) -> bool:
    return True

  @property
  @override
  def name(self) -> str:
    return "linux"

  @property
  def signals(self) -> Type[LinuxSignals]:
    return LinuxSignals

  def check_system_monitoring(self, disable: bool = False) -> bool:
    return True

  @functools.cached_property
  @override
  def device(self) -> str:  #pylint: disable=invalid-overridden-method
    try:
      id_dir = self.path("/sys/devices/virtual/dmi/id")
      vendor = self.cat(id_dir / "sys_vendor").strip()
      product = self.cat(id_dir / "product_name").strip()
      return f"{vendor} {product}"
    except (FileNotFoundError, SubprocessError):
      return "UNKNOWN"

  @functools.cached_property
  @override
  def cpu(self) -> str:  #pylint: disable=invalid-overridden-method
    cpu_str = "UNKNOWN"
    for line in self.cat(self.path("/proc/cpuinfo")).splitlines():
      if line.startswith("model name"):
        _, cpu_str = line.split(":", maxsplit=2)
        break
    if num_cores := self.cpu_cores:
      cpu_str = f"{cpu_str} {num_cores} cores"
    return cpu_str

  @property
  @override
  def has_display(self) -> bool:
    return "DISPLAY" in os.environ

  @property
  @override
  def is_battery_powered(self) -> bool:
    if self.is_local:
      return super().is_battery_powered
    if on_ac_power := self.which("on_ac_power"):
      return self.sh(on_ac_power, check=False).returncode == 1
    return False

  @functools.lru_cache(maxsize=1)
  @override
  def system_details(self) -> Dict[str, Any]:
    details = super().system_details()
    for info_bin in ("lscpu", "inxi"):
      if info_bin_path := self.which(info_bin):
        details[info_bin] = self.sh_stdout(info_bin_path)
    return details

  def search_binary(self, app_or_bin: pth.AnyPathLike) -> Optional[pth.AnyPath]:
    app_or_bin_path: pth.AnyPath = self.path(app_or_bin)
    if not app_or_bin_path.parts:
      raise ValueError("Got empty path")
    if result_path := self.which(app_or_bin_path):
      if not self.exists(result_path):
        raise RuntimeError(f"{result_path} does not exist.")
      return result_path
    for path in self.SEARCH_PATHS:
      # Recreate Path object for easier pyfakefs testing
      result_path = self.path(path) / app_or_bin_path
      if self.exists(result_path):
        return result_path
    return None

  def screenshot(self, result_path: pth.AnyPath) -> None:
    # TODO: maybe use imagemagick's 'import' as more portable alternative
    self.sh("gnome-screenshot", "--file", result_path)

  @functools.lru_cache(maxsize=1)
  def display_details(self) -> Tuple[DisplayInfo, ...]:
    if not self.has_display:
      return tuple()
    if xrandr_str := self.sh_stdout("xrandr"):
      return tuple(parse_display_xrandr(xrandr_str))
    return tuple()


class RemoteLinuxPlatform(RemotePlatformMixin, LinuxPlatform):
  pass
