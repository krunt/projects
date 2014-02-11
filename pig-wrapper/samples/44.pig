A = load 'page_views' as 
    (user:chararray, action:int, timespent:int, query_term:chararray, 
    ip_addr:long, timestamp:long, estimated_revenue:double, 
    page_info:map[], page_links:{(map[])});
B = foreach A generate user;
C = distinct B;
D = limit C 500;

store D into 'proto_power_users';
