# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import shlex
import subprocess
from typing import TYPE_CHECKING, Any, Dict, List, Mapping, Optional, Union

from crossbench.plt.arch import MachineArch
from crossbench.plt.base import CmdArgsT, CmdArgT, ListCmdArgsT
from crossbench.plt.linux import LinuxPlatform

if TYPE_CHECKING:
  from crossbench.path import LocalPath, RemotePath
  from crossbench.plt.base import Platform


class SshPlatform:
  """TODO: use abstract base class"""

  @property
  def is_remote_ssh(self) -> bool:
    return True


class LinuxSshPlatform(SshPlatform, LinuxPlatform):

  def __init__(self, host_platform: Platform, host: str, port: int,
               ssh_port: int, ssh_user: str) -> None:
    super().__init__()
    self._machine: Optional[MachineArch] = None
    self._system_details: Optional[Dict[str, Any]] = None
    self._cpu_details: Optional[Dict[str, Any]] = None
    self._host_platform = host_platform
    self._host = host
    self._port = port
    self._ssh_port = ssh_port
    self._ssh_user = ssh_user

  @property
  def is_remote(self) -> bool:
    return True

  @property
  def name(self) -> str:
    return "linux_ssh"

  @property
  def host_platform(self) -> Platform:
    return self._host_platform

  @property
  def host(self) -> str:
    return self._host

  @property
  def port(self) -> int:
    return self._port

  def _build_ssh_cmd(self, *args: CmdArgT, shell=False) -> ListCmdArgsT:
    ssh_cmd: ListCmdArgsT = [
        "ssh", "-p", f"{self._ssh_port}", f"{self._ssh_user}@{self._host}"
    ]
    if shell:
      ssh_cmd.append(*args)
    else:
      ssh_cmd.append(shlex.join(map(str, args)))
    return ssh_cmd

  def sh_stdout(self,
                *args: CmdArgT,
                shell: bool = False,
                quiet: bool = False,
                encoding: str = "utf-8",
                env: Optional[Mapping[str, str]] = None,
                check: bool = True) -> str:
    ssh_cmd: ListCmdArgsT = self._build_ssh_cmd(*args, shell=shell)
    return self._host_platform.sh_stdout(
        *ssh_cmd, env=env, quiet=quiet, encoding=encoding, check=check)

  def sh(self,
         *args: CmdArgT,
         shell: bool = False,
         capture_output: bool = False,
         stdout=None,
         stderr=None,
         stdin=None,
         env: Optional[Mapping[str, str]] = None,
         quiet: bool = False,
         check: bool = True) -> subprocess.CompletedProcess:
    ssh_cmd: ListCmdArgsT = self._build_ssh_cmd(*args, shell=shell)
    return self._host_platform.sh(
        *ssh_cmd,
        capture_output=capture_output,
        stdout=stdout,
        stderr=stderr,
        stdin=stdin,
        env=env,
        quiet=quiet,
        check=check)

  def processes(self,
                attrs: Optional[List[str]] = None) -> List[Dict[str, Any]]:
    # TODO: Define a more generic method in PosixPlatform, possibly with
    # an overridable function to generate ps command line.
    lines = self.sh_stdout("ps", "-A", "-o", "pid,cmd").splitlines()
    if len(lines) == 1:
      return []

    res: List[Dict[str, Any]] = []
    for line in lines[1:]:
      pid, name = line.split(maxsplit=1)
      res.append({"pid": int(pid), "name": name})
    return res

  def push(self, from_path: LocalPath, to_path: RemotePath) -> RemotePath:
    scp_cmd: CmdArgsT = [
        "scp", "-P", f"{self._ssh_port}", f"{from_path}",
        f"{self._ssh_user}@{self._host}:{to_path}"
    ]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path

  def pull(self, from_path: RemotePath, to_path: LocalPath) -> LocalPath:
    scp_cmd: CmdArgsT = [
        "scp", "-P", f"{self._ssh_port}",
        f"{self._ssh_user}@{self._host}:{from_path}", f"{to_path}"
    ]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path

  def rsync(self, from_path: RemotePath, to_path: LocalPath) -> LocalPath:
    to_path.parent.mkdir(parents=True, exist_ok=True)
    scp_cmd: CmdArgsT = [
        "scp", "-P", f"{self._ssh_port}", "-r",
        f"{self._ssh_user}@{self._host}:{from_path}", f"{to_path}"
    ]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path
