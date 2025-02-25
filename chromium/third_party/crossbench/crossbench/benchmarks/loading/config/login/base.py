# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import logging
from typing import TYPE_CHECKING, Final

from crossbench.benchmarks.loading.config.blocks import ActionBlock

if TYPE_CHECKING:
  from crossbench.benchmarks.loading.page.interactive import InteractivePage
  from crossbench.cli.config.secrets import Secret, SecretType
  from crossbench.runner.run import Run


class BaseLoginBlock(ActionBlock):
  LABEL: Final[str] = "login"

  def validate(self) -> None:
    super().validate()
    assert self.index == 0, (
        f"Login block has to be the first, but got {self.index}")

  @property
  def is_login(self) -> bool:
    return True

  def get_secret(
      self,
      run: Run,
      page: InteractivePage,
      type: SecretType  # pylint: disable=redefined-builtin
  ) -> Secret:
    logging.debug("Looking up secrets {%s} for page %s", type, page)
    if secret := page.secrets.get(type):
      return secret
    if secret := run.browser.secrets.get(type):
      return secret
    raise LookupError(f"Could not find any secret for {repr(str(type))} "
                      f"on {page} or on {run.browser}")

  def is_logged_in(self,
                   run: Run,
                   secret: Secret,
                   strict: bool = False) -> bool:
    return run.browser.is_logged_in(secret, strict)


class PresetLoginBlock(BaseLoginBlock):

  def validate_actions(self) -> None:
    """Skip validation, since PresetLoginBlocks have an unknown number
    of actions."""

  def __len__(self) -> int:
    """LoginBlocks will have at least one action. Given they're not known
    upfront we set this to 1. This also ensures that bool(login_block) is
    True."""
    return 1
