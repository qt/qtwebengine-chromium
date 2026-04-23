-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
INCLUDE PERFETTO MODULE ext.loading_interesting_intervals;

DROP VIEW IF EXISTS interaction_latency_metric;

CREATE VIEW interaction_latency_metric
AS
SELECT
  interval_id,
  "average_interaction_latency" AS metric_name,
  "usec" AS unit,
  avg(original_dur) / 1e3 AS value
FROM interesting_slice_start
WHERE name = 'Responsiveness.Renderer.UserInteraction'
GROUP BY interval_id;

SELECT * FROM interaction_latency_metric;
