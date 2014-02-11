register 'stxjava.jar';
register 'stxeval.jar';

DEFINE ParseUrl org.ParseUrl();

A = load 'tskv-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'tskv_format' as format, m#'vhost' as vhost;
C = group B by format;

store C into 'jobresults' using TSKVStorage('\t','tskv_format,group');
explain;

