A = load 'a-log' as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost,x_forwarded_for,x_real_remote_ip);
B = foreach A generate ip, method, timestamp;
C = group B by method;
D = foreach C {
    E = order B by ip desc;
    generate group, COUNT(E);
};

store D into 'd5' using PigStorage('\t');
explain;
