register 'stxjava.jar';
register 'stxeval.jar';

A = load 'a-log.6p' using TSKVStorage('\t','vhost=yandex.ru') as (m:map[]);
B = load 'blockstat-log.6p' using TSKVStorage('\t','vhost=yandex.ru') as (m:map[]);
C = foreach A generate m#'ip' as ip, SIZE((chararray)m#'method') as mlength;
D = foreach B generate m#'ip' as ip, SIZE((chararray)m#'blocks') as blength;
E = join C by ip, D by ip;
F = foreach E generate ip, mlength, blength;

store E into 'd7' using TSKVStorage('\t','ip,mlen,blen');
explain;

