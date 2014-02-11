register 'stxjava.jar';

A = load 'rawaccess1' using TSKVStorage as (m:map[]);
B = foreach A generate m#'ip' as ip, m#'method' as method;
C1 = filter B by method eq 'GET';
C2 = filter B by method eq 'POST';

store C1 into 'd5' using TSVStorage;
store C2 into 'd6' using TSVStorage;

explain;
