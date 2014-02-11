register 'stxjava.jar';
register 'stxeval.jar';

import 'pigmacro/skew.pig';

A = load 'a-log.6p' using TSKVStorage as (m:map[]);
B = foreach A generate m#'ip' as ip, m#'method' as method, 1 as cnt;
B1 = filter B by method eq 'GET' or method eq 'POST' 
            or method eq 'HEAD' or method eq 'PUT';
B2 = skew_sum2(B1, ip, method, cnt, 10000);

store B2 into 'd7' using TSVStorage;
explain;
