INCLUDE PERFETTO MODULE ext.loadline2_stages;

SELECT
  page,
  (end_request - story_start) / 1e6 AS network,
  (renderer_ready - story_start) / 1e6 AS process_launch,
  (visual_mark - MAX(renderer_ready, end_request)) / 1e6 AS renderer_visual,
  (interactive_mark - MAX(renderer_ready, end_request)) / 1e6 AS renderer_interactive,
  (visual_presentation - visual_mark) / 1e6 AS gpu_visual,
  (interactive_presentation - interactive_mark) / 1e6 AS gpu_interactive
FROM loadline2_stages;
