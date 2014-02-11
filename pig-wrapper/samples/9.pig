A = load 'acc' as (cluster_no,cookies,ip,method,process_id);
B = foreach A generate ip, method;
store B into 'd5' using PigStorage('\t');
explain;
