# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import dataclasses
import json
import logging
import subprocess
from typing import TYPE_CHECKING, Final, Iterator, Mapping, Optional, Sequence

from typing_extensions import override

from crossbench import path as pth

if TYPE_CHECKING:
  from crossbench.plt.base import Platform


class BasePathFinder(abc.ABC):

  def __init__(self, platform: Platform) -> None:
    self._platform: Final[Platform] = platform
    self._path: Final[pth.AnyPath | None] = self._find_path()
    if self._path and not self.is_valid_path(self._path):
      raise ValueError(f"Resolved binary path is not valid: {self._path}")

  @property
  def platform(self) -> Platform:
    return self._platform

  @property
  def path(self) -> pth.AnyPath | None:
    return self._path

  @property
  def local_path(self) -> pth.LocalPath | None:
    if path := self.path:
      return self.platform.local_path(path)
    return None

  def candidates(self) -> tuple[pth.AnyPath, ...]:
    return ()

  def _find_path(self) -> pth.AnyPath | None:
    # Try potential build location
    for candidate_path in self._iterate_candidates():
      if self.is_valid_path(candidate_path):
        return candidate_path
    return self._find_fallback_path()

  def _iterate_candidates(self) -> Iterator[pth.AnyPath]:
    yield from self.candidates()

  def _find_fallback_path(self) -> pth.AnyPath | None:
    return None

  @abc.abstractmethod
  def is_valid_path(self, candidate: pth.AnyPath) -> bool:
    pass


def default_chromium_candidates(platform: Platform) -> tuple[pth.AnyPath, ...]:
  """Returns a generous list of potential locations of a chromium checkout."""
  candidates = []
  if chromium_src := platform.environ.get("CHROMIUM_SRC"):
    candidates.append(platform.path(chromium_src))
  if platform.is_local:
    candidates.append(chromium_src_relative_local_path())
  if platform.is_android:
    return tuple(candidates)
  home_dir = platform.home()
  candidates += [
      # Guessing default locations
      home_dir / "Documents/chromium/src",
      home_dir / "chromium/src",
      platform.path("C:/src/chromium/src"),
      home_dir / "Documents/chrome/src",
      home_dir / "chrome/src",
      platform.path("C:/src/chrome/src"),
  ]
  return tuple(candidates)


def chromium_src_relative_local_path() -> pth.LocalPath:
  """Gets the local relative path of `chromium/src`.

  Assuming the cli.py path is `third_party/crossbench/crossbench/cli/cli.py`.
  """
  return pth.LocalPath(__file__).parents[4]


def is_chromium_checkout_dir(platform: Platform, dir_path: pth.AnyPath) -> bool:
  return (platform.is_dir(dir_path / "v8") and
          platform.is_dir(dir_path / "chrome") and
          platform.is_dir(dir_path / ".git"))


class ChromiumCheckoutFinder(BasePathFinder):
  """Finds a chromium src checkout at either given locations or at
  some preset known checkout locations."""

  @override
  def candidates(self) -> tuple[pth.AnyPath, ...]:
    return default_chromium_candidates(self.platform)

  @override
  def is_valid_path(self, candidate: pth.AnyPath) -> bool:
    return is_chromium_checkout_dir(self.platform, candidate)


class ChromiumBuildBinaryFinder(BasePathFinder):
  """Finds a custom-built binary in either a given out/BUILD dir or
  tries to find it in build dirs in common known chromium checkout locations."""

  BUILD_DIR_NAMES: Final[Sequence[str]] = ("Release", "release", "rel",
                                           "Optdebug", "optdebug", "opt")

  def __init__(self, platform: Platform, binary_name: str,
               candidates: tuple[pth.AnyPath, ...]) -> None:
    self._binary_name: Final[str] = binary_name
    self._candidates = candidates
    super().__init__(platform)

  @override
  def candidates(self) -> tuple[pth.AnyPath, ...]:
    return self._candidates

  @property
  def binary_name(self) -> str:
    return self._binary_name

  @override
  def _iterate_candidates(self) -> Iterator[pth.AnyPath]:
    # TOOD: Use candidates x search_paths like on Platform.
    for candidate_dir in self.candidates():
      yield candidate_dir / self._binary_name
      for build in self.BUILD_DIR_NAMES:
        yield candidate_dir / build / self._binary_name

    for candidate in default_chromium_candidates(self.platform):
      candidate_out = candidate / "out"
      if not self.platform.is_dir(candidate_out):
        continue
      # TODO: support remote glob
      for build in self.BUILD_DIR_NAMES:
        yield candidate_out / build / self._binary_name

  @override
  def is_valid_path(self, candidate: pth.AnyPath) -> bool:
    assert candidate.name == self._binary_name
    if not self.platform.is_file(candidate):
      return False
    # .../chromium/src/out/Release/BINARY => .../chromium/src/
    # Don't use parents[] access to stop at the root.
    maybe_checkout_dir = candidate.parent.parent.parent
    return is_chromium_checkout_dir(self._platform, maybe_checkout_dir)


