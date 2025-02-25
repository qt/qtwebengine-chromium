# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING

from crossbench import path as pth
from crossbench import plt
from crossbench.plt.linux_ssh import LinuxSshPlatform

if TYPE_CHECKING:
  from typing import Optional

  from crossbench.flags.chrome import ChromeFlags
  from crossbench.plt.base import ListCmdArgs


class ChromeOsSshPlatform(LinuxSshPlatform):

  AUTOLOGIN_PATH = pth.AnyPosixPath("/usr/local/autotest/bin/autologin.py")
  DEVTOOLS_PORT_PATH = pth.AnyPosixPath("/home/chronos/DevToolsActivePort")

  def __init__(self, *args, **kwargs):
    self._username: Optional[str] = None
    super().__init__(*args, **kwargs)

  @property
  def name(self) -> str:
    return "chromeos_ssh"

  @property
  def username(self) -> Optional[str]:
    return self._username

  @property
  def is_chromeos(self) -> bool:
    return True

  def create_debugging_session(self,
                               browser_flags: Optional[ChromeFlags] = None,
                               username: Optional[str] = None,
                               password: Optional[str] = None) -> int:
    try:
      args: ListCmdArgs = [self.AUTOLOGIN_PATH]
      if username and password:
        self._username = username
        args.extend(("-u", username, "-p", password))
      if browser_flags:
        args.append("--")
        args.extend(browser_flags)
      self.sh(*args)
    except plt.SubprocessError as e:
      raise RuntimeError("Autologin failed.") from e
    try:
      dbg_port = self.cat(self.DEVTOOLS_PORT_PATH).splitlines()[0].strip()
    except plt.SubprocessError as e:
      raise RuntimeError("Could not read remote debugging port.") from e
    return int(dbg_port)
