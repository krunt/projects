register 'stxjava.jar';

A = load 'a-log' using TSKVStorage as (mp:map[]);
B = foreach A generate mp#'ip' as ip, mp#'method' as method;
C = filter B by method eq 'GET';

store C into 'd5' using PigStorage('\t');
explain;
