# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import logging
from typing import TYPE_CHECKING, Iterable, List, Optional

import sqlalchemy
from sqlalchemy import orm

from crossbench.results_db.records.base import BaseRecord
from crossbench.results_db.records.browser import BrowserRecord
from crossbench.results_db.records.platform import PlatformRecord
from crossbench.results_db.records.run import RunRecord

if TYPE_CHECKING:
  from crossbench import path as pth
  from crossbench.browsers.browser import Browser
  from crossbench.plt.base import Platform
  from crossbench.runner.run import Run


class ResultsDB:

  def __init__(self, db_file: Optional[pth.LocalPath] = None):
    self._db_file: Optional[pth.LocalPath] = db_file
    init_tables: bool = True
    engine_url: str = "sqlite:///:memory:"
    if db_file:
      init_tables = not db_file.exists()
      engine_url = f"sqlite:///{self._db_file}"
    is_debug_logging = logging.getLogger().isEnabledFor(logging.DEBUG)
    self._engine = sqlalchemy.create_engine(engine_url, echo=is_debug_logging)
    if init_tables:
      BaseRecord.metadata.create_all(self._engine)

  @property
  def is_in_memory(self) -> bool:
    return not self._db_file

  @property
  def db_file(self) -> pth.LocalPath:
    if not self._db_file:
      raise RuntimeError("In-memory ResultDB has no DB file.")
    return self._db_file

  @property
  def engine(self) -> sqlalchemy.engine.Engine:
    return self._engine

  @contextlib.contextmanager
  def session(self):
    with orm.Session(self._engine) as session:
      yield session

  def setup_runs(self, runs: List[Run]) -> None:
    platforms = {run.browser_platform for run in runs}
    self.add_platforms(platforms)
    browsers = {run.browser for run in runs}
    self.add_browsers(browsers)
    self.add_runs(runs)

  def add_runs(self, runs: List[Run]) -> None:
    with self.session() as session:
      for run in runs:
        record = RunRecord.create(session, run)
        session.add(record)
      session.commit()

  def add_platforms(self, platforms: Iterable[Platform]) -> None:
    with self.session() as session:
      for platform in set(platforms):
        record = PlatformRecord.create(session, platform)
        session.add(record)
      session.commit()

  def add_browsers(self, browsers: Iterable[Browser]) -> None:
    with self.session() as session:
      for browser in set(browsers):
        record = BrowserRecord.create(session, browser)
        session.add(record)
      session.commit()
