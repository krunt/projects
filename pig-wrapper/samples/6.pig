A = load 'a-log' as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost,x_forwarded_for,x_real_remote_ip);
B = foreach A generate ip, method;
C = group B by method;
D = foreach C {
    generate group, COUNT(B) as cnt;
};

store D into 'd5' using PigStorage('\t');
explain;
