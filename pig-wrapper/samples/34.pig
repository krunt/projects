
A = load 'tsk-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'task_action_id' as action_id, 
                       m#'executor_host' as host, 
                       m#'executor_pid' as pid, 
                       m#'executor_start_time' as start_time;
C = group B by (action_id, host, pid);
D = foreach C {
    E = order B by start_time;
    F = limit E 3;
    generate flatten(group), flatten(F);
}

store D into 'jobresults' using TSKVStorage('\t', 'id,host,pid,times');
explain;

