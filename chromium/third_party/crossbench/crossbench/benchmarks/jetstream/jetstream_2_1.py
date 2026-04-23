# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.benchmarks.jetstream.jetstream_2 import JetStream2Benchmark, \
    JetStream2Probe, JetStream2ProbeContext, JetStream2Story, ProbeClsTupleT

if TYPE_CHECKING:
  from crossbench.benchmarks.base import VersionParts


class JetStream21Probe(JetStream2Probe):
  __doc__ = JetStream2Probe.__doc__
  NAME: ClassVar[str] = "jetstream_2.1"

  @override
  def get_context_cls(self) -> Type[JetStream21ProbeContext]:
    return JetStream21ProbeContext


class JetStream21ProbeContext(JetStream2ProbeContext):
  pass


class JetStream21Story(JetStream2Story):
  __doc__ = JetStream2Story.__doc__
  NAME: ClassVar[str] = "jetstream_2.1"
  URL: ClassVar[str] = "https://chromium-workloads.web.app/jetstream/v2.1/"
  URL_OFFICIAL: ClassVar[str] = "https://browserbench.org/JetStream2.1/"
  URL_CHROME_FORK: ClassVar[
      str] = "https://chromium-workloads.web.app/jetstream/v2.1-custom/"


class JetStream21Benchmark(JetStream2Benchmark):
  """
  Benchmark runner for JetStream 2.1.
  """

  NAME: ClassVar[str] = "jetstream_2.1"
  DEFAULT_STORY_CLS: ClassVar = JetStream21Story
  PROBES: ClassVar[ProbeClsTupleT] = (JetStream21Probe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    return (2, 1)
