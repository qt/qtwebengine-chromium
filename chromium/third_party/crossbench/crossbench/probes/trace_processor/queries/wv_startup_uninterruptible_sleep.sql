 WITH
        target_slice AS (
            SELECT
              s.ts AS slice_ts,
              s.dur AS slice_dur,
              t.utid AS slice_utid
            FROM
              slice s
            JOIN
              thread_track tt
              ON s.track_id = tt.id
            JOIN
              thread t
              ON tt.utid = t.utid
            WHERE
              s.name LIKE '%WebViewChromiumAwInit.startChromiumLockedSync%'
            LIMIT 1  -- In case multiple, take the first (can remove if you want all)
          ),
          thread_state_breakdown AS (
            SELECT
              tsb.state,
              SUM(tsb.dur) AS total_dur_ns
            FROM
              thread_state tsb
            JOIN
              target_slice tslice
              ON
                tsb.utid = tslice.slice_utid
                AND tsb.ts < (tslice.slice_ts + tslice.slice_dur)
                AND (tsb.ts + tsb.dur) > tslice.slice_ts
            GROUP BY
              tsb.state
          )
        SELECT (total_dur_ns / 1000000.0) AS D_thread_state_startCL_dur_ms FROM thread_state_breakdown WHERE state = 'D';
