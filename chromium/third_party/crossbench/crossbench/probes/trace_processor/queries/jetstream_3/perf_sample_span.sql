-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Perfetto script that exports pprof profiles per story, only including
-- score-relevant time intervals.

DROP VIEW IF EXISTS perf_sample_span;

CREATE VIEW
  perf_sample_span AS
SELECT
  ts,
  0 AS dur,
  utid,
  cpu,
  callsite_id
FROM
  perf_sample
UNION ALL
SELECT
  ts,
  0 AS dur,
  utid,
  cpu,
  callsite_id
FROM
  instruments_sample;

DROP VIEW IF EXISTS jetstream_measure_iterations;

CREATE VIEW
  jetstream_measure_iterations AS
SELECT
  id,
  ts,
  dur,
  substr (name, 1, instr (name, '-iteration-') -1) AS name,
  cast(
    substr (name, instr (name, '-iteration-') + 11) AS integer
  ) AS iteration
FROM
  slice
WHERE
  category = 'blink.user_timing'
  AND (
   name LIKE 'Air-iteration-%'
   OR name LIKE 'Babylon-iteration-%'
   OR name LIKE 'Basic-iteration-%'
   OR name LIKE 'Box2D-iteration-%'
   OR name LIKE 'FlightPlanner-iteration-%'
   OR name LIKE 'ML-iteration-%'
   OR name LIKE 'OfflineAssembler-iteration-%'
   OR name LIKE 'Sunspider-iteration-%'
   OR name LIKE 'UniPoker-iteration-%'
   OR name LIKE 'WSL-iteration-%'
   OR name LIKE 'acorn-wtb-iteration-%'
   OR name LIKE 'ai-astar-iteration-%'
   OR name LIKE 'async-fs-iteration-%'
   OR name LIKE 'babel-minify-wtb-iteration-%'
   OR name LIKE 'babel-wtb-iteration-%'
   OR name LIKE 'babylon-wtb-iteration-%'
   OR name LIKE 'bigint-bigdenary-iteration-%'
   OR name LIKE 'bigint-noble-bls12-381-iteration-%'
   OR name LIKE 'bigint-noble-ed25519-iteration-%'
   OR name LIKE 'bigint-noble-secp256k1-iteration-%'
   OR name LIKE 'bigint-paillier-iteration-%'
   OR name LIKE 'bomb-workers-iteration-%'
   OR name LIKE 'cdjs-iteration-%'
   OR name LIKE 'chai-wtb-iteration-%'
   OR name LIKE 'crypto-iteration-%'
   OR name LIKE 'delta-blue-iteration-%'
   OR name LIKE 'doxbee-async-iteration-%'
   OR name LIKE 'doxbee-promise-iteration-%'
   OR name LIKE 'earley-boyer-iteration-%'
   OR name LIKE 'espree-wtb-iteration-%'
   OR name LIKE 'esprima-next-wtb-iteration-%'
   OR name LIKE 'first-inspector-code-load-iteration-%'
   OR name LIKE 'gaussian-blur-iteration-%'
   OR name LIKE 'gbemu-iteration-%'
   OR name LIKE 'hash-map-iteration-%'
   OR name LIKE 'intl-iteration-%'
   OR name LIKE 'js-tokens-iteration-%'
   OR name LIKE 'json-parse-inspector-iteration-%'
   OR name LIKE 'json-stringify-inspector-iteration-%'
   OR name LIKE 'lazy-collections-iteration-%'
   OR name LIKE 'lebab-wtb-iteration-%'
   OR name LIKE 'mandreel-iteration-%'
   OR name LIKE 'multi-inspector-code-load-iteration-%'
   OR name LIKE 'navier-stokes-iteration-%'
   OR name LIKE 'octane-code-load-iteration-%'
   OR name LIKE 'pdfjs-iteration-%'
   OR name LIKE 'postcss-wtb-iteration-%'
   OR name LIKE 'prettier-wtb-iteration-%'
   OR name LIKE 'prismjs-startup-es5-iteration-%'
   OR name LIKE 'prismjs-startup-es6-iteration-%'
   OR name LIKE 'proxy-mobx-iteration-%'
   OR name LIKE 'proxy-vue-iteration-%'
   OR name LIKE 'raytrace-iteration-%'
   OR name LIKE 'raytrace-private-class-fields-iteration-%'
   OR name LIKE 'raytrace-public-class-fields-iteration-%'
   OR name LIKE 'regexp-iteration-%'
   OR name LIKE 'richards-iteration-%'
   OR name LIKE 'segmentation-iteration-%'
   OR name LIKE 'source-map-wtb-iteration-%'
   OR name LIKE 'splay-iteration-%'
   OR name LIKE 'stanford-crypto-aes-iteration-%'
   OR name LIKE 'stanford-crypto-pbkdf2-iteration-%'
   OR name LIKE 'stanford-crypto-sha256-iteration-%'
   OR name LIKE 'sync-fs-iteration-%'
   OR name LIKE 'threejs-iteration-%'
   OR name LIKE 'typescript-lib-iteration-%'
   OR name LIKE 'typescript-octane-iteration-%'
   OR name LIKE 'validatorjs-iteration-%'
   OR name LIKE 'web-ssr-iteration-%'
   OR name LIKE 'babylonjs-startup-es5-iteration-%'
   OR name LIKE 'babylonjs-startup-es6-iteration-%'
   OR name LIKE 'babylonjs-scene-es5-iteration-%'
   OR name LIKE 'babylonjs-scene-es6-iteration-%'
   OR name LIKE 'jsdom-d3-startup-iteration-%'
   OR name LIKE 'mobx-startup-iteration-%'
  )
  AND dur > 0;