class V8CheckoutFinder(BasePathFinder):

  @override
  def candidates(self) -> tuple[pth.AnyPath, ...]:
    if self.platform.is_android:
      return ()
    home_dir = self._platform.home()
    return (
        # V8 Checkouts
        home_dir / "Documents/v8/v8",
        home_dir / "v8/v8",
        self._platform.path("C:/src/v8/v8"),
        # Raw V8 checkouts
        home_dir / "Documents/v8",
        home_dir / "v8",
        self._platform.path("C:/src/v8/"),
    )

  @override
  def _find_fallback_path(self) -> pth.AnyPath | None:
    if chromium_checkout := ChromiumCheckoutFinder(self._platform).path:
      return chromium_checkout / "v8"
    maybe_d8_path = self.platform.environ.get("D8_PATH")
    if not maybe_d8_path:
      return None
    for candidate_dir in self.platform.path(maybe_d8_path).parents:
      if self.is_valid_path(candidate_dir):
        return candidate_dir
    return None

  @override
  def is_valid_path(self, candidate: pth.AnyPath) -> bool:
    v8_header_file = candidate / "include/v8.h"
    return (self.platform.is_file(v8_header_file) and
            (self.platform.is_dir(candidate / ".git")))


class V8ToolsFinder:
  """Helper class to find d8 binaries and the tick-processor.
  If no explicit d8 and checkout path are given, $D8_PATH and common v8 and
  chromium installation directories are checked."""

  def __init__(self,
               platform: Platform,
               d8_binary: Optional[pth.AnyPath] = None,
               v8_checkout: Optional[pth.AnyPath] = None) -> None:
    self.platform = platform
    self.d8_binary: pth.AnyPath | None = d8_binary
    self.v8_checkout: pth.AnyPath | None = None
    if v8_checkout:
      self.v8_checkout = v8_checkout
    else:
      self.v8_checkout = V8CheckoutFinder(self.platform).path
    self.tick_processor: pth.AnyPath | None = None
    self.d8_binary = self._find_d8()
    if self.d8_binary:
      self.tick_processor = self._find_v8_tick_processor()
    logging.debug("V8ToolsFinder found d8_binary='%s' tick_processor='%s'",
                  self.d8_binary, self.tick_processor)

  def _find_d8(self) -> Optional[pth.AnyPath]:
    if self.d8_binary and self.platform.is_file(self.d8_binary):
      return self.d8_binary
    environ = self.platform.environ
    if "D8_PATH" in environ:
      candidate = self.platform.path(environ["D8_PATH"]) / "d8"
      if self.platform.is_file(candidate):
        return candidate
      candidate = self.platform.path(environ["D8_PATH"])
      if self.platform.is_file(candidate):
        return candidate
    # Try potential build location
    for candidate_dir in V8CheckoutFinder(self.platform).candidates():
      for build_type in ("release", "optdebug", "Default", "Release"):
        candidates = list(
            self.platform.glob(candidate_dir, f"out/*{build_type}/d8"))
        if not candidates:
          continue
        d8_candidate = candidates[0]
        if self.platform.is_file(d8_candidate):
          return d8_candidate
    return None

  def _find_v8_tick_processor(self) -> pth.AnyPath | None:
    if self.platform.is_linux:
      tick_processor = "tools/linux-tick-processor"
    elif self.platform.is_macos:
      tick_processor = "tools/mac-tick-processor"
    elif self.platform.is_win:
      tick_processor = "tools/windows-tick-processor.bat"
    else:
      logging.debug(
          "Not looking for the v8 tick-processor on unsupported platform: %s",
          self.platform)
      return None
    if self.v8_checkout and self.platform.is_dir(self.v8_checkout):
      candidate = self.v8_checkout / tick_processor
      assert self.platform.is_file(candidate), (
          f"Provided v8_checkout has no '{tick_processor}' at {candidate}")
    assert self.d8_binary
    # Try inferring the V8 checkout from a built d8:
    # .../foo/v8/v8/out/x64.release/d8
    candidate = self.d8_binary.parents[2] / tick_processor
    if self.platform.is_file(candidate):
      return candidate
    if self.v8_checkout:
      candidate = self.v8_checkout / tick_processor
      if self.platform.is_file(candidate):
        return candidate
    return None


class BaseChromiumPathFinder(BasePathFinder, metaclass=abc.ABCMeta):

  @override
  def is_valid_path(self, candidate: pth.AnyPath) -> bool:
    return self._platform.is_file(candidate)

  @classmethod
  @abc.abstractmethod
  def chrome_path(cls) -> pth.AnyPath:
    """ Path within a chromium checkout. """
    raise NotImplementedError

  @override
  def candidates(self) -> tuple[pth.AnyPath, ...]:
    relative_path = chromium_src_relative_local_path() / self.chrome_path()
    if maybe_chrome := ChromiumCheckoutFinder(self._platform).path:
      return (relative_path, maybe_chrome / self.chrome_path(),)
    return (relative_path,)


