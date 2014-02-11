register 'stxjava.jar';
register 'stxeval.jar';

DEFINE HttpParseParams org.HttpUtilsParseRequestParams();
DEFINE RegionByIp org.GeobaseRegionByIp('Geobasev6.bin');

-- dicts:
-- Geobasev6.bin

A = load 'a-log.6p' using TSKVStorage('\t','method=POST') as (m:map[]);
B = foreach A generate m#'ip' as ip, HttpParseParams(m#'request') as params:map[];
C = filter B by params is not null and params#'clid' is not null;
D = foreach C generate RegionByIp(ip) as region, params#'clid' as clid;
E = foreach D generate region.$1, region.$4, clid;

store E into 'd7' using TSKVStorage('\t','shortname,id,clid');
explain;
