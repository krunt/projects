
A = load 'page_views' as (user:chararray, action:int, timespent:int, query_term:chararray, 
    ip_addr:long, timestamp:long, estimated_revenue:double, 
    page_info:map[], page_links:{(map[])});
B = foreach A generate user, action, timespent, query_term, ip_addr, timestamp, estimated_revenue, page_info, page_links,
user as user1, action as action1, timespent as timespent1, query_term as query_term1, ip_addr as ip_addr1, timestamp as timestamp1, estimated_revenue as estimated_revenue1, page_info as page_info1, page_links as page_links1,
user as user2, action as action2, timespent as timespent2, query_term as query_term2, ip_addr as ip_addr2, timestamp as timestamp2, estimated_revenue as estimated_revenue2, page_info as page_info2, page_links as page_links2;
store B into 'widegroupbydata';
