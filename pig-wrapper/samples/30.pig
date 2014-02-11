register 'stxjava.jar';
register 'stxeval.jar';

A = load 'test-a-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'method' as method, m#'ip' as ip:bytearray;
C = group B by method;
D = foreach C {
    E = foreach B generate ip as double_ip;
    generate group, flatten(E);
};
store D into 'jobresults' using TSKVStorage('\t', 'proto,vhost');
explain;

