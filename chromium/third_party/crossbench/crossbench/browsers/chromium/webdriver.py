# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import atexit
import datetime as dt
import json
import logging
import os
import re
import shutil
import stat
import tempfile
import urllib.error
import zipfile
from typing import (TYPE_CHECKING, Any, Dict, Final, Iterable, List, Optional,
                    Sequence, Tuple, Type, cast)

import hjson
from immutabledict import immutabledict
from selenium.webdriver.chromium.options import ChromiumOptions
from selenium.webdriver.chromium.service import ChromiumService
from selenium.webdriver.chromium.webdriver import ChromiumDriver
from selenium.webdriver.remote.webdriver import WebDriver as RemoteWebDriver

from crossbench import exception, helper
from crossbench import path as pth
from crossbench import plt
from crossbench.browsers.attributes import BrowserAttributes
from crossbench.browsers.chromium.chromium import Chromium
from crossbench.browsers.chromium.version import (ChromeDriverVersion,
                                                  ChromiumVersion)
from crossbench.browsers.webdriver import WebDriverBrowser
from crossbench.cli.config.secret_type import SecretType
from crossbench.flags.chrome import ChromeFlags
from crossbench.plt.android_adb import AndroidAdbPlatform
from crossbench.plt.chromeos_ssh import ChromeOsSshPlatform
from crossbench.plt.linux_ssh import LinuxSshPlatform

if TYPE_CHECKING:
  from selenium import webdriver

  from crossbench.browsers.settings import Settings
  from crossbench.cli.config.secrets import Secret
  from crossbench.flags.base import FlagsT
  from crossbench.plt.base import Platform
  from crossbench.runner.groups.session import BrowserSessionRunGroup


