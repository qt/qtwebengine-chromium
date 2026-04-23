-- Copyright 2025 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.
INCLUDE PERFETTO MODULE ext.speedometer;
INCLUDE PERFETTO MODULE ext.speedometer_scheduling;

SELECT * FROM ext_benchmark_scheduling_thread_cpu_time;
