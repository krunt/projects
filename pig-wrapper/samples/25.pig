register 'stxjava.jar';
register 'stxeval.jar';

DEFINE ParseUrl org.ParseUrl();

import 'pigmacro/skew.pig';

-- top n-hits-implementation

A = load 'a-log.6p' using TSKVStorage as (m:map[]);
B = foreach A generate m#'ip' as ip, 
    flatten(ParseUrl(CONCAT(CONCAT('http://', m#'vhost'), m#'request'))) as (vhost, page);
B1 = filter B by vhost is not null and page is not null and ip is not null;
C = foreach B1 generate ip, page, vhost as url_group, 1 as hits;
C1 = filter C by url_group is not null;
D = skew_sum3(C1, ip, page, url_group, hits, 10000);
E = foreach D generate page, url_group, 1 as uni, hits;
F = skew_sum2_2(E, page, url_group, uni, hits, 10000);
G = skew_orderbylimit1_2(F, url_group, page, uni, hits, uni, 'desc', 100, 1000);

store G into 'd7' using TSKVStorage('\t','url_group,page,uni,hits');
explain;
