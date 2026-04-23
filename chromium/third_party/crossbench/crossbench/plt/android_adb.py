# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import dataclasses
import datetime as dt
import functools
import logging
import math
import re
import shlex
from typing import TYPE_CHECKING, Any, Final, Generator, Mapping, Optional

from mobly.controllers import android_device
from snippet_uiautomator import uiautomator
from typing_extensions import override

from crossbench import path as pth
from crossbench.flags.base import Flags, FlagsData
from crossbench.helper.path_finder import BundletoolFinder
from crossbench.parse import NumberParser
from crossbench.plt.arch import MachineArch
from crossbench.plt.base import SubprocessError
from crossbench.plt.bin import Binaries
from crossbench.plt.device_info import DeviceInfo
from crossbench.plt.port_manager import PortManager
from crossbench.plt.posix import PosixVersion, RemotePosixPlatform
from crossbench.plt.process_meminfo import ProcessMeminfo
from protoc import activitymanagerservice_pb2, battery_pb2, enums_pb2, \
    windowmanagerservice_pb2

if TYPE_CHECKING:
  import subprocess

  from crossbench.plt.base import Platform
  from crossbench.plt.display_info import DisplayInfo
  from crossbench.plt.types import CmdArg, ListCmdArgs, ProcessIo
  from crossbench.types import JsonDict

# Defines the Android permissions to be granted.
# TODO(381985595): make this configurable.
ANDROID_PERMISSIONS: Final = ("POST_NOTIFICATIONS", "CAMERA", "RECORD_AUDIO")

# Template for Perfetto config to capture Java heaps.
ANDROID_JAVA_HPROF_PERFETTO_CFG = """buffers {{
  size_kb: {size_kb}
  fill_policy: DISCARD
}}

data_sources {{
  config {{
    name: "android.java_hprof"
    java_hprof_config {{
      process_cmdline: "{process_cmdline}"
      dump_smaps: true
    }}
  }}
}}

data_sources {{
  config {{
    name: "android.packages_list"
  }}
}}

data_source_stop_timeout_ms: {data_source_stop_timeout_ms}
duration_ms: 1000
"""


@dataclasses.dataclass(frozen=True)
class AndroidDeviceInfo(DeviceInfo):
  model: str = ""
  product: str = ""
  transport_id: str = ""

  @property
  def serial_id(self) -> str:
    return self.device_id


def _find_adb_bin(platform: Platform,
                  adb_bin: Optional[pth.AnyPath] = None) -> pth.AnyPath:
  if adb_bin:
    return platform.parse_binary_path(adb_bin)
  if adb_bin := Binaries.ADB.search(platform):
    return adb_bin
  raise ValueError(
      "Could not find adb binary."
      "See https://developer.android.com/tools/adb fore more details.")


def adb_devices(
    platform: Platform,
    adb_bin: Optional[pth.AnyPath] = None) -> dict[str, AndroidDeviceInfo]:
  adb_bin = adb_bin or _find_adb_bin(platform)
  output = platform.sh_stdout(adb_bin, "devices", "-l")
  raw_lines = output.strip().splitlines()[1:]
  result: dict[str, AndroidDeviceInfo] = {}
  for line in raw_lines:
    serial_id, details_str = line.split(" ", maxsplit=1)
    details: dict[str, str] = _parse_adb_device_info(details_str)
    device = AndroidDeviceInfo(
        device_id=serial_id.strip(),
        name=details.get("device", ""),
        model=details.get("model", ""),
        product=details.get("product", ""),
        transport_id=details.get("transport_id", ""))
    result[device.serial_id] = device
  return result


def _parse_adb_device_info(value: str) -> dict[str, str]:
  """
  Convert a line from adb devices -l into a descriptive dictionary.
  `value` is a line of output, typically:
  ABCDEF01234567 device 2-1 product:shiba model:AOSP device:shiba transport_id:3

  Some older versions of adb would not contain the `2-1` part.
  """
  parts = value.strip().split(" ")
  assert parts[0], "device"
  return dict(part.split(":") for part in parts[1:] if ":" in part)


