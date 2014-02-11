A = load 'acc' as (cluster_no,cookies,ip,method,process_id,processing_microtime,processing_time,protocol,referer,req_id,request,size,status,timestamp,timezone,unixtime,unixtime_micro,user_agent,vhost,x_forwarded_for,x_real_remote_ip);
B = foreach A generate vhost, ip, method, timestamp, x_real_remote_ip;
C = filter B by x_real_remote_ip != '' and ip != '';
D = group C by vhost;
E = foreach D {
    F = foreach C generate ToDate(timestamp, 'dd/MMM/YYYY:HH:mm:ss') as dt;
    FF = foreach F generate ToUnixTime(dt) as unixtime;
    G = distinct FF;
    H = order G by unixtime desc;
    HH = limit H 5;
    generate group, flatten(HH);
};

store E into 'd5' using PigStorage('\t');
explain;
