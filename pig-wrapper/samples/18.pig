register 'stxjava.jar';

A = load 'rawaccess1' using TSKVStorage as (m:map[]);
B = foreach A generate m#'ip' as ip, m#'method' as method;
B1 = filter B by method eq 'GET' or method eq 'POST' 
            or method eq 'HEAD' or method eq 'PUT';
C = group B1 by method;
D = foreach C {
    E = foreach B1 generate ip; 
    F = distinct E;
    generate group as method, COUNT(F) as uniq, COUNT($1) as hits;
};

store D into 'd7' using TSVStorage;
explain;