class Adb:

  def __init__(self,
               host_platform: Platform,
               device_identifier: Optional[str] = None,
               adb_bin: Optional[pth.AnyPath] = None,
               bundletool: Optional[pth.AnyPath] = None) -> None:
    self._host_platform: Final[Platform] = host_platform
    self._adb_bin: Final[pth.AnyPath] = _find_adb_bin(host_platform, adb_bin)
    self._bundletool: Final[pth.AnyPath
                            | None] = BundletoolFinder.find_binary(
                                host_platform, bundletool)
    serial_id, device_info = self._start(device_identifier)
    self._serial_id: Final[str] = serial_id
    self._device_info: Final[AndroidDeviceInfo] = device_info

  def _start(
      self,
      device_identifier: Optional[str] = None) -> tuple[str, AndroidDeviceInfo]:
    self.start_server()
    serial_id, device_info = self._find_serial_id(device_identifier)
    logging.debug("ADB Selected device: %s %s", serial_id, device_info)
    assert serial_id
    return serial_id, device_info

  def _find_serial_id(
      self,
      device_identifier: Optional[str] = None) -> tuple[str, AndroidDeviceInfo]:
    devices = self.devices()
    if not devices:
      raise ValueError("adb could not find any attached devices."
                       "Connect your device and use 'adb devices' to list all.")
    if device_identifier is None:
      if len(devices) != 1:
        raise ValueError(
            f"Too many adb devices attached, please specify one of: {devices}")
      device_identifier = list(devices.keys())[0]
    if not device_identifier:
      raise ValueError(f"Invalid device identifier: {repr(device_identifier)}")
    if device_identifier in devices:
      return device_identifier, devices[device_identifier]
    matches: list[str] = []
    under_name = device_identifier.replace(" ", "_")
    for key, device in devices.items():
      if device_identifier in device.model or under_name in device.model:
        matches.append(key)
    if not matches:
      raise ValueError(
          f"Could not find adb device matching: '{device_identifier}'")
    if len(matches) > 1:
      raise ValueError(
          f"Found {len(matches)} adb devices matching: '{device_identifier}'.\n"
          f"Choices: {matches}")
    return matches[0], devices[matches[0]]

  def __str__(self) -> str:
    info = f"info='{self._device_info}'"
    if model := self._device_info.model:
      info = f"model={repr(model)}"
    return f"adb(device_id={repr(self._serial_id)}, {info})"

  def has_root(self) -> bool:
    return self.shell_stdout("id").startswith("uid=0(root)")

  def path(self, path: pth.AnyPathLike) -> pth.AnyPath:
    return pth.AnyPosixPath(path)

  @property
  def serial_id(self) -> str:
    return self._serial_id

  @functools.cached_property
  def build_version(self) -> int:
    return int(self.getprop("ro.build.version.release"))

  @functools.cached_property
  def build_description(self) -> str:
    return self.getprop("ro.build.description")

  @property
  def device_info(self) -> AndroidDeviceInfo:
    return self._device_info

  def _build_adb_cmd(self,
                     *args: CmdArg,
                     use_serial_id: bool = True) -> ListCmdArgs:
    adb_cmd: ListCmdArgs = [self._adb_bin]
    if use_serial_id:
      adb_cmd.extend(("-s", self._serial_id))
    adb_cmd.extend(args)
    return adb_cmd

  def _adb(self,
           *args: CmdArg,
           shell: bool = False,
           capture_output: bool = False,
           stdout: ProcessIo = None,
           stderr: ProcessIo = None,
           stdin: ProcessIo = None,
           quiet: bool = False,
           check: bool = True,
           use_serial_id: bool = True) -> subprocess.CompletedProcess:
    del shell
    adb_cmd = self._build_adb_cmd(*args, use_serial_id=use_serial_id)
    return self._host_platform.sh(
        *adb_cmd,
        capture_output=capture_output,
        stdout=stdout,
        stderr=stderr,
        stdin=stdin,
        quiet=quiet,
        check=check)

  def _adb_stdout(self,
                  *args: CmdArg,
                  quiet: bool = False,
                  stdin: ProcessIo = None,
                  encoding: str = "utf-8",
                  use_serial_id: bool = True,
                  check: bool = True) -> str:
    result = self._adb_stdout_bytes(
        *args,
        quiet=quiet,
        stdin=stdin,
        use_serial_id=use_serial_id,
        check=check)
    return result.decode(encoding)

  def _adb_stdout_bytes(self,
                        *args: CmdArg,
                        quiet: bool = False,
                        stdin: ProcessIo = None,
                        use_serial_id: bool = True,
                        check: bool = True) -> bytes:
    adb_cmd = self._build_adb_cmd(*args, use_serial_id=use_serial_id)
    return self._host_platform.sh_stdout_bytes(
        *adb_cmd, quiet=quiet, check=check, stdin=stdin)

  def _get_current_user(self) -> str | None:
    try:
      return self.shell_stdout("am", "get-current-user").strip()
    except SubprocessError as e:
      logging.info(
          "get-current-user failed, return code %d, stderr %s, stdout %s",
          e.returncode, e.stderr, e.stdout)
      return None

  def build_shell_cmd(
      self,
      *args: CmdArg,
      shell: bool = False,
      env: Optional[Mapping[str, str]] = None,
      cwd: Optional[pth.AnyPath] = None,
  ) -> ListCmdArgs:
    if env:
      # TODO: support env with "export FOO=bar;" prefixes
      raise ValueError("ADB shell only supports an empty env for now.")
    if cwd:
      # TODO: support env with "cd foo/bar &&" prefix
      raise ValueError("ADB shell does not support custom cwd")
    self._host_platform.validate_shell_args(args, shell)
    shell_cmd: ListCmdArgs = ["shell"]
    if not shell:
      shell_cmd.append(shlex.join(map(str, args)))
    elif len(args) == 1:
      shell_cmd.append(args[0])
    else:
      raise ValueError("Expected single sh arg with shell=True, "
                       f"but got: {args}")
    adb_shell_cmd = self._build_adb_cmd(*shell_cmd)
    return adb_shell_cmd

  def shell_stdout(self,
                   *args: CmdArg,
                   shell: bool = False,
                   quiet: bool = False,
                   encoding: str = "utf-8",
                   stdin: ProcessIo = None,
                   env: Optional[Mapping[str, str]] = None,
                   cwd: Optional[pth.AnyPath] = None,
                   check: bool = True) -> str:
    result = self.shell_stdout_bytes(
        *args,
        shell=shell,
        quiet=quiet,
        stdin=stdin,
        env=env,
        cwd=cwd,
        check=check)
    return result.decode(encoding)

  def shell_stdout_bytes(self,
                         *args: CmdArg,
                         shell: bool = False,
                         quiet: bool = False,
                         stdin: ProcessIo = None,
                         env: Optional[Mapping[str, str]] = None,
                         cwd: Optional[pth.AnyPath] = None,
                         check: bool = True) -> bytes:
    # -e: choose escape character, or "none"; default '~'
    # -n: don't read from stdin
    # -T: disable pty allocation
    # -t: allocate a pty if on a tty (-tt: force pty allocation)
    # -x: disable remote exit codes and stdout/stderr separation
    shell_cmd = self.build_shell_cmd(*args, shell=shell, env=env, cwd=cwd)
    return self._host_platform.sh_stdout_bytes(
        *shell_cmd, stdin=stdin, quiet=quiet, check=check)

  def shell(self,
            *args: CmdArg,
            shell: bool = False,
            capture_output: bool = False,
            stdout: ProcessIo = None,
            stderr: ProcessIo = None,
            stdin: ProcessIo = None,
            env: Optional[Mapping[str, str]] = None,
            cwd: Optional[pth.AnyPath] = None,
            quiet: bool = False,
            check: bool = True) -> subprocess.CompletedProcess:
    # See shell_stdout for more `adb shell` options.
    shell_cmd = self.build_shell_cmd(*args, shell=shell, env=env, cwd=cwd)
    return self._host_platform.sh(
        *shell_cmd,
        capture_output=capture_output,
        stdout=stdout,
        stderr=stderr,
        stdin=stdin,
        quiet=quiet,
        check=check)

  def start_server(self) -> None:
    self._adb_stdout("start-server", use_serial_id=False)

  def stop_server(self) -> None:
    self.kill_server()

  def kill_server(self) -> None:
    self._adb_stdout("kill-server", use_serial_id=False)

  def root(self) -> None:
    self._adb("root", use_serial_id=False)

  def unroot(self) -> None:
    self._adb("unroot", use_serial_id=False)

  def devices(self) -> dict[str, AndroidDeviceInfo]:
    return adb_devices(self._host_platform, self._adb_bin)

  def forward(self,
              local: int,
              remote: int | str,
              local_protocol: str = "tcp",
              remote_protocol: str = "tcp",
              flags_data: FlagsData = None) -> int:
    cmd_args: list[Any] = ["forward"]
    if flags_data:
      parsed_flags = Flags(flags_data)
      cmd_args.extend(list(parsed_flags))
    cmd_args.append(f"{local_protocol}:{local}")
    cmd_args.append(f"{remote_protocol}:{remote}")
    stdout = self._adb_stdout(*cmd_args).strip()
    if not stdout:
      used_ports = self._adb_stdout("forward", "--list")
      raise ValueError(
          f"Could not setup port-forwarding, ports in use:\n{used_ports}")
    local_port = NumberParser.port_number(stdout, "local_port")
    return local_port

  def forward_remove(self, local: int, protocol: str = "tcp") -> None:
    self._adb("forward", "--remove", f"{protocol}:{local}")

  def reverse(self, remote: int, local: int, protocol: str = "tcp") -> int:
    stdout = self._adb_stdout("reverse", f"{protocol}:{remote}",
                              f"{protocol}:{local}").strip()
    if not stdout:
      used_ports = self._adb_stdout("reverse", "--list")
      raise ValueError("Could not setup reverse port-forwarding, "
                       f"ports in use:\n{used_ports}")
    remote_port = NumberParser.port_number(stdout, "remote_port")
    return remote_port

  def reverse_remove(self, remote: int, protocol: str = "tcp") -> None:
    self._adb("reverse", "--remove", f"{protocol}:{remote}")

  def pull(self, device_src_path: pth.AnyPath,
           local_dest_path: pth.LocalPath) -> None:
    self._adb("pull", self.path(device_src_path), local_dest_path)

  def push(self, local_src_path: pth.LocalPath,
           device_dest_path: pth.AnyPath) -> None:
    self._adb("push", local_src_path, self.path(device_dest_path))

  def cmd(self,
          *args: str,
          quiet: bool = False,
          encoding: str = "utf-8") -> str:
    cmd: ListCmdArgs = ["cmd", *args]
    return self.shell_stdout(*cmd, quiet=quiet, encoding=encoding)

  def dumpsys(self,
              *args: str,
              quiet: bool = False,
              encoding: str = "utf-8") -> str:
    cmd: ListCmdArgs = ["dumpsys", *args]
    return self.shell_stdout(*cmd, quiet=quiet, encoding=encoding)

  def dumpsys_bytes(self, *args: str, quiet: bool = False) -> bytes:
    cmd: ListCmdArgs = ["dumpsys", *args]
    return self.shell_stdout_bytes(*cmd, quiet=quiet)

  def getprop(self,
              *args: str,
              quiet: bool = False,
              encoding: str = "utf-8") -> str:
    cmd: ListCmdArgs = ["getprop", *args]
    return self.shell_stdout(*cmd, quiet=quiet, encoding=encoding).strip()

  def services(self, quiet: bool = False, encoding: str = "utf-8") -> list[str]:
    lines = list(
        self.cmd("-l", quiet=quiet, encoding=encoding).strip().splitlines())
    lines = lines[1:]
    lines.sort()
    return [line.strip() for line in lines]

  def packages(self, quiet: bool = False, encoding: str = "utf-8") -> list[str]:
    # adb shell cmd package list packages
    raw_list = self.cmd(
        "package", "list", "packages", quiet=quiet,
        encoding=encoding).strip().splitlines()
    packages = [package.split(":", maxsplit=2)[1] for package in raw_list]
    packages.sort()
    return packages

  def force_stop(self, package_name: str) -> None:
    if not package_name:
      raise ValueError("Got empty package name")
    self.shell("am", "force-stop", package_name)

  def force_clear(self, package_name: str) -> None:
    if not package_name:
      raise ValueError("Got empty package name")
    cmd: ListCmdArgs = ["pm", "clear"]
    if user := self._get_current_user():
      cmd.extend(["--user", user])
    cmd.extend([package_name])
    self.shell(*cmd)

  def install(self,
              bundle: pth.AnyPath,
              allow_downgrade: bool = False,
              modules: Optional[str] = None) -> None:
    if bundle.suffix == ".apks":
      self.install_apks(bundle, allow_downgrade, modules)
    if bundle.suffix == ".apk":
      self.install_apk(bundle, allow_downgrade)

  def install_apk(self,
                  apk: pth.AnyPath,
                  allow_downgrade: bool = False) -> None:
    if not self._host_platform.exists(apk):
      raise ValueError(f"APK {apk} does not exist {self._host_platform}.")
    args = ["install"]
    if allow_downgrade:
      args.append("-d")
    args.append(str(apk))
    self._adb(*args)

  def install_apks(self,
                   apks: pth.AnyPath,
                   allow_downgrade: bool = False,
                   modules: Optional[str] = None) -> None:
    if not self._host_platform.exists(apks):
      raise ValueError(f"APK {apks} does not exist on {self._host_platform}.")
    if self._bundletool is None:
      raise RuntimeError(
          "Could not find bundletool binary required for install_apks")
    if self._bundletool.suffix == ".jar":
      binary = ["java", "-jar", str(self._bundletool)]
    else:
      binary = [str(self._bundletool)]
    cmd = binary + [
        "install-apks",
        f"--apks={apks}",
        f"--adb={self._adb_bin}",
        f"--device-id={self._serial_id}",
    ]
    if allow_downgrade:
      cmd.append("--allow-downgrade")
    if modules:
      cmd.append(f"--modules={modules}")
    self._host_platform.sh(*cmd)

  def uninstall(self, package_name: str, missing_ok: bool = False) -> None:
    if not package_name:
      raise ValueError("Got empty package name")
    try:
      self._adb("uninstall", package_name)
    except Exception as e:  # noqa: BLE001
      if missing_ok:
        logging.debug("Could not uninstall %s: %s", package_name, e)
      else:
        raise

  def grant_permissions(self, package_name: str) -> None:
    if self.build_version < 13:
      # Notification permission setting is needed for Android 13 and above.
      # https://developer.android.com/develop/ui/views/notifications/notification-permission
      return
    if not package_name:
      raise ValueError("Got empty package name")
    user: str | None = self._get_current_user()
    for perm in ANDROID_PERMISSIONS:
      cmd: ListCmdArgs = ["pm", "grant"]
      if user:
        cmd.extend(["--user", user])
      cmd.extend([package_name, f"android.permission.{perm}"])
      self.shell(*cmd)


