A = load 'acc111' as (ip, vhost);

B = filter A by vhost != '' and ip != '';
C = group B by vhost;
D = foreach C {
    E = foreach B generate ip;
    EE = distinct E;
    generate group as vhost, COUNT(EE), COUNT(B);
};

store D into 'd5' using PigStorage('\t');
explain;
