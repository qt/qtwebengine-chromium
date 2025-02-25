# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import abc
import collections.abc
import contextlib
import datetime as dt
import functools
import inspect
import logging
import os
import pathlib
import platform as py_platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import (TYPE_CHECKING, Any, Callable, Dict, Final, Generator,
                    Iterable, Iterator, List, Mapping, Optional, Sequence,
                    Tuple, Union)

import psutil

from crossbench import path as pth
from crossbench.plt.arch import MachineArch
from crossbench.plt.bin import Binary

if TYPE_CHECKING:
  from crossbench.path import LocalPath
  from crossbench.types import JsonDict


CmdArg = pth.AnyPathLike
ListCmdArgs = List[CmdArg]
TupleCmdArgs = Tuple[CmdArg, ...]
CmdArgs = Union[ListCmdArgs, TupleCmdArgs]


class Environ(collections.abc.MutableMapping, metaclass=abc.ABCMeta):
  pass


class LocalEnviron(Environ):

  def __init__(self) -> None:
    self._environ = os.environ

  def __getitem__(self, key: str) -> str:
    return self._environ.__getitem__(key)

  def __setitem__(self, key: str, item: str) -> None:
    self._environ.__setitem__(key, item)

  def __delitem__(self, key: str) -> None:
    self._environ.__delitem__(key)

  def __iter__(self) -> Iterator[str]:
    return self._environ.__iter__()

  def __len__(self) -> int:
    return self._environ.__len__()


class SubprocessError(subprocess.CalledProcessError):
  """ Custom version that also prints stderr for debugging"""

  def __init__(self, platform: Platform, process) -> None:
    self.platform = platform
    super().__init__(process.returncode, shlex.join(map(str, process.args)),
                     process.stdout, process.stderr)

  def __str__(self) -> str:
    super_str = super().__str__()
    if not self.stderr:
      return f"{self.platform}: {super_str}"
    return f"{self.platform}: {super_str}\nstderr:{self.stderr.decode()}"


_IGNORED_PROCESS_EXCEPTIONS: Final = (psutil.NoSuchProcess, psutil.AccessDenied,
                                      psutil.ZombieProcess)

DEFAULT_CACHE_DIR = pth.LocalPath(__file__).parents[2] / "cache"

