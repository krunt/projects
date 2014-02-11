define hd `./ls` ship('/bin/ls');
A = load 'watch-log' as (browser_info,chunk_id,counter_class,counter_id,ip_numeric,params,referer,region_id,request_entity,sess_id,timestamp,url,user_agent,watch_id,yandexuid);
B = foreach A generate chunk_id, counter_class, ip_numeric, yandexuid;
C = group B by counter_class;
D = foreach C {
    E = order B by yandexuid desc;
    F = limit E 3;
    generate group, flatten(F);
};
G = stream D through hd as (yandexuid);
store G into 'watch-results' using PigStorage('\t');
store D into 'watch-results101' using PigStorage('\t');
store C into 'watch-results102' using PigStorage('\t');
explain;
-- dump B;
