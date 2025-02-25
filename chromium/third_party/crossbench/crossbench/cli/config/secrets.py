# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import dataclasses
from typing import Dict, Type

from immutabledict import immutabledict

from crossbench import exception
from crossbench.cli.config.secret_type import SecretType
from crossbench.config import ConfigObject, ConfigParser
from crossbench.parse import ObjectParser

SecretsDict = immutabledict[SecretType, "Secret"]


@dataclasses.dataclass(frozen=True)
class SecretsConfig(ConfigObject):
  secrets: SecretsDict = dataclasses.field(default_factory=immutabledict)

  @classmethod
  def parse_str(cls, value: str) -> SecretsConfig:
    if value[0] == "{":
      return cls.parse_inline_hjson(value)
    # TODO: maybe support passwd style string format
    raise NotImplementedError("Cannot create secrets from string")

  @classmethod
  def parse_dict(cls, config: Dict) -> SecretsConfig:
    secrets = {}
    for type_str, secret_data in config.items():
      secret_type = SecretType.parse(type_str)
      with exception.annotate_argparsing("Parsing Secret details:"):
        secret = Secret.parse_dict(secret_data, type=secret_type)
      assert isinstance(secret,
                        Secret), f"Expected {cls} but got {type(secret)}"
      assert secret_type not in secrets, f"Duplicate entry for {type_str}"
      secrets[secret_type] = secret
    return SecretsConfig(immutabledict(secrets))

  def as_dict(self) -> SecretsDict:
    return self.secrets


@dataclasses.dataclass(frozen=True)
class Secret(ConfigObject):
  type: SecretType
  username: str
  password: str

  @classmethod
  def config_parser(cls: Type[Secret]) -> ConfigParser[Secret]:
    parser = ConfigParser(f"{cls.__name__} parser", cls)
    parser.add_argument("type", type=SecretType, required=True)
    parser.add_argument(
        "username",
        aliases=("user", "usr", "account"),
        type=ObjectParser.non_empty_str,
        required=True)
    parser.add_argument(
        "password",
        aliases=("pass", "pw"),
        type=ObjectParser.any_str,
        required=True)
    return parser

  @classmethod
  def parse_dict(  # pylint: disable=arguments-differ
      cls, config: Dict, **kwargs) -> Secret:
    return cls.config_parser().parse(config, **kwargs)

  @classmethod
  def parse_str(cls, value: str):
    # TODO: maybe support passwd style string format
    raise NotImplementedError("Cannot support")