DROP VIEW IF EXISTS jetstream_measure_wasm;

CREATE VIEW
  jetstream_measure_wasm AS
SELECT
  id,
  ts,
  dur,
  substr (name, 1, instr (name, '-wasm-') + 4) AS name,
  substr (name, instr (name, '-wasm-') + 6) AS subtest
FROM
  slice
WHERE
  category = 'blink.user_timing'
  AND (
    name LIKE 'tsf-wasm-%'
    OR name LIKE 'richards-wasm-%'
    OR name LIKE 'quicksort-wasm-%'
    OR name LIKE 'HashSet-wasm-%'
    OR name LIKE '8bitbench-wasm-iteration-%'
    OR name LIKE 'Dart-flute-complex-wasm-iteration-%'
    OR name LIKE 'Dart-flute-todomvc-wasm-iteration-%'
    OR name LIKE 'HashSet-wasm-iteration-%'
    OR name LIKE 'Kotlin-compose-wasm-iteration-%'
    OR name LIKE 'argon2-wasm-iteration-%'
    OR name LIKE 'dotnet-aot-wasm-iteration-%'
    OR name LIKE 'dotnet-interp-wasm-iteration-%'
    OR name LIKE 'gcc-loops-wasm-iteration-%'
    OR name LIKE 'j2cl-box2d-wasm-iteration-%'
    OR name LIKE 'quicksort-wasm-iteration-%'
    OR name LIKE 'richards-wasm-iteration-%'
    OR name LIKE 'sqlite3-wasm-iteration-%'
    OR name LIKE 'tfjs-wasm-iteration-%'
    OR name LIKE 'tfjs-wasm-simd-iteration-%'
    OR name LIKE 'transformersjs-bert-wasm-iteration-%'
    OR name LIKE 'transformersjs-whisper-wasm-iteration-%'
    OR name LIKE 'tsf-wasm-iteration-%'
    OR name LIKE 'zlib-wasm-iteration-%'
  )
  AND dur > 0;

DROP VIEW IF EXISTS jetstream_measure;

CREATE VIEW
  jetstream_measure AS
SELECT
  id,
  ts,
  dur,
  name,
  iteration,
  IIF (rank_within_name <= 4, 'Worst', 'Average') AS subtest
FROM
  (
    SELECT
      id,
      ts,
      dur,
      name,
      iteration,
      RANK() OVER (
        PARTITION BY
          name
        ORDER BY
          dur DESC
      ) AS rank_within_name
    FROM
      jetstream_measure_iterations
    WHERE
      iteration > 0
  )
UNION ALL
SELECT
  id,
  ts,
  dur,
  name,
  iteration,
  'First' AS subtest
FROM
  jetstream_measure_iterations
WHERE
  iteration = 0
UNION ALL
SELECT
  id,
  ts,
  dur,
  name,
  0 AS iteration,
  subtest
FROM
  jetstream_measure_wasm;

DROP TABLE IF EXISTS jetstream_sample;

CREATE VIRTUAL TABLE jetstream_sample USING SPAN_JOIN (jetstream_measure, perf_sample_span);

-- WRITE_FILE needs trace_processor's --dev flag.
SELECT
  'jetstream.pprof' as file_name,
  WRITE_FILE (
    'jetstream.pprof',
    (
      SELECT
        EXPERIMENTAL_PROFILE (
          CAT_STACKS (
            jetstream_sample.name,
            jetstream_sample.subtest,
            process.name,
            IIF (
              instr (thread.name, ' ') > 0,
              substr (thread.name, 1, instr (thread.name, ' ') -1),
              thread.name
            ),
            STACK_FROM_STACK_PROFILE_CALLSITE (callsite_id)
          ),
          'samples',
          'count',
          1
        ) AS profile
      FROM
        jetstream_sample
        JOIN thread ON jetstream_sample.utid = thread.utid
        JOIN process ON thread.upid = process.upid
        --WHERE thread.name LIKE "ThreadPoolForegroundWorker%"
    )
  ) as file_size