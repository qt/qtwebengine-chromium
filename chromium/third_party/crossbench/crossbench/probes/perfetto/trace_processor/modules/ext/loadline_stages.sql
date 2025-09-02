INCLUDE PERFETTO MODULE ext.loadline_benchmark;

DROP VIEW IF EXISTS loadline_presentation;
CREATE VIEW loadline_presentation AS
SELECT
  first_navigation_start() + 60e9 / loadline_benchmark_score() AS presentation;

DROP VIEW IF EXISTS loadline_request;
CREATE VIEW loadline_request AS
SELECT ts AS start_request, ts + dur AS end_request
FROM slice
WHERE
  name = 'WillStartRequest'
  AND ts >= first_navigation_start()
ORDER BY ts
LIMIT 1;

DROP VIEW IF EXISTS loadline_renderer_ready;
CREATE VIEW loadline_renderer_ready AS
SELECT ts + dur AS renderer_ready
FROM slice
WHERE
  name = 'ReadyToCommitNavigation'
  AND ts >= first_navigation_start()
ORDER BY ts
LIMIT 1;

-- Find the frame in the pipeline which was chosen as the "loading complete"
-- moment for the purpose of LoadLine score. The exact end timestamp might
-- differ a little due to rounding error, so we allow 1ms discrepancy while
-- matching. This should not match any extra frames since frames are aligned to
-- vsyncs, and vsync interval is usually 8-17ms.
DROP VIEW IF EXISTS loadline_frame;
CREATE VIEW loadline_frame AS
SELECT id
FROM slice, loadline_presentation
WHERE
  name = 'PipelineReporter'
  AND ts + dur BETWEEN presentation - 1e6 AND presentation + 1e6
  AND extract_arg(arg_set_id, 'chrome_frame_reporter.state') = 'STATE_PRESENTED_ALL'
ORDER BY ts
LIMIT 1;

DROP VIEW IF EXISTS loadline_frame_commit;
CREATE VIEW loadline_frame_commit AS
SELECT child.ts + child.dur AS frame_commit
FROM loadline_frame, descendant_slice(loadline_frame.id) AS child
WHERE child.name = 'Commit';

DROP VIEW IF EXISTS loadline_submit_compositor_frame;
CREATE VIEW loadline_submit_compositor_frame AS
SELECT child.ts AS submit_compositor_frame
FROM loadline_frame, descendant_slice(loadline_frame.id) AS child
WHERE child.name = 'SubmitCompositorFrameToPresentationCompositorFrame';

DROP VIEW IF EXISTS loadline_frame_swap;
CREATE VIEW loadline_frame_swap AS
SELECT child.ts + child.dur AS frame_swap
FROM loadline_frame, descendant_slice(loadline_frame.id) AS child
WHERE child.name = 'StartDrawToSwapStart';

DROP VIEW IF EXISTS loadline_stages;
CREATE VIEW loadline_stages AS
SELECT
  first_navigation_start() AS navigation_start,
  start_request,
  end_request,
  renderer_ready,
  frame_commit,
  submit_compositor_frame,
  frame_swap,
  presentation
FROM loadline_presentation, loadline_request, loadline_renderer_ready,
     loadline_frame_commit, loadline_submit_compositor_frame, loadline_frame_swap;

