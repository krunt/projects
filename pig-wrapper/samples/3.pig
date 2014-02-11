define hd `./ls` ship('/bin/ls');
A = load 'watch-log' as (browser_info,chunk_id,counter_class,counter_id,ip_numeric,params,referer,region_id,request_entity,sess_id,timestamp,url,user_agent,watch_id,yandexuid);
B = foreach A generate chunk_id, counter_class;
C = foreach A generate chunk_id, yandexuid;
D = join B by chunk_id, C by chunk_id;
store C into 'watch-results112' using PigStorage('\t');
store D into 'watch-results113' using PigStorage('\t');
explain;
-- dump B;
