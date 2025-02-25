# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import shlex
import subprocess
from typing import TYPE_CHECKING, Any, Dict, List, Mapping, Optional

from crossbench.plt.arch import MachineArch
from crossbench.plt.linux import RemoteLinuxPlatform
from crossbench.plt.ssh import SshPlatformMixin

if TYPE_CHECKING:
  from crossbench.path import AnyPath, LocalPath
  from crossbench.plt.base import CmdArg, CmdArgs, ListCmdArgs, Platform


class LinuxSshPlatform(SshPlatformMixin, RemoteLinuxPlatform):

  def __init__(self, host_platform: Platform, host: str, port: int,
               ssh_port: int, ssh_user: str) -> None:
    super().__init__(host_platform)
    self._machine: Optional[MachineArch] = None
    self._system_details: Optional[Dict[str, Any]] = None
    self._cpu_details: Optional[Dict[str, Any]] = None
    # TODO: move ssh-related code to SshPlatformMixin
    self._host = host
    self._port = port
    self._ssh_port = ssh_port
    self._ssh_user = ssh_user

  @property
  def name(self) -> str:
    return "linux_ssh"

  @property
  def host(self) -> str:
    return self._host

  @property
  def port(self) -> int:
    return self._port

  @property
  def ssh_user(self) -> str:
    return self._ssh_user

  @property
  def ssh_port(self) -> int:
    return self._ssh_port

  def _build_ssh_cmd(self, *args: CmdArg, shell=False) -> ListCmdArgs:
    ssh_cmd: ListCmdArgs = [
        "ssh", "-p", f"{self._ssh_port}", f"{self._ssh_user}@{self._host}"
    ]
    if shell:
      ssh_cmd.append(*args)
    else:
      ssh_cmd.append(shlex.join(map(str, args)))
    return ssh_cmd

  def sh_stdout_bytes(self,
                      *args: CmdArg,
                      shell: bool = False,
                      quiet: bool = False,
                      stdin=None,
                      env: Optional[Mapping[str, str]] = None,
                      check: bool = True) -> bytes:
    ssh_cmd: ListCmdArgs = self._build_ssh_cmd(*args, shell=shell)
    return self._host_platform.sh_stdout_bytes(
        *ssh_cmd, quiet=quiet, stdin=stdin, env=env, check=check)

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
    ssh_cmd: ListCmdArgs = self._build_ssh_cmd(*args, shell=shell)
    return self._host_platform.sh(
        *ssh_cmd,
        capture_output=capture_output,
        stdout=stdout,
        stderr=stderr,
        stdin=stdin,
        env=env,
        quiet=quiet,
        check=check)

  def popen(self,
            *args: CmdArg,
            bufsize=-1,
            shell: bool = False,
            stdout=None,
            stderr=None,
            stdin=None,
            env: Optional[Mapping[str, str]] = None,
            quiet: bool = False) -> subprocess.Popen:
    ssh_cmd: ListCmdArgs = self._build_ssh_cmd(*args, shell=shell)
    return self._host_platform.popen(
        *ssh_cmd,
        bufsize=bufsize,
        shell=shell,
        stdout=stdout,
        stderr=stderr,
        stdin=stdin,
        env=env,
        quiet=quiet)

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

  def push(self, from_path: LocalPath, to_path: AnyPath) -> AnyPath:
    scp_cmd: CmdArgs = [
        "scp", "-P", f"{self._ssh_port}", f"{from_path}",
        f"{self._ssh_user}@{self._host}:{to_path}"
    ]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path

  def pull(self, from_path: AnyPath, to_path: LocalPath) -> LocalPath:
    scp_cmd: CmdArgs = [
        "scp", "-P", f"{self._ssh_port}",
        f"{self._ssh_user}@{self._host}:{from_path}", f"{to_path}"
    ]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path
