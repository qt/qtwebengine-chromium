# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
from typing import Any

from .base import ActionRunner
from .basic_action_runner import BasicActionRunner


# TODO: migrate to full config.ConfigObject
class ActionRunnerConfig:

  @classmethod
  def parse(cls, value: Any) -> ActionRunner:
    if isinstance(value, ActionRunner):
      return value
    if value == "basic":
      return BasicActionRunner()
    raise argparse.ArgumentTypeError(
        f"Invalid choice '{value}', allowed values are 'basic'")