class PerfettoFinder(BaseChromiumPathFinder, metaclass=abc.ABCMeta):

  @classmethod
  @abc.abstractmethod
  def binary_name(cls) -> str:
    pass

  @classmethod
  def perfetto_tools_dir(cls) -> pth.AnyPath:
    return pth.AnyPath("third_party/perfetto/tools")

  @classmethod
  @override
  def chrome_path(cls) -> pth.AnyPath:
    return cls.perfetto_tools_dir() / cls.binary_name()


class TraceconvFinder(PerfettoFinder):

  @classmethod
  @override
  def binary_name(cls) -> str:
    return "traceconv"


class TraceboxFinder(PerfettoFinder):

  @classmethod
  @override
  def binary_name(cls) -> str:
    return "tracebox"


class TraceProcessorFinder(PerfettoFinder):

  @classmethod
  @override
  def binary_name(cls) -> str:
    return "trace_processor"


CROSSBENCH_DIR: Final = pth.LocalPath(__file__).parents[2]


class BaseCrossbenchPathFinder(BaseChromiumPathFinder):

  @override
  def candidates(self) -> tuple[pth.AnyPath, ...]:
    candidates = super().candidates()
    return (CROSSBENCH_DIR / self.crossbench_path(),) + candidates

  @classmethod
  @abc.abstractmethod
  def crossbench_path(cls) -> pth.AnyPath:
    pass


@dataclasses.dataclass(frozen=True)
class WprCloudBinary:
  file_hash: str

  @property
  def url(self) -> str:
    return ("gs://chromium-telemetry/binary_dependencies/wpr_go_"
            f"{self.file_hash}")


class WprGoFinder(BaseCrossbenchPathFinder):
  # See binary_dependencies.json in the WebPageReplay repo.
  # Public for testing.
  WPR_PREBUILT_LOOKUP: Final[Mapping[tuple[str, str], str]] = {
      ("android", "arm64"): "linux_aarch64",
      ("android", "arm32"): "linux_armv7l",
      ("android", "x64"): "linux_x86_64",
      ("chromeos_ssh", "arm64"): "linux_aarch64",
      ("chromeos_ssh", "x64"): "linux_x86_64",
      ("linux", "x64"): "linux_x86_64",
      ("macos", "arm64"): "mac_arm64",
      ("macos", "x64"): "mac_x86_64",
      ("win", "x64"): "win_AMD64",
  }

  @classmethod
  @override
  def chrome_path(cls) -> pth.AnyPath:
    return pth.AnyPath("third_party/webpagereplay/src/wpr.go")

  @classmethod
  @override
  def crossbench_path(cls) -> pth.AnyPath:
    return pth.AnyPath("third_party/webpagereplay/src/wpr.go")


  # Info of a prebuilt WPR binary for `browser_platform`, stored in the cloud.
  def cloud_binary(self, browser_platform: Platform) -> WprCloudBinary:
    wpr_go_file = self.local_path
    if not wpr_go_file:
      raise RuntimeError("Could not find local wpr.go")

    with (wpr_go_file.parents[1] /
          "scripts/binary_dependencies.json").open() as file:
      hashes_json = json.load(file)
    platform_key = self.WPR_PREBUILT_LOOKUP[browser_platform.key]
    return WprCloudBinary(
        hashes_json["wpr_go"][platform_key]["cloud_storage_hash"])


class BuildtoolFinder(BaseChromiumPathFinder):

  @classmethod
  @override
  def chrome_path(cls) -> pth.AnyPath:
    return pth.AnyPath(
        "third_party/android_build_tools/bundletool/cipd/bundletool.jar")

  @override
  def candidates(self) -> tuple[pth.AnyPath, ...]:
    super_candidates: tuple[pth.AnyPath, ...] = super().candidates()
    if not self.platform.is_macos:
      return super_candidates

    try:
      brew_path: pth.LocalPath = self.platform.local_path(
          self.platform.sh_stdout("brew", "--prefix").strip("\n"))
    except FileNotFoundError:
      return super_candidates
    except subprocess.CalledProcessError:
      return super_candidates

    return super_candidates + (self.platform.local_path(
        brew_path / "bin/bundletool"),)


class TsProxyFinder(BaseCrossbenchPathFinder):

  @classmethod
  @override
  def chrome_path(cls) -> pth.AnyPath:
    return pth.AnyPath("third_party/catapult/third_party/tsproxy/tsproxy.py")

  @classmethod
  @override
  def crossbench_path(cls) -> pth.AnyPath:
    return pth.AnyPath("third_party/tsproxy/tsproxy.py")