class ChromiumWebDriver(WebDriverBrowser, Chromium, metaclass=abc.ABCMeta):

  WEB_DRIVER_OPTIONS: Type[ChromiumOptions] = ChromiumOptions
  WEB_DRIVER_SERVICE: Type[ChromiumService] = ChromiumService

  @property
  def attributes(self) -> BrowserAttributes:
    return (BrowserAttributes.CHROMIUM | BrowserAttributes.CHROMIUM_BASED
            | BrowserAttributes.WEBDRIVER)

  def use_local_chromedriver(self) -> bool:
    return self.major_version == 0 or self.is_locally_compiled()

  def is_locally_compiled(self) -> bool:
    return pth.LocalPath(self.app_path.parent / "args.gn").exists()

  def _execute_cdp_cmd(self, driver: webdriver.Remote, cmd: str,
                       cmd_args: dict):
    return driver.execute("executeCdpCommand", {
        "cmd": cmd,
        "params": cmd_args
    })["value"]

  def _find_driver(self) -> pth.AnyPath:
    if self._driver_path:
      return self._driver_path
    finder = ChromeDriverFinder(self)
    assert self.app_path
    if self.use_local_chromedriver():
      return finder.find_local_build()
    try:
      return finder.download()
    except DriverNotFoundError as original_download_error:
      logging.debug(
          "Could not download chromedriver, "
          "falling back to finding local build: %s", original_download_error)
      try:
        return finder.find_local_build()
      except DriverNotFoundError as e:
        logging.debug("Could not find fallback chromedriver: %s", e)
        raise original_download_error from e
      # to make an old pytype version happy
      return pth.LocalPath()

  def _start_driver(self, session: BrowserSessionRunGroup,
                    driver_path: pth.AnyPath) -> webdriver.Remote:
    return self._start_chromedriver(session, driver_path)

  def _start_chromedriver(self, session: BrowserSessionRunGroup,
                          driver_path: pth.AnyPath) -> ChromiumDriver:
    assert not self._is_running
    assert self.log_file
    args = self._get_browser_flags_for_session(session)
    options = self._create_options(session, args)

    self._log_browser_start(args, driver_path)
    service_args: List[str] = []
    log_path: Optional[str] = None
    if self._settings.driver_logging:
      service_args += ["--verbose"]
      log_path = os.fspath(self.driver_log_file)
    # pytype: disable=wrong-keyword-args
    service = self.WEB_DRIVER_SERVICE(
        executable_path=os.fspath(driver_path),
        log_path=log_path,
        service_args=service_args)
    # TODO: support remote platforms
    service.log_file = pth.LocalPath(self.stdout_log_file).open(  # pylint: disable=consider-using-with
        "w", encoding="utf-8")
    driver = self._create_driver(options, service)
    # pytype: enable=wrong-keyword-args
    # Prevent debugging overhead.
    self._execute_cdp_cmd(driver, "Runtime.setMaxCallStackSizeToCapture",
                          {"size": 0})
    return driver

  def _create_options(self, session: BrowserSessionRunGroup,
                      args: Sequence[str]) -> ChromiumOptions:
    assert not self._is_running
    options: ChromiumOptions = self.WEB_DRIVER_OPTIONS()
    options.set_capability("browserVersion", str(self.major_version))
    # Don't wait for document-ready.
    options.set_capability("pageLoadStrategy", "eager")
    for arg in args:
      options.add_argument(arg)
    options.binary_location = os.fspath(self.path)
    session.setup_selenium_options(options)
    return options

  @abc.abstractmethod
  def _create_driver(self, options: ChromiumOptions,
                     service: ChromiumService) -> ChromiumDriver:
    pass

  def _validate_driver_version(self) -> None:
    assert self._driver_path, "No driver available"
    error_message = None
    if self.is_local and is_build_dir(
        self.platform.local_path(self.app_path.parent)):
      error_message = self._validate_locally_built_driver(
          self.platform.local_path(self._driver_path))
    else:
      error_message = self._validate_any_driver_version(self._driver_path)
    if error_message:
      raise RuntimeError("\n".join(error_message))

  def _validate_locally_built_driver(
      self, driver_path: pth.LocalPath) -> Optional[Iterable[str]]:
    # TODO: migrate to version object on the browser
    browser_version = ChromiumVersion.parse(self.version)
    driver_version = ChromeDriverVersion.parse(
        self.platform.app_version(driver_path))
    if browser_version.parts == driver_version.parts:
      return None
    return (f"Chromedriver version mismatch: driver={driver_version.parts_str} "
            f"browser={browser_version.parts_str} ({self}).",
            build_chromedriver_instructions(driver_path.parent))

  def _validate_any_driver_version(
      self, driver_path: pth.AnyPath) -> Optional[Iterable[str]]:
    raw_version_str = self.host_platform.sh_stdout(driver_path, "--version")
    driver_version = ChromeDriverVersion.parse(raw_version_str)
    if driver_version.major == self.major_version:
      return None
    return (f"Chromedriver version mismatch: driver={driver_version} "
            f"browser={self.version} ({self})",)

  def run_script_on_new_document(self, script: str) -> None:
    self._execute_cdp_cmd(self._private_driver,
                          "Page.addScriptToEvaluateOnNewDocument",
                          {"source": script})

  def current_window_id(self) -> str:
    return str(self._private_driver.current_window_handle)

  def switch_window(self, window_id: str) -> None:
    self._private_driver.switch_to.window(window_id)

  def switch_tab(
      self,
      title: Optional[re.Pattern] = None,
      url: Optional[re.Pattern] = None,
      tab_index: Optional[int] = None,
      timeout: dt.timedelta = dt.timedelta(seconds=0)
  ) -> None:
    driver = self._private_driver
    original_handle = driver.current_window_handle
    for _ in helper.wait_with_backoff(timeout, self.platform):
      # Search through other handles starting from current_window_handle + 1
      try:
        i = driver.window_handles.index(original_handle)
      except ValueError as e:
        raise RuntimeError("Original starting tab no longer exists") from e

      if tab_index is not None:
        handles = [driver.window_handles[tab_index]]
      else:
        handles = driver.window_handles[i + 1:] + driver.window_handles[:i]

      for handle in handles:
        driver.switch_to.window(handle)
        if title is not None:
          if title.match(driver.title) is None:
            continue
        if url is not None:
          if url.match(driver.current_url) is None:
            continue
        return
    error = "No new tab found"
    if title is not None:
      error += f" with title matching {repr(title.pattern)}"
    if url is not None:
      error += f" with url matching {repr(url.pattern)}"
    if tab_index is not None:
      error += f" with tab_index matching {tab_index}"
    raise RuntimeError(error)

  def start_profiling(self) -> None:
    assert isinstance(self._private_driver, ChromiumDriver)
    # TODO: reuse the TraceProbe categories,
    self._execute_cdp_cmd(
        self._private_driver, "Tracing.start", {
            "transferMode":
                "ReturnAsStream",
            "includedCategories": [
                "devtools.timeline",
                "v8.execute",
                "disabled-by-default-devtools.timeline",
                "disabled-by-default-devtools.timeline.frame",
                "toplevel",
                "blink.console",
                "blink.user_timing",
                "latencyInfo",
                "disabled-by-default-devtools.timeline.stack",
                "disabled-by-default-v8.cpu_profiler",
            ],
        })

  def stop_profiling(self) -> Any:
    assert isinstance(self._private_driver, ChromiumDriver)
    data = self._execute_cdp_cmd(self._private_driver,
                                 "Tracing.tracingComplete", {})
    # TODO: use webdriver bidi to get the async Tracing.end event.
    # self._execute_cdp_cmd(self._driver, "Tracing.end", {})
    return data


