A = load 'watch-log' as (browser_info,chunk_id,counter_class,counter_id,ip_numeric,params,referer,region_id,request_entity,sess_id,timestamp,url,user_agent,watch_id,yandexuid);
B = load 'a-log' as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost,x_forwarded_for,x_real_remote_ip);
C = foreach B generate method, ip;
D = filter C by method eq 'GET';
E = order D by ip asc;
store D into 'd5' using PigStorage('\t');
store E into 'd6' using PigStorage('\t');
explain;
