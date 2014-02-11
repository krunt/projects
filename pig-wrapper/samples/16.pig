register 'stxjava.jar';

A = load 'rawaccess' using TSVStorage as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost,x_forwarded_for,x_real_remote_ip);
B = foreach A generate ip, method;
C1 = filter B by method eq 'GET';
C2 = filter B by method eq 'POST';

store C1 into 'd5' using PigStrStorage('\t');
store C2 into 'd6' using TSVStorage('\t');

explain;
