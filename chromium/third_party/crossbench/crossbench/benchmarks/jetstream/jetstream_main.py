# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar, Type

from typing_extensions import override

from crossbench.benchmarks.jetstream.jetstream_3 import JetStream3Benchmark, \
    JetStream3Probe, JetStream3ProbeContext, JetStream3Story, ProbeClsTupleT

if TYPE_CHECKING:
  from crossbench.benchmarks.base import VersionParts


class JetStreamMainProbe(JetStream3Probe):
  __doc__ = JetStream3Probe.__doc__
  NAME: ClassVar[str] = "jetstream_main"

  @override
  def get_context_cls(self) -> Type[JetStreamMainProbeContext]:
    return JetStreamMainProbeContext


class JetStreamMainProbeContext(JetStream3ProbeContext):
  pass


class JetStreamMainStory(JetStream3Story):
  __doc__ = JetStream3Story.__doc__
  NAME: ClassVar[str] = "jetstream_main"
  URL: ClassVar[str] = "https://chromium-workloads.web.app/jetstream/main/"
  URL_OFFICIAL: ClassVar[
      str] = "https://chromium-workloads.web.app/jetstream/main/"
  URL_CHROME_FORK: ClassVar[
      str] = "https://chromium-workloads.web.app/jetstream/main-custom/"
  # Contents of running:
  # JSON.stringify(BENCHMARKS.map(e => e.name).sort((a, b) => a.toLowerCase() < b.toLowerCase() ? 1 : -1), undefined, " ")
  SUBSTORIES: ClassVar[tuple[str, ...]] = (
      "zlib-wasm",
      "WSL",
      "web-ssr",
      "validatorjs",
      "UniPoker",
      "typescript-octane",
      "typescript-lib",
      "tsf-wasm",
      "transformersjs-whisper-wasm",
      "transformersjs-bert-wasm",
      "threejs",
      "tfjs-wasm-simd",
      "tfjs-wasm",
      "sync-fs",
      "Sunspider",
      "stanford-crypto-sha256",
      "stanford-crypto-pbkdf2",
      "stanford-crypto-aes",
      "sqlite3-wasm",
      "splay",
      "source-map-wtb",
      "segmentation",
      "richards-wasm",
      "richards",
      "regexp-octane",
      "raytrace-public-class-fields",
      "raytrace-private-class-fields",
      "raytrace",
      "quicksort-wasm",
      "proxy-vue",
      "proxy-mobx",
      "prismjs-startup-es6",
      "prismjs-startup-es5",
      "prettier-wtb",
      "postcss-wtb",
      "pdfjs",
      "OfflineAssembler",
      "octane-code-load",
      "navier-stokes",
      "multi-inspector-code-load",
      "ML",
      "mandreel",
      "lebab-wtb",
      "lazy-collections",
      "Kotlin-compose-wasm",
      "json-stringify-inspector",
      "json-parse-inspector",
      "jsdom-d3-startup",
      "js-tokens",
      "j2cl-box2d-wasm",
      "intl",
      "HashSet-wasm",
      "hash-map",
      "gcc-loops-wasm",
      "gbemu",
      "gaussian-blur",
      "FlightPlanner",
      "first-inspector-code-load",
      "esprima-next-wtb",
      "espree-wtb",
      "earley-boyer",
      "doxbee-promise",
      "doxbee-async",
      "dotnet-interp-wasm",
      "dotnet-aot-wasm",
      "delta-blue",
      "Dart-flute-todomvc-wasm",
      "Dart-flute-complex-wasm",
      "crypto",
      "chai-wtb",
      "cdjs",
      "Box2D",
      "bomb-workers",
      "bigint-paillier",
      "bigint-noble-secp256k1",
      "bigint-noble-ed25519",
      "bigint-noble-bls12-381",
      "bigint-bigdenary",
      "Basic",
      "babylonjs-startup-es6",
      "babylonjs-startup-es5",
      "babylonjs-scene-es6",
      "babylonjs-scene-es5",
      "babylon-wtb",
      "Babylon",
      "babel-wtb",
      "babel-minify-wtb",
      "async-fs",
      "argon2-wasm",
      "Air",
      "ai-astar",
      "acorn-wtb",
      "8bitbench-wasm",
  )


class JetStreamMainBenchmark(JetStream3Benchmark):
  """
  Benchmark runner for the JetStream main development version.
  """

  NAME: ClassVar[str] = "jetstream_main"
  DEFAULT_STORY_CLS: ClassVar = JetStreamMainStory
  PROBES: ClassVar[ProbeClsTupleT] = (JetStreamMainProbe,)

  @classmethod
  @override
  def version(cls) -> VersionParts:
    return ("main",)

  @classmethod
  @override
  def aliases(cls) -> tuple[str, ...]:
    return ("jetstream_3", "js3") + super().aliases()
