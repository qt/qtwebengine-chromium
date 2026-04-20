# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import logging
from typing import TYPE_CHECKING, Iterator

if TYPE_CHECKING:
  from crossbench.path import LocalPath


@contextlib.contextmanager
def change_cwd(destination: LocalPath) -> Iterator[None]:
  with contextlib.chdir(destination):
    logging.debug("CWD=%s", destination)
    yield
