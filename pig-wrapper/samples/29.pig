register 'stxjava.jar';
register 'stxeval.jar';

import 'pigmacro/skew.pig';

A = load 'a-log.6p' using TSKVStorage as (m:map[]);
B = foreach A generate 
    REGEX_EXTRACT(m#'source_uri', '^([^:]*)', 1) as proto,
        REGEX_EXTRACT(m#'source_uri', '@([^/]*)', 1) as vhost; 
C = skew_count(B, proto, vhost, 10000);
store C into 'jobresults' using TSKVStorage('\t', 'proto,vhost');
explain;

