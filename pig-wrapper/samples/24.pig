register 'stxjava.jar';
register 'stxeval.jar';

DEFINE ParseUrl org.ParseUrl();
DEFINE UrlToGroupsByPage org.UrlToGroupsByPage('UrlToGroups.yaml');

-- dicts:
-- UrlToGroups.yaml

import 'pigmacro/skew.pig';

A = load 'a-log.6p' using TSKVStorage('\t','method=GET') as (m:map[]);
B = foreach A generate m#'ip' as ip, 
    flatten(ParseUrl(CONCAT(CONCAT('http://', m#'vhost'), m#'request'))) as (vhost, page);
B1 = filter B by vhost is not null and page is not null and ip is not null;
C = foreach B generate ip, vhost, 
    flatten(UrlToGroupsByPage(vhost, page)) as url_group, 1 as hits;
C1 = filter C by url_group is not null;
D = skew_sum3(C1, ip, vhost, url_group, hits, 10000);
E = foreach D generate vhost, url_group, 1 as uni, hits;
F = skew_sum2_2(E, vhost, url_group, uni, hits, 10000);

store F into 'd7' using TSKVStorage('\t','vhost,url_group,uni,hits');
explain;
