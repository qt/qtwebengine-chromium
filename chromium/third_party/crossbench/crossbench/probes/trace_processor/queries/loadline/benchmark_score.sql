-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
INCLUDE PERFETTO MODULE ext.loadline_benchmark;

SELECT loadline_benchmark_score() as score;
