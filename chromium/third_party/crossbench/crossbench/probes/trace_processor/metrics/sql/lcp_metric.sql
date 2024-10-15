DROP VIEW IF EXISTS lcp_metric_output;

CREATE PERFETTO VIEW lcp_metric_output AS
SELECT LCPMetric(
  'lcp_ms', (
      SELECT dur / 1e6
      FROM slice
      WHERE name = 'PageLoadMetrics.NavigationToLargestContentfulPaint'
      ORDER BY ts DESC
      LIMIT 1
  )
);
