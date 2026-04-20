INCLUDE PERFETTO MODULE ext.loadline2_stages;

SELECT
  page || '_visual' AS metric,
  60e9 / (visual_presentation - story_start) AS value
FROM loadline2_stages
UNION ALL
SELECT
  page || '_interactive' AS metric,
  60e9 / (interactive_presentation - story_start) AS value
FROM loadline2_stages;
