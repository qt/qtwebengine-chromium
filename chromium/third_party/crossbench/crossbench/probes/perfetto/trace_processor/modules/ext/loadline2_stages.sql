INCLUDE PERFETTO MODULE ext.first_presentation_time;
INCLUDE PERFETTO MODULE ext.loadline2_string_functions;

DROP TABLE IF EXISTS story_start;
CREATE PERFETTO TABLE story_start AS
SELECT
  page_name(name) AS page,
  ts AS story_start
FROM slice
WHERE is_loadline2_mark(name) AND mark_name(name) = 'start';

DROP TABLE IF EXISTS story_pid;
CREATE PERFETTO TABLE story_pid AS
SELECT
  page_name(name) AS page,
  pid
FROM thread_slice
WHERE is_loadline2_mark(name) AND mark_name(name) = 'finish';

DROP TABLE IF EXISTS story_start_with_pid;
CREATE PERFETTO TABLE story_start_with_pid AS
SELECT
  page,
  story_start,
  pid
FROM story_start JOIN story_pid USING (page);

DROP TABLE IF EXISTS end_request;
CREATE PERFETTO TABLE end_request AS
SELECT
  page,
  MIN(ts) AS end_request
FROM slice, story_start_with_pid
WHERE
  name = 'CommitSentToFirstSubresourceLoadStart'
  AND ts >= story_start
GROUP BY page;

DROP TABLE IF EXISTS renderer_ready;
CREATE PERFETTO TABLE renderer_ready AS
SELECT
  page,
  MIN(ts) AS renderer_ready
FROM thread_slice
JOIN story_start_with_pid USING (pid)
WHERE
  name = 'DocumentLoader::CommitNavigation'
  AND ts >= story_start
GROUP BY page;

DROP TABLE IF EXISTS visual_mark;
CREATE PERFETTO TABLE visual_mark AS
SELECT
  page_name(name) AS page,
  ts AS visual_mark,
  pid
FROM thread_slice
WHERE is_loadline2_mark(name) AND mark_name(name) = 'visual';

DROP TABLE IF EXISTS interactive_mark;
CREATE PERFETTO TABLE interactive_mark AS
SELECT
  page_name(name) AS page,
  ts AS interactive_mark,
  pid
FROM thread_slice
WHERE is_loadline2_mark(name) AND mark_name(name) = 'interactive';

DROP TABLE IF EXISTS visual_presentation;
CREATE PERFETTO TABLE visual_presentation AS
SELECT
  page,
  get_next_presentation_time_by_pid(visual_mark, pid) AS visual_presentation
FROM visual_mark;

DROP TABLE IF EXISTS interactive_presentation;
CREATE PERFETTO TABLE interactive_presentation AS
SELECT
  page,
  get_next_presentation_time_by_pid(interactive_mark, pid) AS interactive_presentation
FROM interactive_mark;

DROP TABLE IF EXISTS loadline2_stages;
CREATE PERFETTO TABLE loadline2_stages AS
SELECT
  page,
  story_start,
  end_request,
  renderer_ready,
  visual_mark,
  visual_presentation,
  interactive_mark,
  interactive_presentation
FROM story_start_with_pid
JOIN end_request USING (page)
JOIN renderer_ready USING (page)
JOIN visual_mark USING (page)
JOIN visual_presentation USING (page)
JOIN interactive_mark USING (page)
JOIN interactive_presentation USING (page);