# Android is high-tech and reads chrome flags from an app-specific file.
# TODO: extend support to more than just chrome.
_FLAG_ROOT: pth.AnyPosixPath = pth.AnyPosixPath("/data/local/tmp/")
FLAGS_WEBLAYER: pth.AnyPosixPath = _FLAG_ROOT / "weblayer-command-line"
FLAGS_WEBVIEW: pth.AnyPosixPath = _FLAG_ROOT / "webview-command-line"
FLAGS_CONTENT_SHELL: pth.AnyPosixPath = (
    _FLAG_ROOT / "content-shell-command-line")
FLAGS_CHROME: pth.AnyPosixPath = _FLAG_ROOT / "chrome-command-line"


class ChromiumWebDriverAndroid(ChromiumWebDriver):

  def __init__(self,
               label: str,
               path: Optional[pth.AnyPath] = None,
               settings: Optional[Settings] = None):
    assert settings, "Android browser needs custom settings and platform"
    self._chrome_command_line_path: pth.AnyPath = FLAGS_CHROME
    self._previous_command_line_contents: Optional[str] = None
    super().__init__(label, path, settings)
    self._android_package: str = self._lookup_android_package(self.path)
    if not self._android_package:
      raise RuntimeError("Could not find matching adb package for "
                         f"{self.path} on {self.platform}")

  def _lookup_android_package(self, path: pth.AnyPath) -> str:
    return self.platform.app_path_to_package(path)

  @property
  def android_package(self) -> str:
    return self._android_package

  @property
  def platform(self) -> AndroidAdbPlatform:
    assert isinstance(
        self._platform,
        AndroidAdbPlatform), (f"Invalid platform: {self._platform}")
    return cast(AndroidAdbPlatform, self._platform)

  def _resolve_binary(self, path: pth.AnyPath) -> pth.AnyPath:
    return path

  # TODO: implement setting a clean profile on android
  _UNSUPPORTED_FLAGS: Tuple[str, ...] = (
      "--user-data-dir",
      "--disable-sync",
      "--window-size",
      "--window-position",
  )

  def _filter_flags_for_run(self, flags: FlagsT) -> FlagsT:
    assert isinstance(flags, ChromeFlags)
    chrome_flags = cast(ChromeFlags, flags)
    for flag in self._UNSUPPORTED_FLAGS:
      if flag not in chrome_flags:
        continue
      flag_value = chrome_flags.pop(flag, None)
      logging.debug("Chrome Android: Removed unsupported flag: %s=%s", flag,
                    flag_value)
    return chrome_flags

  def _start_driver(self, session: BrowserSessionRunGroup,
                    driver_path: pth.AnyPath) -> webdriver.Remote:
    self.adb_force_stop()
    if session.browser.wipe_system_user_data:
      self.adb_force_clear()
      self.platform.adb.grant_notification_permissions(self.android_package)
    self._backup_chrome_flags()
    atexit.register(self._restore_chrome_flags)
    return self._start_chromedriver(session, driver_path)

  def _backup_chrome_flags(self) -> None:
    assert self._previous_command_line_contents is None
    self._previous_command_line_contents = self._read_device_flags()

  def _read_device_flags(self) -> Optional[str]:
    if not self.platform.exists(self._chrome_command_line_path):
      return None
    return self.platform.cat(self._chrome_command_line_path)

  def adb_force_stop(self) -> None:
    self.platform.adb.force_stop(self.android_package)

  def adb_force_clear(self) -> None:
    self.platform.adb.force_clear(self.android_package)

  def force_quit(self) -> None:
    try:
      try:
        super().force_quit()
      finally:
        self.adb_force_stop()
    finally:
      self._restore_chrome_flags()

  def _restore_chrome_flags(self) -> None:
    atexit.unregister(self._restore_chrome_flags)
    current_flags = self._read_device_flags()
    if current_flags != self._previous_command_line_contents:
      logging.warning("%s: flags file changed during run", self)
      logging.debug("before: %s", self._previous_command_line_contents)
      logging.debug("current: %s", current_flags)
    if self._previous_command_line_contents is None:
      logging.debug("%s: deleting chrome flags file: %s", self,
                    self._chrome_command_line_path)
      self.platform.rm(self._chrome_command_line_path, missing_ok=True)
    else:
      logging.debug("%s: restoring previous flags file contents in %s", self,
                    self._chrome_command_line_path)
      self.platform.set_file_contents(self._chrome_command_line_path,
                                      self._previous_command_line_contents)
    self._previous_command_line_contents = None

  def _create_options(self, session: BrowserSessionRunGroup,
                      args: Sequence[str]) -> ChromiumOptions:
    options: ChromiumOptions = super()._create_options(session, args)
    options.binary_location = ""
    options.add_experimental_option("androidPackage", self.android_package)
    options.add_experimental_option("androidDeviceSerial",
                                    self.platform.adb.serial_id)
    return options

  def setup_binary(self) -> None:
    super().setup_binary()
    self.platform.adb.grant_notification_permissions(self.android_package)


