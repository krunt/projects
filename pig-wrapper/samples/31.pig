
A = load 'tsk-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'task_action_id' as action_id, 
                       m#'executor_host' as host, 
                       m#'executor_pid' as pid, 
                       m#'executor_start_time' as start_time;
C = distinct B;

store C into 'jobresults' using TSKVStorage('\t', 'id,host,pid,time');
explain;

