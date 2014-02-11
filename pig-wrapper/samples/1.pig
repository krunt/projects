A = LOAD 'testdata' USING PigStorage('\t') AS (k, v);
B = FOREACH A generate UPPER(v) AS (v);
; dump B;