class LocalChromiumWebDriverAndroid(ChromiumWebDriverAndroid):
  """
  Custom version that uses a locally built bundle wrapper.
  https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_build_instructions.md
  """

  @classmethod
  def is_apk_helper(cls, path: Optional[pth.AnyPath]) -> bool:
    if not path or len(path.parts) == 1:
      return False
    return path.name.endswith("_apk")

  def __init__(self,
               label: str,
               path: Optional[pth.AnyPath] = None,
               settings: Optional[Settings] = None):
    if self.is_apk_helper(path):
      raise ValueError(
          "Locally built chrome version needs package, got empty path")
    assert settings, "Android browser needs custom settings and platform"
    self._package_info: immutabledict[str, Any] = self._parse_package_info(
        settings.platform, path)
    super().__init__(label, path, settings)

  def _lookup_android_package(self, path: pth.AnyPath) -> str:
    return self._package_info["Package name"]

  def _extract_version(self) -> str:
    return self._package_info["versionName"]

  def _parse_package_info(self, platform: plt.Platform,
                          path: pth.AnyPath) -> immutabledict[str, Any]:
    output = platform.host_platform.sh_stdout(
        path, "package-info").rstrip().splitlines()
    package_info = {}
    for line in output:
      key, value = line.split(": ")
      package_info[key] = hjson.loads(value)
    return immutabledict(package_info)

  def setup_binary(self) -> None:
    super().setup_binary()
    self.host_platform.sh_stdout(self.path, "install",
                                 f"--device={self.platform.serial_id}")


class ChromiumWebDriverSsh(ChromiumWebDriver):

  @property
  def platform(self) -> LinuxSshPlatform:
    assert isinstance(self._platform,
                      LinuxSshPlatform), (f"Invalid platform: {self._platform}")
    return cast(LinuxSshPlatform, self._platform)

  def _start_driver(self, session: BrowserSessionRunGroup,
                    driver_path: pth.AnyPath) -> RemoteWebDriver:
    del driver_path
    args = self._get_browser_flags_for_session(session)
    options = self._create_options(session, args)
    platform = self.platform
    host = platform.host
    port = platform.port
    driver = RemoteWebDriver(f"http://{host}:{port}", options=options)
    return driver


