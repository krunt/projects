A = load 'acc' as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost:chararray,x_forwarded_for,x_real_remote_ip);
B = foreach A generate TOMAP(vhost, ip) as (mp:map[]);
describe B;

C = foreach B generate $0#'yandex.ru';
describe C;

store C into 'd5' using PigStorage('\t');
explain;
