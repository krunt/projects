
A = load 'tsk-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'task_action_id' as action_id, 
                       m#'executor_host' as host, 
                       m#'executor_pid' as pid, 
                       m#'executor_start_time' as start_time;
C = group B by (action_id, host, pid, start_time) PARALLEL 5;
D = foreach C generate flatten(group);

store D into 'jobresults' using TSKVStorage('\t', 'id,host,pid,time');
explain;