class ChromiumWebDriverChromeOsSsh(ChromiumWebDriver):

  @property
  def platform(self) -> ChromeOsSshPlatform:
    assert isinstance(
        self._platform,
        ChromeOsSshPlatform), (f"Invalid platform: {self._platform}")
    return cast(ChromeOsSshPlatform, self._platform)

  def _start_driver(self, session: BrowserSessionRunGroup,
                    driver_path: pth.AnyPath) -> RemoteWebDriver:
    del driver_path
    platform = self.platform
    host = platform.host
    port = platform.port
    args = self._get_browser_flags_for_session(session)
    # TODO(spadhi): correctly handle flags:
    #   1. decide which flags to pass to chrome vs chromedriver
    #   2. investigate irrelevant / unsupported flags on ChromeOS
    #   3. filter out and pass the chrome flags to the debugging session below
    #   4. pass the remaining flags to RemoteWebDriver options
    google_login = session.browser.secrets.get(SecretType.GOOGLE)
    if google_login:
      dbg_port = platform.create_debugging_session(
          username=google_login.username, password=google_login.password)
    else:
      dbg_port = platform.create_debugging_session()
    options = self._create_options(session, args)
    options.add_experimental_option("debuggerAddress", f"127.0.0.1:{dbg_port}")
    driver = RemoteWebDriver(f"http://{host}:{port}", options=options)
    return driver

  # On ChromeOS, the system profile is the same as the browser profile.
  def is_logged_in(self, secret: Secret, strict: bool = False) -> bool:
    if secret.type != SecretType.GOOGLE:
      return False
    if secret.username == self.platform.username:
      return True
    if not strict:
      return False
    raise RuntimeError("Login of non-primary Google accounts not supported")


class DriverNotFoundError(ValueError):
  pass


def build_chromedriver_instructions(build_dir: pth.AnyPath) -> str:
  return ("Please build 'chromedriver' manually for local builds:\n"
          f"    autoninja -C {build_dir} chromedriver")


def is_build_dir(path: pth.LocalPath,
                 platform: plt.Platform = plt.PLATFORM) -> bool:
  return platform.is_file(path / "args.gn")


