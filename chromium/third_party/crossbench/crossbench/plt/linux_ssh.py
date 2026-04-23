# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import shlex
from typing import TYPE_CHECKING, Any, Mapping, Optional

from typing_extensions import override

from crossbench.plt.linux import RemoteLinuxPlatform
from crossbench.plt.ssh import SshPlatformMixin
from crossbench.plt.ssh_port_manager import SshPortManager

if TYPE_CHECKING:
  from crossbench.path import AnyPath, LocalPath
  from crossbench.plt.arch import MachineArch
  from crossbench.plt.base import Platform
  from crossbench.plt.port_manager import PortManager
  from crossbench.plt.types import CmdArg, CmdArgs, ListCmdArgs


class LinuxSshPlatform(SshPlatformMixin, RemoteLinuxPlatform):

  def __init__(self, host_platform: Platform, host: str, port: int,
               ssh_port: int, ssh_user: str) -> None:
    super().__init__(host_platform, host, port, ssh_port, ssh_user)
    self._machine: MachineArch | None = None
    self._system_details: dict[str, Any] | None = None
    self._cpu_details: dict[str, Any] | None = None

  def _create_port_manager(self) -> PortManager:
    return SshPortManager(self)

  @property
  @override
  def name(self) -> str:
    return "linux_ssh"

  def build_ssh_cmd(self,
                    *args: CmdArg,
                    shell: bool = False,
                    env: Optional[Mapping[str, str]] = None,
                    cwd: Optional[AnyPath] = None) -> ListCmdArgs:
    if env:
      # TODO: support env with "export FOO=bar;" prefixes
      raise ValueError(f"{self} platform only supports an empty env for now.")
    if cwd:
      # TODO: support env with "cd foo/bar &&" prefix
      raise ValueError(f"{self} platform does not support custom cwd")
    self.validate_shell_args(args, shell)
    ssh_cmd: ListCmdArgs = [
        "ssh", "-p", f"{self._ssh_port}", f"{self._ssh_user}@{self._host}"
    ]
    ssh_cmd.append(shlex.join(map(str, args)))

    if not shell:
      return ssh_cmd

    combined_ssh_cmd: str = " ".join(map(str, ssh_cmd))
    return [combined_ssh_cmd]

  @override
  def build_shell_cmd(
      self,
      *args: CmdArg,
      shell: bool = False,
      env: Optional[Mapping[str, str]] = None,
      cwd: Optional[AnyPath] = None,
  ) -> ListCmdArgs:
    return self.build_ssh_cmd(*args, shell=shell, env=env, cwd=cwd)

  def processes(self,
                attrs: Optional[list[str]] = None) -> list[dict[str, Any]]:
    del attrs
    # TODO: Define a more generic method in PosixPlatform, possibly with
    # an overridable function to generate ps command line.
    lines = self.sh_stdout("ps", "-A", "-o", "pid,cmd").splitlines()
    if len(lines) == 1:
      return []

    res: list[dict[str, Any]] = []
    for line in lines[1:]:
      pid, name = line.split(maxsplit=1)
      res.append({"pid": int(pid), "name": name})
    return res

  def push(self, from_path: LocalPath, to_path: AnyPath) -> AnyPath:
    self.mkdir(to_path.parent, parents=True, exist_ok=True)

    scp_cmd: ListCmdArgs = ["scp", "-P", f"{self._ssh_port}"]
    if from_path.is_dir():
      scp_cmd.append("-r")
    scp_cmd += [f"{from_path}", f"{self._ssh_user}@{self._host}:{to_path}"]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path

  def pull(self, from_path: AnyPath, to_path: LocalPath) -> LocalPath:
    self._host_platform.mkdir(to_path.parent, parents=True, exist_ok=True)

    scp_cmd: CmdArgs = [
        "scp", "-P", f"{self._ssh_port}",
        f"{self._ssh_user}@{self._host}:{from_path}", to_path
    ]
    self._host_platform.sh_stdout(*scp_cmd)
    return to_path
