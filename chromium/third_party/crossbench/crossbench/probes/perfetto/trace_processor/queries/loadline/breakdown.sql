INCLUDE PERFETTO MODULE ext.loadline_stages;

-- Reports durations of loadline stages in milliseconds.
-- Stages approximately correspond to the Chrome subsystem which is most
-- important for the page loading performance.
-- For more info on page loading process in Chrome, see the following docs:
-- https://chromium.googlesource.com/chromium/src/+/main/docs/navigation.md
-- https://chromium.googlesource.com/chromium/src/+/main/docs/life_of_a_frame.md
-- https://chromium.googlesource.com/chromium/src/+/main/components/page_load_metrics/
SELECT
  (start_request - navigation_start) / 1e6 AS init,
  (end_request - start_request) / 1e6 AS network,
  (renderer_ready - end_request) / 1e6 AS launch,
  (frame_commit - renderer_ready) / 1e6 AS renderer,
  (submit_compositor_frame - frame_commit) / 1e6 AS compositor,
  (frame_swap - submit_compositor_frame) / 1e6 AS gpu,
  (presentation - frame_swap) / 1e6 AS surfaceflinger
FROM loadline_stages;