class Platform(abc.ABC):
  # pylint: disable=locally-disabled, redefined-builtin

  def __init__(self) -> None:
    self._binary_lookup_override: Dict[str, pth.AnyPath] = {}
    self._cache_dir: Optional[pth.AnyPath] = None
    if self.is_local:
      self._cache_dir = DEFAULT_CACHE_DIR

  def assert_is_local(self) -> None:
    if self.is_local:
      return
    caller = "assert_is_local"
    caller = inspect.stack()[1].function
    raise RuntimeError(f"{type(self).__name__}.{caller}(...) is not supported "
                       "on remote platform")

  @property
  @abc.abstractmethod
  def name(self) -> str:
    pass

  @property
  @abc.abstractmethod
  def version(self) -> str:
    pass

  @property
  @abc.abstractmethod
  def device(self) -> str:
    pass

  @property
  @abc.abstractmethod
  def cpu(self) -> str:
    pass

  @property
  def full_version(self) -> str:
    return f"{self.name} {self.version} {self.machine}"

  def __str__(self) -> str:
    return ".".join(self.key) + (".remote" if self.is_remote else ".local")

  @property
  def is_remote(self) -> bool:
    return False

  @property
  def is_local(self) -> bool:
    return not self.is_remote

  @property
  def host_platform(self) -> Platform:
    return self

  @functools.cached_property
  def machine(self) -> MachineArch:
    raw = self._raw_machine_arch()
    if raw in ("i386", "i686", "x86", "ia32"):
      return MachineArch.IA32
    if raw in ("x86_64", "AMD64"):
      return MachineArch.X64
    if raw in ("arm64", "aarch64"):
      return MachineArch.ARM_64
    if raw in ("arm"):
      return MachineArch.ARM_32
    raise NotImplementedError(f"Unsupported machine type: {raw}")

  def _raw_machine_arch(self) -> str:
    self.assert_is_local()
    return py_platform.machine()

  @property
  def is_ia32(self) -> bool:
    return self.machine == MachineArch.IA32

  @property
  def is_x64(self) -> bool:
    return self.machine == MachineArch.X64

  @property
  def is_arm64(self) -> bool:
    return self.machine == MachineArch.ARM_64

  @property
  def key(self) -> Tuple[str, str]:
    return (self.name, str(self.machine))

  @property
  def is_macos(self) -> bool:
    return False

  @property
  def is_linux(self) -> bool:
    return False

  @property
  def is_android(self) -> bool:
    return False

  @property
  def is_chromeos(self) -> bool:
    return False

  @property
  def is_posix(self) -> bool:
    return self.is_macos or self.is_linux or self.is_android

  @property
  def is_win(self) -> bool:
    return False

  @property
  def is_remote_ssh(self) -> bool:
    return False

  @property
  def environ(self) -> Environ:
    self.assert_is_local()
    return LocalEnviron()

  @property
  def is_battery_powered(self) -> bool:
    self.assert_is_local()
    if not psutil.sensors_battery:
      return False
    status = psutil.sensors_battery()
    if not status:
      return False
    return not status.power_plugged

  def _search_executable(
      self,
      name: str,
      macos: Sequence[str],
      win: Sequence[str],
      linux: Sequence[str],
      lookup_callable: Callable[[pth.AnyPath], Optional[pth.AnyPath]],
  ) -> pth.AnyPath:
    executables: Sequence[str] = []
    if self.is_macos:
      executables = macos
    elif self.is_win:
      executables = win
    elif self.is_linux:
      executables = linux
    if not executables:
      raise ValueError(f"Executable {name} not supported on {self}")
    for name_or_path in executables:
      path = self.local_path(name_or_path).expanduser()
      binary = lookup_callable(path)
      if binary and self.exists(binary):
        return binary
    raise ValueError(f"Executable {name} not found on {self}")

  def search_app_or_executable(
      self,
      name: str,
      macos: Sequence[str] = (),
      win: Sequence[str] = (),
      linux: Sequence[str] = ()
  ) -> pth.AnyPath:
    return self._search_executable(name, macos, win, linux, self.search_app)

  def search_platform_binary(
      self,
      name: str,
      macos: Sequence[str] = (),
      win: Sequence[str] = (),
      linux: Sequence[str] = ()
  ) -> pth.AnyPath:
    return self._search_executable(name, macos, win, linux, self.search_binary)

  def search_app(self, app_or_bin: pth.AnyPath) -> Optional[pth.AnyPath]:
    """Look up a application bundle (macos) or binary (all other platforms) in
    the common search paths.
    """
    return self.search_binary(app_or_bin)

  @abc.abstractmethod
  def search_binary(self, app_or_bin: pth.AnyPathLike) -> Optional[pth.AnyPath]:
    """Look up a binary in the common search paths based of a path or a single
    segment path with just the binary name.
    Returns the location of the binary (and not the .app bundle on macOS).
    """

  @abc.abstractmethod
  def app_version(self, app_or_bin: pth.AnyPathLike) -> str:
    pass

  @property
  def has_display(self) -> bool:
    """Return a bool whether the platform has an active display.
    This can be false on linux without $DISPLAY, true an all other platforms."""
    return True

  def sleep(self, seconds: Union[int, float, dt.timedelta]) -> None:
    if isinstance(seconds, dt.timedelta):
      seconds = seconds.total_seconds()
    if seconds == 0:
      return
    logging.debug("WAIT %ss", seconds)
    time.sleep(seconds)

  def which(self, binary_name: pth.AnyPathLike) -> Optional[pth.AnyPath]:
    if not binary_name:
      raise ValueError("Got empty path")
    self.assert_is_local()
    if override := self.lookup_binary_override(binary_name):
      return override
    if result := shutil.which(os.fspath(binary_name)):
      return self.path(result)
    return None

  def lookup_binary_override(
      self, binary_name: pth.AnyPathLike) -> Optional[pth.AnyPath]:
    return self._binary_lookup_override.get(os.fspath(binary_name))

  def set_binary_lookup_override(self, binary_name: pth.AnyPathLike,
                                 new_path: Optional[pth.AnyPath]):
    name = os.fspath(binary_name)
    if new_path is None:
      prev_result = self._binary_lookup_override.pop(name, None)
      if prev_result is None:
        logging.debug(
            "Could not remove binary override for %s as it was never set",
            binary_name)
      return
    if self.search_binary(new_path) is None:
      raise ValueError(f"Suggested binary override for {repr(name)} "
                       f"does not exist: {new_path}")
    self._binary_lookup_override[name] = new_path

  @contextlib.contextmanager
  def override_binary(self, binary: Union[pth.AnyPathLike, Binary],
                      result: Optional[pth.AnyPath]):
    binary_name: pth.AnyPathLike = ""
    if isinstance(binary, Binary):
      if override := binary.platform_path(self):
        binary_name = override[0]
      else:
        raise RuntimeError("Cannot override binary:"
                           f" {binary} is not supported supported on {self}")
    else:
      binary_name = binary
    prev_override = self.lookup_binary_override(binary_name)
    self.set_binary_lookup_override(binary_name, result)
    try:
      yield
    finally:
      self.set_binary_lookup_override(binary_name, prev_override)

  def processes(self,
                attrs: Optional[List[str]] = None) -> List[Dict[str, Any]]:
    # TODO(cbruni): support remote platforms
    assert self.is_local, "Only local platform supported"
    return self._collect_process_dict(psutil.process_iter(attrs=attrs))

  def process_running(self, process_name_list: List[str]) -> Optional[str]:
    self.assert_is_local()
    # TODO(cbruni): support remote platforms
    for proc in psutil.process_iter(attrs=["name"]):
      try:
        if proc.name().lower() in process_name_list:
          return proc.name()
      except _IGNORED_PROCESS_EXCEPTIONS:
        pass
    return None

  def process_children(self,
                       parent_pid: int,
                       recursive: bool = False) -> List[Dict[str, Any]]:
    self.assert_is_local()
    # TODO(cbruni): support remote platforms
    try:
      process = psutil.Process(parent_pid)
    except _IGNORED_PROCESS_EXCEPTIONS:
      return []
    return self._collect_process_dict(process.children(recursive=recursive))

  def _collect_process_dict(
      self, process_iterator: Iterable[psutil.Process]) -> List[Dict[str, Any]]:
    process_info_list: List[Dict[str, Any]] = []
    for process in process_iterator:
      try:
        process_info_list.append(process.as_dict())
      except _IGNORED_PROCESS_EXCEPTIONS:
        pass
    return process_info_list

  def process_info(self, pid: int) -> Optional[Dict[str, Any]]:
    self.assert_is_local()
    # TODO(cbruni): support remote platforms
    try:
      return psutil.Process(pid).as_dict()
    except _IGNORED_PROCESS_EXCEPTIONS:
      return None

  def foreground_process(self) -> Optional[Dict[str, Any]]:
    return None

  def terminate(self, proc_pid: int) -> None:
    self.assert_is_local()
    # TODO(cbruni): support remote platforms
    process = psutil.Process(proc_pid)
    for proc in process.children(recursive=True):
      proc.terminate()
    process.terminate()

  @property
  def default_tmp_dir(self) -> pth.AnyPath:
    self.assert_is_local()
    return self.path(tempfile.gettempdir())

  def port_forward(self, local_port: int, remote_port: int) -> int:
    if remote_port != local_port:
      raise ValueError("Cannot forward a remote port on a local platform.")
    self.assert_is_local()
    return local_port

  def stop_port_forward(self, local_port: int) -> None:
    del local_port
    self.assert_is_local()

  def reverse_port_forward(self, remote_port: int, local_port: int) -> int:
    if remote_port != local_port:
      raise ValueError("Cannot forward a remote port on a local platform.")
    self.assert_is_local()
    return remote_port

  def stop_reverse_port_forward(self, remote_port: int) -> None:
    del remote_port
    self.assert_is_local()

  def local_cache_dir(self, name: Optional[str] = None) -> pth.LocalPath:
    return self.local_path(self.cache_dir(name))

  def cache_dir(self, name: Optional[str] = None) -> pth.AnyPath:
    assert self._cache_dir, "missing cache dir"
    if not name:
      dir = self._cache_dir
    else:
      dir = self._cache_dir / pth.safe_filename(name)
    self.mkdir(dir, parents=True, exist_ok=True)
    return dir

  def set_cache_dir(self, path: pth.AnyPath) -> None:
    self._cache_dir = path
    self.mkdir(path, parents=True, exist_ok=True)

  def cat(self, file: pth.AnyPathLike, encoding: str = "utf-8") -> str:
    """Meow! I return the file contents as a str."""
    with self.local_path(file).open(encoding=encoding) as f:
      return f.read()

  def cat_bytes(self, file: pth.AnyPathLike) -> bytes:
    """Hiss! I return the file contents as bytes."""
    with self.local_path(file).open("rb") as f:
      return f.read()

  def get_file_contents(self,
                        file: pth.AnyPathLike,
                        encoding: str = "utf-8") -> str:
    return self.cat(file, encoding)

  def set_file_contents(self,
                        file: pth.AnyPathLike,
                        data: str,
                        encoding: str = "utf-8") -> None:
    with self.local_path(file).open("w", encoding=encoding) as f:
      f.write(data)

  def pull(self, from_path: pth.AnyPath,
           to_path: pth.LocalPath) -> pth.LocalPath:
    """ Download / Copy a (remote) file to the local filesystem.
    By default this is just a copy operation on the local filesystem.
    """
    self.assert_is_local()
    return self.local_path(self.copy_file(from_path, to_path))

  def push(self, from_path: pth.LocalPath, to_path: pth.AnyPath) -> pth.AnyPath:
    """ Copy a local file to this (remote) platform.
    By default this is just a copy operation on the local filesystem.
    """
    self.assert_is_local()
    return self.copy_file(from_path, to_path)

  def copy(self, from_path: pth.AnyPath, to_path: pth.AnyPath) -> pth.AnyPath:
    """ Convenience implementation for copying local files and dirs """
    if not self.exists(from_path):
      raise ValueError(f"Cannot copy non-existing source path: {from_path}")
    if self.is_dir(from_path):
      return self.copy_dir(from_path, to_path)
    return self.copy_file(from_path, to_path)

  def copy_dir(self, from_path: pth.AnyPathLike,
               to_path: pth.AnyPathLike) -> pth.AnyPath:
    from_path = self.local_path(from_path)
    to_path = self.local_path(to_path)
    self.mkdir(to_path.parent, parents=True, exist_ok=True)
    shutil.copytree(os.fspath(from_path), os.fspath(to_path))
    return to_path

  def copy_file(self, from_path: pth.AnyPathLike,
                to_path: pth.AnyPathLike) -> pth.AnyPath:
    from_path = self.local_path(from_path)
    to_path = self.local_path(to_path)
    self.mkdir(to_path.parent, parents=True, exist_ok=True)
    shutil.copy2(os.fspath(from_path), os.fspath(to_path))
    return to_path

  def rm(self,
         path: pth.AnyPathLike,
         dir: bool = False,
         missing_ok: bool = False) -> None:
    """Remove a single file on this platform."""
    path = self.local_path(path)
    if dir:
      if missing_ok and not self.exists(path):
        return
      shutil.rmtree(os.fspath(path))
    else:
      path.unlink(missing_ok)

  def rename(self, src_path: pth.AnyPathLike,
             dst_path: pth.AnyPathLike) -> pth.AnyPath:
    """Remove a single file on this platform."""
    return self.local_path(src_path).rename(dst_path)

  def symlink_or_copy(self, src: pth.AnyPathLike,
                      dst: pth.AnyPathLike) -> pth.AnyPath:
    """Windows does not support symlinking without admin support.
    Copy files on windows (see WinPlatform) but symlink everywhere else."""
    assert not self.is_win, "Unsupported operation 'symlink_or_copy' on windows"
    dst_path = self.local_path(dst)
    dst_path.symlink_to(self.path(src))
    return dst_path

  def path(self, path: pth.AnyPathLike) -> pth.AnyPath:
    """"Used to convert any paths and strings to a platform specific
    remote path.
    For instance a remote ADB platform on windows returns posix paths:
      posix_path = adb_remote_platform.patch(windows_path)
    This is used when passing out platform specific paths to remote shell
    commands.
    """
    return self.local_path(path)

  def local_path(self, path: pth.AnyPathLike) -> pth.LocalPath:
    self.assert_is_local()
    return pth.LocalPath(path)

  def absolute(self, path: pth.AnyPathLike) -> pth.AnyPath:
    """Convert an arbitrary path to a platform-specific absolute path"""
    platform_path: pth.AnyPath = self.path(path)
    if platform_path.is_absolute():
      return platform_path
    if self.is_local:
      return self.local_path(platform_path).absolute()
    raise RuntimeError(
        f"Converting relative to absolute paths is not supported on {self}")

  def home(self) -> pth.AnyPath:
    return pathlib.Path.home()

  def touch(self, path: pth.AnyPathLike) -> None:
    self.local_path(path).touch(exist_ok=True)

  def mkdir(self,
            path: pth.AnyPathLike,
            parents: bool = True,
            exist_ok: bool = True) -> None:
    self.local_path(path).mkdir(parents=parents, exist_ok=exist_ok)

  @contextlib.contextmanager
  def NamedTemporaryFile(  # pylint: disable=invalid-name
      self,
      prefix: Optional[str] = None,
      dir: Optional[pth.AnyPathLike] = None):
    tmp_file: LocalPath = self.host_platform.local_path(
        self.host_platform.mktemp(prefix, dir))
    try:
      yield tmp_file
    finally:
      self.rm(tmp_file, missing_ok=True)

  def mkdtemp(self,
              prefix: Optional[str] = None,
              dir: Optional[pth.AnyPathLike] = None) -> pth.AnyPath:
    self.assert_is_local()
    return self.path(tempfile.mkdtemp(prefix=prefix, dir=dir))

  def mktemp(self,
             prefix: Optional[str] = None,
             dir: Optional[pth.AnyPathLike] = None) -> pth.AnyPath:
    self.assert_is_local()
    fd, name = tempfile.mkstemp(prefix=prefix, dir=dir)
    os.close(fd)
    return self.path(name)

  @contextlib.contextmanager
  def TemporaryDirectory(  # pylint: disable=invalid-name
      self,
      prefix: Optional[str] = None,
      dir: Optional[pth.AnyPathLike] = None):
    tmp_dir = self.mkdtemp(prefix, dir)
    try:
      yield tmp_dir
    finally:
      self.rm(tmp_dir, dir=True, missing_ok=True)

  def exists(self, path: pth.AnyPathLike) -> bool:
    return self.local_path(path).exists()

  def is_file(self, path: pth.AnyPathLike) -> bool:
    return self.local_path(path).is_file()

  def is_dir(self, path: pth.AnyPathLike) -> bool:
    return self.local_path(path).is_dir()

  def iterdir(self,
              path: pth.AnyPathLike) -> Generator[pth.AnyPath, None, None]:
    return self.local_path(path).iterdir()

  def glob(self, path: pth.AnyPathLike,
           pattern: str) -> Generator[pth.AnyPath, None, None]:
    # TODO: support remotely
    return self.local_path(path).glob(pattern)

  def file_size(self, path: pth.AnyPathLike) -> int:
    # TODO: support remotely
    return self.local_path(path).stat().st_size

  def sh_stdout(self,
                *args: CmdArg,
                shell: bool = False,
                quiet: bool = False,
                encoding: str = "utf-8",
                stdin=None,
                env: Optional[Mapping[str, str]] = None,
                check: bool = True) -> str:
    result = self.sh_stdout_bytes(
        *args, shell=shell, quiet=quiet, stdin=stdin, env=env, check=check)
    return result.decode(encoding)

  def sh_stdout_bytes(self,
                      *args: CmdArg,
                      shell: bool = False,
                      quiet: bool = False,
                      stdin=None,
                      env: Optional[Mapping[str, str]] = None,
                      check: bool = True) -> bytes:
    completed_process = self.sh(
        *args,
        shell=shell,
        capture_output=True,
        quiet=quiet,
        stdin=stdin,
        env=env,
        check=check)
    return completed_process.stdout

  def popen(self,
            *args: CmdArg,
            bufsize=-1,
            shell: bool = False,
            stdout=None,
            stderr=None,
            stdin=None,
            env: Optional[Mapping[str, str]] = None,
            quiet: bool = False) -> subprocess.Popen:
    self.assert_is_local()
    if not quiet:
      logging.debug("SHELL: %s", shlex.join(map(str, args)))
      logging.debug("CWD: %s", os.getcwd())
    return subprocess.Popen(
        args=args,
        bufsize=bufsize,
        shell=shell,
        stdin=stdin,
        stderr=stderr,
        stdout=stdout,
        env=env)

  def sh(self,
         *args: CmdArg,
         shell: bool = False,
         capture_output: bool = False,
         stdout=None,
         stderr=None,
         stdin=None,
         env: Optional[Mapping[str, str]] = None,
         quiet: bool = False,
         check: bool = True) -> subprocess.CompletedProcess:
    self.assert_is_local()
    if not quiet:
      logging.debug("SHELL: %s", shlex.join(map(str, args)))
      logging.debug("CWD: %s", os.getcwd())
    process = subprocess.run(
        args=args,
        shell=shell,
        stdin=stdin,
        stdout=stdout,
        stderr=stderr,
        env=env,
        capture_output=capture_output,
        check=False)
    if check and process.returncode != 0:
      raise SubprocessError(self, process)
    return process

  def exec_apple_script(self, script: str, *args: str) -> str:
    del script, args
    raise NotImplementedError("AppleScript is only available on MacOS")

  def log(self, *messages: Any, level: int = 2) -> None:
    message_str = " ".join(map(str, messages))
    if level == 3:
      level = logging.DEBUG
    if level == 2:
      level = logging.INFO
    if level == 1:
      level = logging.WARNING
    if level == 0:
      level = logging.ERROR
    logging.log(level, message_str)

  # TODO(cbruni): split into separate list_system_monitoring and
  # disable_system_monitoring methods
  def check_system_monitoring(self, disable: bool = False) -> bool:
    # pylint: disable=unused-argument
    return True

  def get_relative_cpu_speed(self) -> float:
    return 1

  def is_thermal_throttled(self) -> bool:
    return self.get_relative_cpu_speed() < 1

  def disk_usage(self, path: pth.AnyPathLike) -> psutil._common.sdiskusage:
    return psutil.disk_usage(str(self.local_path(path)))

  def cpu_usage(self) -> float:
    self.assert_is_local()
    return 1 - psutil.cpu_times_percent().idle / 100

  def cpu_details(self) -> Dict[str, Any]:
    self.assert_is_local()
    details = {
        "physical cores":
            psutil.cpu_count(logical=False),
        "logical cores":
            psutil.cpu_count(logical=True),
        "usage":
            psutil.cpu_percent(  # pytype: disable=attribute-error
                percpu=True, interval=0.1),
        "total usage":
            psutil.cpu_percent(),
        "system load":
            psutil.getloadavg(),
        "info":
            self.cpu,
    }
    try:
      cpu_freq = psutil.cpu_freq()
    except FileNotFoundError as e:
      logging.debug("psutil.cpu_freq() failed (normal on macOS M1): %s", e)
      return details
    details.update({
        "max frequency": f"{cpu_freq.max:.2f}Mhz",
        "min frequency": f"{cpu_freq.min:.2f}Mhz",
        "current frequency": f"{cpu_freq.current:.2f}Mhz",
    })
    return details

  def system_details(self) -> Dict[str, Any]:
    return {
        "machine": str(self.machine),
        "os": self.os_details(),
        "python": self.python_details(),
        "CPU": self.cpu_details(),
    }

  def os_details(self) -> JsonDict:
    self.assert_is_local()
    return {
        "system": py_platform.system(),
        "release": py_platform.release(),
        "version": py_platform.version(),
        "platform": py_platform.platform(),
    }

  def python_details(self) -> JsonDict:
    self.assert_is_local()
    return {
        "version": py_platform.python_version(),
        "bits": 64 if sys.maxsize > 2**32 else 32,
    }

  def download_to(self, url: str, path: pth.LocalPath) -> pth.LocalPath:
    self.assert_is_local()
    logging.debug("DOWNLOAD: %s\n       TO: %s", url, path)
    assert not path.exists(), f"Download destination {path} exists already."
    try:
      urllib.request.urlretrieve(url, path)
    except (urllib.error.HTTPError, urllib.error.URLError) as e:
      raise OSError(f"Could not load {url}") from e
    assert path.exists(), (
        f"Downloading {url} failed. Downloaded file {path} doesn't exist.")
    return path

  def concat_files(self,
                   inputs: Iterable[pth.LocalPath],
                   output: pth.LocalPath,
                   prefix: str = "") -> pth.LocalPath:
    self.assert_is_local()
    with output.open("w", encoding="utf-8") as output_f:
      if prefix:
        output_f.write(prefix)
      for input_file in inputs:
        assert input_file.is_file()
        with input_file.open(encoding="utf-8") as input_f:
          shutil.copyfileobj(input_f, output_f)
    return output

  def set_main_display_brightness(self, brightness_level: int) -> None:
    raise NotImplementedError(
        "'set_main_display_brightness' is only available on MacOS for now")

  def get_main_display_brightness(self) -> int:
    raise NotImplementedError(
        "'get_main_display_brightness' is only available on MacOS for now")

  def check_autobrightness(self) -> bool:
    raise NotImplementedError(
        "'check_autobrightness' is only available on MacOS for now")

  def screenshot(self, result_path: pth.AnyPath) -> None:
    # TODO: support screen coordinates
    raise NotImplementedError(
        "'screenshot' is only available on MacOS for now")
