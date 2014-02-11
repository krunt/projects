A = load 'a-log' using TSVStorage('\t','188.72.22[23].(116|181)') as (ip, method);
B = foreach A generate ip, method;
C = group B by ip;
D = foreach C generate group, COUNT(B.method);

store D into 'd5' using TSKVStorage('\t','ip,cnt');
explain;