class AndroidAdbPortManager(PortManager):

  def __init__(self, platform: AndroidAdbPlatform, adb: Adb) -> None:
    super().__init__(platform)
    self._adb: Adb = adb

  @property
  def host_platform(self) -> Platform:
    return self._platform.host_platform

  @override
  def forward(self, local_port: int, remote_port: int) -> int:
    local_port = NumberParser.positive_zero_int(local_port, "local_port")
    remote_port = NumberParser.port_number(remote_port, "remote_port")
    local_port = self._adb.forward(
        local_port, remote_port, local_protocol="tcp", remote_protocol="tcp")
    logging.debug("Forwarded Remote Port: %s:%s <= %s:%s",
                  self.host_platform.name, local_port, self, remote_port)
    return local_port

  @override
  def forward_devtools(self, local_port: int, remote_identifier: str) -> int:
    local_port = NumberParser.positive_zero_int(local_port, "local_port")
    local_port = self._adb.forward(
        local=local_port,
        remote=remote_identifier,
        local_protocol="tcp",
        remote_protocol="localabstract")
    logging.debug("Forwarded DevTools Port: %s:%s <= %s:%s",
                  self.host_platform.name, local_port, self, remote_identifier)
    return local_port

  @override
  def stop_forward(self, local_port: int) -> None:
    self._adb.forward_remove(local_port, protocol="tcp")

  @override
  def reverse_forward(self, remote_port: int, local_port: int) -> int:
    remote_port = NumberParser.positive_zero_int(remote_port, "remote_port")
    local_port = NumberParser.port_number(local_port, "local_port")
    remote_port = self._adb.reverse(remote_port, local_port, protocol="tcp")
    logging.debug("Forwarded Local Port: %s:%s => %s:%s", self.host_platform,
                  local_port, self, remote_port)
    return remote_port

  @override
  def stop_reverse_forward(self, remote_port: int) -> None:
    self._adb.reverse_remove(remote_port, protocol="tcp")


