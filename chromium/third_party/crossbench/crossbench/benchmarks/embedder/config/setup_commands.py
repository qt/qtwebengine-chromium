# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import dataclasses
from typing import Any, Self

from typing_extensions import override

from crossbench import exception
from crossbench.config import ConfigObject
from crossbench.parse import ObjectParser


@dataclasses.dataclass(frozen=True)
class SetupCommandConfig:
  label: str
  command: tuple[str, ...] = ()


@dataclasses.dataclass(frozen=True)
class SetupCommandsConfig(ConfigObject):
  commands: tuple[SetupCommandConfig, ...] = ()

  @override
  def validate(self) -> None:
    super().validate()
    for index, command in enumerate(self.commands):
      assert isinstance(command, SetupCommandConfig), (
          f"commands[{index}] is not an SetupCommandConfig "
          f"but {type(command).__name__}")

  @classmethod
  @override
  def parse_str(cls, value: str) -> Self:
    del value
    raise NotImplementedError("Cannot create SetupCommandsConfig from string")

  @classmethod
  @override
  def parse_dict(cls, config: dict, **kwargs) -> Self:
    """
    Variant a):
      { "setup_commands": { "LABEL": ["command", "arg1", "arg2"] } }
    """
    with exception.annotate_argparsing("Parsing setup commands"):
      if "setup_commands" not in config:
        raise argparse.ArgumentTypeError(
            "Config does not provide a 'setup_commands' dict.")
      commands_config = ObjectParser.non_empty_dict(config["setup_commands"],
                                                    "setup_commands")
      with exception.annotate_argparsing("Parsing config 'setup_commands'"):
        commands = cls._parse_commands(commands_config)
        return cls(commands)
    raise exception.UnreachableError

  @classmethod
  def _parse_commands(cls, data: dict[str,
                                      Any]) -> tuple[SetupCommandConfig, ...]:
    commands = []
    for name, command_config in data.items():
      with exception.annotate_argparsing(f"Parsing command ...['{name}']"):
        command_list = ObjectParser.sequence(command_config, "command")
        command = SetupCommandConfig(
            label=name, command=tuple(str(s) for s in command_list))
        commands.append(command)
    return tuple(commands)