class ChromeDriverFinder:
  driver_path: pth.LocalPath

  def __init__(self, browser: ChromiumWebDriver):
    self.browser = browser
    self.platform: Platform = browser.platform
    self.host_platform: Platform = browser.platform.host_platform
    extension: str = ""
    if self.host_platform.is_win:
      extension = ".exe"
    cache_dir = self.host_platform.local_cache_dir("driver")
    self.driver_path: pth.LocalPath = (
        cache_dir / f"chromedriver-{self.browser.major_version}{extension}")
    self._validate_browser()

  def _validate_browser(self) -> None:
    browser_platform = self.browser.platform
    if browser_platform.is_local:
      return
    # Some remote platforms rely on a local chromedriver
    if (browser_platform.is_android or browser_platform.is_remote_ssh):
      return
    raise RuntimeError("Cannot download chromedriver for remote browser yet")

  def find_local_build(self) -> pth.LocalPath:
    assert self.browser.app_path
    # assume it's a local build
    lookup_dir = pth.LocalPath(self.browser.app_path.parent)
    driver_path = lookup_dir / "chromedriver"
    if self.platform.is_win:
      driver_path = driver_path.with_suffix(".exe")
    if self.platform.is_file(driver_path):
      return driver_path
    error_message: List[str] = [f"Driver '{driver_path}' does not exist."]
    if is_build_dir(lookup_dir, self.platform):
      error_message += [build_chromedriver_instructions(lookup_dir)]
    else:
      error_message += ["Please manually provide a chromedriver binary."]
    raise DriverNotFoundError("\n".join(error_message))

  def download(self) -> pth.LocalPath:
    if not self.platform.is_file(self.driver_path):
      with exception.annotate(
          f"Downloading chromedriver for {self.browser.version}"):
        self._download()
    return self.driver_path

  def _download(self) -> None:
    milestone = self.browser.major_version
    logging.info("CHROMEDRIVER Downloading from %s v%s", self.browser.type_name,
                 milestone)
    url: Optional[str] = None
    listing_url: Optional[str] = None
    if milestone >= self.CFT_MIN_MILESTONE:
      listing_url, url = self._get_cft_url(milestone)
    if not url:
      listing_url, url = self._get_pre_115_stable_url(milestone)
      if not url:
        listing_url, url = self._get_canary_url()

    if not url:
      raise DriverNotFoundError(
          "Please manually compile/download chromedriver for "
          f"{self.browser.type_name} {self.browser.version}")

    logging.info("CHROMEDRIVER Downloading M%s: %s", milestone, listing_url or
                 url)
    with tempfile.TemporaryDirectory() as tmp_dir:
      if ".zip" not in url:
        maybe_driver = pth.LocalPath(tmp_dir) / "chromedriver"
        self.host_platform.download_to(url, maybe_driver)
      else:
        zip_file = pth.LocalPath(tmp_dir) / "download.zip"
        self.host_platform.download_to(url, zip_file)
        with zipfile.ZipFile(zip_file, "r") as zip_ref:
          zip_ref.extractall(zip_file.parent)
        zip_file.unlink()
        maybe_driver = None
        candidates: List[pth.LocalPath] = [
            path for path in zip_file.parent.glob("**/*")
            if path.is_file() and "chromedriver" in path.name
        ]
        # Find exact match first:
        maybe_drivers: List[pth.LocalPath] = [
            path for path in candidates if path.stem == "chromedriver"
        ]
        # Backup less strict matching:
        maybe_drivers += candidates
        if len(maybe_drivers) > 0:
          maybe_driver = maybe_drivers[0]
      if not maybe_driver or not maybe_driver.is_file():
        raise DriverNotFoundError(
            f"Extracted driver at {maybe_driver} does not exist.")
      self.driver_path.parent.mkdir(parents=True, exist_ok=True)
      shutil.move(os.fspath(maybe_driver), os.fspath(self.driver_path))
      self.driver_path.chmod(self.driver_path.stat().st_mode | stat.S_IEXEC)

  # Using CFT as abbreviation for Chrome For Testing here.
  CFT_MIN_MILESTONE = 115
  CFT_BASE_URL: str = "https://googlechromelabs.github.io/chrome-for-testing"
  CFT_VERSION_URL: str = f"{CFT_BASE_URL}/{{version}}.json"
  CFT_LATEST_URL: str = f"{CFT_BASE_URL}/LATEST_RELEASE_{{major}}"

  CFT_PLATFORM: Final[Dict[Tuple[str, str], str]] = {
      ("linux", "x64"): "linux64",
      ("macos", "x64"): "mac-x64",
      ("macos", "arm64"): "mac-arm64",
      ("win", "ia32"): "win32",
      ("win", "x64"): "win64"
  }

  def _get_cft_url(self, milestone: int) -> Tuple[str, Optional[str]]:
    logging.debug("ChromeDriverFinder: Looking up chrome-for-testing version.")
    platform_name: Optional[str] = self.CFT_PLATFORM.get(self.host_platform.key)
    if not platform_name:
      raise DriverNotFoundError(
          f"Unsupported platform {self.host_platform.key} for chromedriver.")
    listing_url, version_data = self._get_cft_version_data(milestone)
    download_url: Optional[str] = None
    if version_data:
      download_url = self._get_cft_driver_download_url(version_data,
                                                       platform_name)
    return (listing_url, download_url)

  def _get_cft_version_data(self, milestone: int) -> Tuple[str, Optional[Dict]]:
    logging.debug("ChromeDriverFinder: Trying direct download url")
    listing_url, data = self._get_cft_precise_version_data(self.browser.version)
    if data:
      return listing_url, data
    logging.debug(
        "ChromeDriverFinder: Invalid precise version url %s, "
        "using M%s", listing_url, milestone)
    return self._get_ctf_milestone_data(milestone)

  def _get_cft_precise_version_data(self,
                                    version: str) -> Tuple[str, Optional[Dict]]:
    version_url = self.CFT_VERSION_URL.format(version=version)
    try:
      with helper.urlopen(version_url) as response:
        version_data = json.loads(response.read().decode("utf-8"))
        return (version_url, version_data)
    except urllib.error.HTTPError as e:
      logging.debug("ChromeDriverFinder: "
                    "Precise version download failed %s", e)
      return (version_url, None)

  def _get_ctf_milestone_data(self,
                              milestone: int) -> Tuple[str, Optional[Dict]]:
    latest_version_url = self.CFT_LATEST_URL.format(major=milestone)
    try:
      with helper.urlopen(latest_version_url) as response:
        alternative_version = response.read().decode("utf-8").strip()
        logging.debug(
            "ChromeDriverFinder: Using alternative version %s "
            "for M%s", alternative_version, milestone)
        return self._get_cft_precise_version_data(alternative_version)
    except urllib.error.HTTPError:
      return (self.CFT_BASE_URL, None)

  def _get_cft_driver_download_url(self, version_data,
                                   platform_name) -> Optional[str]:
    if all_downloads := version_data.get("downloads"):
      driver_downloads: Dict = all_downloads.get("chromedriver", [])
      for download in driver_downloads:
        if isinstance(download, dict) and download["platform"] == platform_name:
          return download["url"]
    return None

  PRE_115_STABLE_URL: str = "http://chromedriver.storage.googleapis.com"

  def _get_pre_115_stable_url(self,
                              milestone: int) -> Tuple[str, Optional[str]]:
    logging.debug(
        "ChromeDriverFinder: "
        "Looking upe old-style stable version M%s", milestone)
    assert milestone < self.CFT_MIN_MILESTONE
    listing_url = f"{self.PRE_115_STABLE_URL}/index.html"
    driver_version: Optional[str] = self._get_pre_115_driver_version(milestone)
    if not driver_version:
      return listing_url, None
    if self.host_platform.is_linux:
      arch_suffix = "linux64"
    elif self.host_platform.is_macos:
      arch_suffix = "mac64"
      if self.host_platform.is_arm64:
        # The uploaded chromedriver archives changed the naming scheme after
        # chrome version 106.0.5249.21 for Arm64 (previously m1):
        #   before: chromedriver_mac64_m1.zip
        #   after:  chromedriver_mac_arm64.zip
        last_old_naming_version = (106, 0, 5249, 21)
        version_tuple = tuple(map(int, driver_version.split(".")))
        if version_tuple <= last_old_naming_version:
          arch_suffix = "mac64_m1"
        else:
          arch_suffix = "mac_arm64"
    elif self.host_platform.is_win:
      arch_suffix = "win32"
    else:
      raise DriverNotFoundError("Unsupported chromedriver platform")
    url = (f"{self.PRE_115_STABLE_URL}/{driver_version}/"
           f"chromedriver_{arch_suffix}.zip")
    return listing_url, url

  def _get_pre_115_driver_version(self, milestone) -> Optional[str]:
    if milestone < 70:
      return self._get_pre_70_driver_version(milestone)
    url = f"{self.PRE_115_STABLE_URL}/LATEST_RELEASE_{milestone}"
    try:
      with helper.urlopen(url) as response:
        return response.read().decode("utf-8")
    except urllib.error.HTTPError as e:
      if e.code != 404:
        raise DriverNotFoundError(f"Could not query {url}") from e
      logging.debug("ChromeDriverFinder: Could not load latest release url %s",
                    e)
    return None

  def _get_pre_70_driver_version(self, milestone) -> Optional[str]:
    with helper.urlopen(
        f"{self.PRE_115_STABLE_URL}/2.46/notes.txt") as response:
      lines = response.read().decode("utf-8").splitlines()
    for i, line in enumerate(lines):
      if not line.startswith("---"):
        continue
      [min_version, max_version] = map(int, re.findall(r"\d+", lines[i + 1]))
      if min_version <= milestone <= max_version:
        match = re.search(r"\d\.\d+", line)
        if not match:
          raise DriverNotFoundError(f"Could not parse version number: {line}")
        return match.group(0)
    return None

  CHROMIUM_DASH_URL: str = "https://chromiumdash.appspot.com/fetch_releases"
  CHROMIUM_LISTING_URL: str = (
      "https://www.googleapis.com/storage/v1/b/chromium-browser-snapshots/o/")
  CHROMIUM_DASH_PARAMS: Dict[Tuple[str, str], Dict] = {
      ("linux", "x64"): {
          "dash_platform": "linux",
          "dash_channel": "dev",
          "dash_limit": 10,
      },
      ("macos", "x64"): {
          "dash_platform": "mac",
      },
      ("macos", "arm64"): {
          "dash_platform": "mac",
      },
      ("win", "ia32"): {
          "dash_platform": "win",
      },
      ("win", "x64"): {
          "dash_platform": "win64",
      },
  }
  CHROMIUM_LISTING_PREFIX: Dict[Tuple[str, str], str] = {
      ("linux", "x64"): "Linux_x64",
      ("macos", "x64"): "Mac",
      ("macos", "arm64"): "Mac_Arm",
      ("win", "ia32"): "Win",
      ("win", "x64"): "Win_x64",
  }

  def _get_canary_url(self) -> Tuple[str, Optional[str]]:
    logging.debug(
        "ChromeDriverFinder: Try downloading the chromedriver canary version")
    properties = self.CHROMIUM_DASH_PARAMS.get(self.host_platform.key)
    if not properties:
      raise DriverNotFoundError(
          f"Unsupported platform={self.platform}, key={self.host_platform.key}")
    dash_platform = properties["dash_platform"]
    dash_channel = properties.get("dash_channel", "canary")
    # Limit should be > len(canary_versions) so we also get potentially
    # the latest dev version (only beta / stable have official driver binaries).
    dash_limit = properties.get("dash_limit", 100)
    url = helper.update_url_query(
        self.CHROMIUM_DASH_URL, {
            "platform": dash_platform,
            "channel": dash_channel,
            "milestone": str(self.browser.major_version),
            "num": str(dash_limit),
        })
    chromium_base_position = 0
    with helper.urlopen(url) as response:
      version_infos = list(json.loads(response.read().decode("utf-8")))
      if not version_infos:
        raise DriverNotFoundError("Could not find latest version info for "
                                  f"platform={self.host_platform}")
      for version_info in version_infos:
        if version_info["version"] == self.browser.version:
          chromium_base_position = int(
              version_info["chromium_main_branch_position"])
          break

    if not chromium_base_position and version_infos:
      fallback_version_info = None
      # Try matching latest milestone
      for version_info in version_infos:
        if version_info["milestone"] == self.browser.major_version:
          fallback_version_info = version_info
          break

      if not fallback_version_info:
        # Android has a slightly different release cycle than the desktop
        # versions. Assume that the latest canary version is good enough
        fallback_version_info = version_infos[0]
      chromium_base_position = int(
          fallback_version_info["chromium_main_branch_position"])
      logging.warning(
          "Falling back to latest (not precisely matching) "
          "canary chromedriver %s (expected %s)",
          fallback_version_info["version"], self.browser.version)

    if not chromium_base_position:
      raise DriverNotFoundError("Could not find matching canary chromedriver "
                                f"for {self.browser.version}")
    # Use prefixes to limit listing results and increase chances of finding
    # a matching version
    listing_prefix = self.CHROMIUM_LISTING_PREFIX.get(self.host_platform.key)
    if not listing_prefix:
      raise NotImplementedError(
          f"Unsupported chromedriver platform {self.host_platform}")
    base_prefix = str(chromium_base_position)[:4]
    listing_url = helper.update_url_query(self.CHROMIUM_LISTING_URL, {
        "prefix": f"{listing_prefix}/{base_prefix}",
        "maxResults": "10000"
    })
    with helper.urlopen(listing_url) as response:
      listing = json.loads(response.read().decode("utf-8"))

    versions = []
    logging.debug("Filtering %s candidate URLs.", len(listing["items"]))
    for version in listing["items"]:
      if "name" not in version:
        continue
      if "mediaLink" not in version:
        continue
      name = version["name"]
      if "chromedriver" not in name:
        continue
      parts = name.split("/")
      if "chromedriver" not in parts[-1] or len(parts) < 3:
        continue
      base = parts[1]
      try:
        int(base)
      except ValueError:
        # Ignore base if it is not an int
        continue
      versions.append((int(base), version["mediaLink"]))
    versions.sort()
    logging.debug("Found candidates: %s", versions)
    logging.debug("chromium_base_position=%s", chromium_base_position)

    for i in range(len(versions)):
      base, url = versions[i]
      if base > chromium_base_position:
        base, url = versions[i - 1]
        return listing_url, url
    return listing_url, None
