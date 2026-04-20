-- Functions to extract information from LoadLine2 performance mark labels
-- that have the following form: LoadLine2/<page_name>/<mark_name>
-- For example,
-- page_name('LoadLine2/amazon_product/interactive') = 'amazon_product'
-- mark_name('LoadLine2/amazon_product/interactive') = 'interactive'

-- TODO(khokhlov): Replace these with REGEXP_EXTRACT once it's available
-- in stable trace_processor.
CREATE OR REPLACE PERFETTO FUNCTION head(str STRING)
RETURNS STRING
AS
SELECT
  CASE INSTR($str, '/')
  WHEN 0 THEN $str
  ELSE SUBSTR($str, 1, INSTR($str, '/') - 1)
  END;

CREATE OR REPLACE PERFETTO FUNCTION tail(str STRING)
RETURNS STRING
AS
SELECT
  CASE INSTR($str, '/')
  WHEN 0 THEN ''
  ELSE SUBSTR($str, INSTR($str, '/') + 1, LENGTH($str))
  END;

CREATE OR REPLACE PERFETTO FUNCTION is_loadline2_mark(label STRING)
RETURNS INT
AS
SELECT head($label) = 'LoadLine2';

CREATE OR REPLACE PERFETTO FUNCTION page_name(label STRING)
RETURNS STRING
AS
SELECT head(tail($label));

CREATE OR REPLACE PERFETTO FUNCTION mark_name(label STRING)
RETURNS STRING
AS
SELECT head(tail(tail($label)));