class AndroidVersion(PosixVersion):
  pass


class AndroidAdbPlatform(RemotePosixPlatform):

  def __init__(self,
               host_platform: Platform,
               device_identifier: Optional[str] = None,
               adb: Optional[Adb] = None) -> None:
    assert not host_platform.is_remote, (
        "adb on remote platform is not supported yet")
    self._adb: Final[Adb] = adb or Adb(host_platform, device_identifier)
    super().__init__(host_platform)

  def _create_port_manager(self) -> PortManager:
    return AndroidAdbPortManager(self, self._adb)

  @override
  def _create_default_tmp_dir(self) -> pth.AnyPath:
    return self.path("/data/local/tmp/")

  @property
  @override
  def is_android(self) -> bool:
    return True

  @property
  @override
  def name(self) -> str:
    return "android"

  @functools.cached_property
  @override
  def version_str(self) -> str:
    return str(self.adb.build_version)

  @functools.cached_property
  @override
  def version(self) -> AndroidVersion:
    return AndroidVersion.parse(self.adb.build_description)

  @functools.cached_property
  @override
  def model(self) -> str:
    return self.adb.getprop("ro.product.model")

  @property
  def serial_id(self) -> str:
    return self._adb.serial_id

  @functools.cached_property
  def uiautomator_device(self) -> android_device.AndroidDevice:
    ad = android_device.AndroidDevice(self.serial_id)
    ad.services.register(uiautomator.ANDROID_SERVICE_NAME,
                         uiautomator.UiAutomatorService)
    return ad

  @functools.cached_property
  @override
  def cpu(self) -> str:
    variant = self.adb.getprop("dalvik.vm.isa.arm.variant")
    platform = self.adb.getprop("ro.board.platform")
    cpu_str = f"{variant} {platform}"

    # Some android devices do not populate props for CPU info.
    # In that case, fallback to attempting to parse /proc/cpuinfo
    if not variant or not platform:
      return super().cpu

    if num_cores := self.cpu_cores(logical=False):
      cpu_str = f"{cpu_str} {num_cores} cores"
    return cpu_str

  def cpu_usage(self) -> float:
    return math.nan

  @property
  def adb(self) -> Adb:
    return self._adb

  _MACHINE_ARCH_LOOKUP: Final = {
      "arm64-v8a": MachineArch.ARM_64,
      "armeabi-v7a": MachineArch.ARM_32,
      "x86": MachineArch.IA32,
      "x86_64": MachineArch.X64,
  }

  @functools.cached_property
  @override
  def machine(self) -> MachineArch:
    cpu_abi = self.adb.getprop("ro.product.cpu.abi")
    arch = self._MACHINE_ARCH_LOOKUP.get(cpu_abi, None)
    if not arch:
      raise ValueError(f"Unknown android CPU ABI: {cpu_abi}")
    return arch

  @override
  def get_relative_cpu_speed(self) -> float:
    # TODO figure out
    return 1.0

  def app_path_to_package(self, app_path: pth.AnyPathLike) -> str:
    path = self.path(app_path)
    parts = path.parts
    if len(parts) > 1:
      raise ValueError(f"Invalid android package name: '{path}'")
    package: str = parts[0]
    packages = self.adb.packages()
    if package not in packages:
      raise ValueError(f"Package '{package}' is not installed on {self._adb}")
    return package

  @override
  def search_binary(self, app_or_bin: pth.AnyPathLike) -> Optional[pth.AnyPath]:
    app_or_bin_path = self.path(app_or_bin)
    if not app_or_bin_path.parts:
      raise ValueError("Got empty path")
    if result_path := self.which(app_or_bin_path):
      return result_path
    if str(app_or_bin) in self.adb.packages():
      return app_or_bin_path
    return None

  @override
  def home(self) -> pth.AnyPath:
    raise RuntimeError("Cannot access home dir on (non-rooted) android device")

  _VERSION_NAME_RE: Final[re.Pattern] = re.compile(
      r"versionName=(?P<version>.+)")

  @override
  def app_version(self, app_or_bin: pth.AnyPathLike) -> str:
    # adb shell dumpsys package com.chrome.canary | grep versionName -C2
    package = self.app_path_to_package(app_or_bin)
    package_info = self.adb.dumpsys("package", str(package))
    match_result = self._VERSION_NAME_RE.search(package_info)
    if match_result is None:
      raise ValueError(
          f"Could not find version for '{package}': {package_info}")
    return match_result.group("version")

  @override
  def process_children(self,
                       parent_pid: int,
                       recursive: bool = False) -> list[dict[str, Any]]:
    # TODO: implement
    return []

  @override
  def foreground_process(self) -> Optional[dict[str, Any]]:
    # adb shell dumpsys activity activities
    # TODO: implement
    return None

  @override
  def check_autobrightness(self) -> bool:
    # adb shell dumpsys display
    # TODO: implement.
    return True

  _BRIGHTNESS_RE: Final[re.Pattern] = re.compile(
      r"mLatestFloatBrightness=(?P<brightness>[0-9]+\.[0-9]+)")

  @override
  def get_main_display_brightness(self) -> int:
    display_info: str = self.adb.dumpsys("display")
    match_result = self._BRIGHTNESS_RE.search(display_info)
    if match_result is None:
      raise ValueError("Could not parse adb display brightness.")
    return int(float(match_result.group("brightness")) * 100)

  @override
  def build_shell_cmd(
      self,
      *args: CmdArg,
      shell: bool = False,
      env: Optional[Mapping[str, str]] = None,
      cwd: Optional[pth.AnyPath] = None,
  ) -> ListCmdArgs:
    return self.adb.build_shell_cmd(*args, shell=shell, env=env, cwd=cwd)

  @override
  def sh(self,
         *args: CmdArg,
         shell: bool = False,
         capture_output: bool = False,
         stdout: ProcessIo = None,
         stderr: ProcessIo = None,
         stdin: ProcessIo = None,
         env: Optional[Mapping[str, str]] = None,
         cwd: Optional[pth.AnyPath] = None,
         quiet: bool = False,
         check: bool = True) -> subprocess.CompletedProcess:
    return self.adb.shell(
        *args,
        shell=shell,
        capture_output=capture_output,
        stdout=stdout,
        stderr=stderr,
        stdin=stdin,
        env=env,
        cwd=cwd,
        quiet=quiet,
        check=check)

  @override
  def sh_stdout_bytes(self,
                      *args: CmdArg,
                      shell: bool = False,
                      quiet: bool = False,
                      stdin: ProcessIo = None,
                      env: Optional[Mapping[str, str]] = None,
                      cwd: Optional[pth.AnyPath] = None,
                      check: bool = True) -> bytes:
    return self.adb.shell_stdout_bytes(
        *args,
        shell=shell,
        stdin=stdin,
        env=env,
        cwd=cwd,
        quiet=quiet,
        check=check)

  @override
  def pull(self, from_path: pth.AnyPath,
           to_path: pth.LocalPath) -> pth.LocalPath:
    device_path = self.path(from_path)
    if not self.exists(device_path):
      raise ValueError(f"Source file '{from_path}' does not exist on {self}")
    local_host_path = self.host_path(to_path)
    local_host_path.parent.mkdir(parents=True, exist_ok=True)
    self.adb.pull(device_path, local_host_path)
    return to_path

  @override
  def push(self, from_path: pth.LocalPath, to_path: pth.AnyPath) -> pth.AnyPath:
    to_path = self.path(to_path)
    self.adb.push(self.host_path(from_path), to_path)
    return to_path

  def _mktemp_sh(self,
                 is_dir: bool,
                 suffix: Optional[str] = None,
                 prefix: Optional[str] = None,
                 dir: Optional[pth.AnyPathLike] = None) -> pth.AnyPath:
    temp_path = super()._mktemp_sh(is_dir, prefix=prefix, dir=dir)
    if not suffix:
      return temp_path
    # android's mktemp does not support suffix on some platforms.
    temp_path_with_suffix = temp_path.with_name(f"{temp_path.name}{suffix}")
    self.rename(temp_path, temp_path_with_suffix)
    return temp_path_with_suffix

  @override
  def processes(self,
                attrs: Optional[list[str]] = None) -> list[dict[str, Any]]:
    lines = self.sh_stdout("ps", "-A", "-o", "PID,NAME").splitlines()
    if len(lines) == 1:
      return []

    res: list[dict[str, Any]] = []
    for line in lines[1:]:
      tokens = line.strip().split(maxsplit=1)
      assert len(tokens) == 2, f"Got invalid process tokens: {tokens}"
      res.append({"pid": int(tokens[0]), "name": tokens[1]})
    return res

  _DUMPSYS_TIMEOUT_RE: Final[re.Pattern] = re.compile(
      rb"\*\*\* SERVICE '[^']+' DUMP TIMEOUT \(\d+ms\) EXPIRED \*\*\*")

  @override
  def process_meminfo(
      self, process_name: str, timeout: dt.timedelta = dt.timedelta(seconds=10)
  ) -> list[ProcessMeminfo]:
    timeout_ms = int(timeout / dt.timedelta(milliseconds=1))
    meminfo_output: bytes = self.adb.dumpsys_bytes("-T", str(timeout_ms),
                                                   "meminfo", "--proto",
                                                   "--package", process_name)
    if self._DUMPSYS_TIMEOUT_RE.search(meminfo_output):
      raise TimeoutError("dumpsys meminfo timed out")
    proto_dump = activitymanagerservice_pb2.MemInfoDumpProto()
    proto_dump.ParseFromString(meminfo_output)
    meminfos: list[ProcessMeminfo] = []
    for app_process in proto_dump.app_processes:
      mem_info = app_process.process_memory.total_heap.mem_info
      meminfos.append(
          ProcessMeminfo(
              pid=app_process.process_memory.pid,
              name=app_process.process_memory.process_name,
              pss_total=mem_info.total_pss_kb,
              rss_total=mem_info.total_rss_kb,
              swap_total=mem_info.dirty_swap_pss_kb or mem_info.dirty_swap_kb))
    return meminfos

  _DUMPSYS_SYSTEM_TOTAL_FREE_RE: Final[re.Pattern] = re.compile(
      br"Total RAM: (?P<total_ram_kb>[0-9][,0-9]*)K.*"
      br"\n Free RAM: [0-9][,0-9]*K \( *"
      br"(?P<cached_pss_kb>[0-9][,0-9]*)K cached pss \+ +"
      br"(?P<cached_kernel_kb>[0-9][,0-9]*)K cached kernel \+ +"
      br"(?P<free_kb>[0-9][,0-9]*)K free\)"
      # Include other footer lines so we don't have to parse the whole output
      # for optional fields.
      br".*$",
      re.DOTALL)

  _DUMPSYS_SYSTEM_DMA_BUF_RE: Final[re.Pattern] = re.compile(
      br"DMA-BUF: +(?P<dma_buf_kb>[0-9][,0-9]*)K \("
      br" +(?P<dma_buf_mapped_kb>[0-9][,0-9]*)K mapped \+"
      br" +(?P<dma_buf_unmapped_kb>[0-9][,0-9]*)K unmapped\)", re.DOTALL)

  def _groupdict_to_system_meminfo(
      self, groupdict: dict[str, bytes]) -> dict[str, float]:
    return {
        key: float(value.replace(b",", b""))
        for [key, value] in groupdict.items()
    }

  def system_meminfo(
      self,
      timeout: dt.timedelta = dt.timedelta(seconds=10)) -> dict[str, float]:
    timeout_ms = int(timeout / dt.timedelta(milliseconds=1))
    # TODO: switch to proto parsing if/when DMA-BUF counters are in proto
    # output.
    meminfo_output: bytes = self.adb.dumpsys_bytes("-T", str(timeout_ms),
                                                   "meminfo")
    if self._DUMPSYS_TIMEOUT_RE.search(meminfo_output):
      raise TimeoutError("dumpsys meminfo timed out")

    meminfo: dict[str, float] = {}
    footer_match = self._DUMPSYS_SYSTEM_TOTAL_FREE_RE.search(meminfo_output)
    if not footer_match:
      raise RuntimeError("No 'Total RAM' line found in dumpsys meminfo output")
    meminfo.update(self._groupdict_to_system_meminfo(footer_match.groupdict()))

    dma_buf_match = self._DUMPSYS_SYSTEM_DMA_BUF_RE.search(footer_match[0])
    if dma_buf_match:
      meminfo.update(
          self._groupdict_to_system_meminfo(dma_buf_match.groupdict()))

    return meminfo

  def dump_java_heap(self, identifier: str, label: str,
                     trace_buffer_size_kb: int,
                     timeout: dt.timedelta) -> pth.AnyPath:

    timeout_ms = timeout / dt.timedelta(milliseconds=1)
    cfg_path = self.path("/data/misc/perfetto-configs/{}.pbtxt".format(label))
    dump_path = self.path(
        "/data/misc/perfetto-traces/{}.trace.pb".format(label))
    cfg = ANDROID_JAVA_HPROF_PERFETTO_CFG.format(
        size_kb=trace_buffer_size_kb,
        process_cmdline=identifier,
        data_source_stop_timeout_ms=timeout_ms)
    try:
      self.write_text(cfg_path, cfg)
      self.sh("perfetto", "--config", cfg_path, "--txt", "--out", dump_path)
    except Exception:
      self.rm(dump_path, missing_ok=True)
      raise

    self.rm(cfg_path, missing_ok=True)
    return dump_path

  @functools.lru_cache(maxsize=1)
  @override
  def cpu_details(self) -> dict[str, Any]:
    # TODO: Implement properly (i.e. remove all n/a values)
    return {
        "info": self.cpu,
        "physical cores": self.cpu_cores(logical=False),
        "logical cores": self.cpu_cores(logical=True),
        "usage": "n/a",
        "total usage": "n/a",
        "system load": "n/a",
        "min frequency": "n/a",
        "max frequency": "n/a",
        "current frequency": "n/a",
    }

  _GETPROP_RE: Final[re.Pattern] = re.compile(
      r"^\[(?P<key>[^\]]+)\]: \[(?P<value>[^\]]+)\]$")

  @functools.lru_cache(maxsize=1)
  @override
  def system_details(self) -> dict[str, Any]:
    system_details = super().system_details()
    system_details.update({
        "Android": self._getprop_system_details(),
    })
    return system_details

  def _getprop_system_details(self) -> dict[str, Any]:
    properties: dict[str, str] = {}
    for line in self.adb.shell_stdout("getprop").strip().splitlines():
      result = self._GETPROP_RE.fullmatch(line)
      if result:
        properties[result.group("key")] = result.group("value")
    return properties

  @functools.lru_cache(maxsize=1)
  @override
  def python_details(self) -> JsonDict:
    # TODO: Implement properly (i.e. remove all n/a values)
    return {
        "version": "n/a",
        "bits": "n/a",
    }

  @property
  @override
  def is_battery_powered(self) -> bool:
    battery_info_bytes = self.adb.dumpsys_bytes("battery", "--proto")
    battery_info = battery_pb2.BatteryServiceDumpProto()
    battery_info.ParseFromString(battery_info_bytes)
    return (battery_info.plugged ==
            enums_pb2.BatteryPluggedStateEnum.BATTERY_PLUGGED_NONE)

  @override
  def screenshot(self, result_path: pth.AnyPath) -> None:
    self.sh("screencap", "-p", result_path)

  _DUMPSYS_WINDOW_DISPLAYS_RE: Final[re.Pattern] = re.compile(
      r" cur=(?P<x>\d+)x(?P<y>\d+) ")

  @functools.lru_cache(maxsize=1)
  def display_details(self) -> tuple[DisplayInfo, ...]:
    return ({"resolution": self.display_resolution(), "refresh_rate": -1},)

  @override
  def display_resolution(self) -> tuple[int, int]:
    displays_bytes = self.adb.dumpsys_bytes("window", "displays", "--proto")

    displays = windowmanagerservice_pb2.WindowManagerServiceDumpProto()
    displays.ParseFromString(displays_bytes)

    width = (
        displays.root_window_container.window_container.configuration_container
        .full_configuration.window_configuration.max_bounds.right)
    height = (
        displays.root_window_container.window_container.configuration_container
        .full_configuration.window_configuration.max_bounds.bottom)

    return (width, height)

  def user_id(self) -> int:
    return NumberParser.any_int(self.sh_stdout("am", "get-current-user"))

  @override
  @contextlib.contextmanager
  def low_power_mode(self) -> Generator[None, Any, None]:
    try:
      self.lock_screen()
      self.doze()
      yield
    finally:
      self.exit_doze()
      self.unlock_screen()

  def doze(self) -> None:
    self.adb.dumpsys("deviceidle", "force-idle")

  def exit_doze(self) -> None:
    self.adb.dumpsys("deviceidle", "unforce")
    self.adb.dumpsys("battery", "reset")

  def lock_screen(self) -> None:
    self.adb.shell("input", "keyevent", "KEYCODE_POWER")

  def unlock_screen(self) -> None:
    # Wake up the device
    self.adb.shell("input", "keyevent", "KEYCODE_WAKEUP")
    # Unlock the device
    self.adb.shell("input", "keyevent", "KEYCODE_MENU")
