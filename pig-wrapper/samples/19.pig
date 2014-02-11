register 'stxjava.jar';

A = load 'rawaccess1' using TSKVStorage as (m:map[]);
B = foreach A generate m#'ip' as ip, m#'method' as method, m#'vhost' as vhost;
B1 = filter B by (method eq 'GET' or method eq 'POST' 
            or method eq 'HEAD' or method eq 'PUT') 
    and vhost is not null and vhost matches '[\\w\\.]{1,20}';
B2 = foreach B1 generate ip, method, CONCAT(CONCAT(vhost, ':'), 
        (chararray)ROUND(RANDOM() * 10000)) as vhost;
C = group B2 by (method, vhost);
D = foreach C {
    E = foreach B2 generate ip;
    F = distinct E;
    generate flatten(group) as (method, vhost), COUNT(F) as uniq, COUNT($1) as cnt;
};
G = foreach D generate method,
    flatten(STRSPLIT(vhost, ':', 2)) as (vhost, groupnum), uniq, cnt;
G1 = group G by (method, vhost); 
G2 = foreach G1 generate flatten(group), SUM(G.uniq) as sumuniq, 
    SUM(G.cnt) as sumcnt, MIN(G.cnt) as mincnt, MAX(G.cnt) as maxcnt,
    MIN(G.uniq) as minuniq, MAX(G.uniq) as maxuniq;

store G2 into 'd8' using TSVStorage;
explain;
