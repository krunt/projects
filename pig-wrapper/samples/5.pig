A = load 'a-log' using TSVStorage as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost,x_forwarded_for,x_real_remote_ip);
B = foreach A generate ip, method;
C = filter B by method eq 'GET';

store C into 'd5' using PigStorage('\t');
explain;
