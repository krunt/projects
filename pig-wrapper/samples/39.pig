
import 'pigmacro/skew.pig';
A = load 'a-log.6p' using TSKVStorage as (m:map[]);

B = filter A by m#'cookies' is not null and m#'vhost' matches '[\\w\\.]{1,20}';
C = foreach B generate m#'vhost' as vhost,
                       REGEX_EXTRACT(m#'cookies', 'yandexuid=(\\d+)', 1) as yandexuid,
                       m#'ip' as ip;

D1 = foreach C generate vhost, yandexuid;
D11 = filter D1 by yandexuid is not null;

D111 = skew_unihits(D11, vhost, yandexuid, uni, hits, 1000);

D2 = foreach C generate vhost, ip;
D22 = skew_unihits(D2, vhost, ip, uni, hits, 1000);

E1 = skew_sum1_2(D111, vhost, uni, hits, 1000);
E2 = skew_sum1_2(D22, vhost, uni, hits, 1000);

E3 = join E1 by vhost, E2 by vhost;
E4 = foreach E3 generate E1::vhost as vhost, E1::uni as uniq, E1::hits as hits, E2::uni as hosts;

store E4 into 'jobresults' using TSKVStorage('\t', 'vhost,uniq,hits,hosts');
explain;
