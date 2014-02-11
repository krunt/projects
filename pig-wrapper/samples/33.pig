
A = load 'tsk-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'task_action_id' as action_id, 
                       m#'executor_host' as host, 
                       m#'executor_pid' as pid, 
                       m#'executor_start_time' as start_time;
C = group B by (action_id, host, pid, start_time);
D = foreach C generate flatten(group) as (action_id, host, pid, start_time), 
                    1 as uni, COUNT($1) as cnt;
E = group D by (action_id, host, pid);
F = foreach E generate flatten(group), SUM($1.uni) as uni, SUM($1.cnt) as hits;

store F into 'jobresults' using TSKVStorage('\t', 'id,host,pid,uni,hits');
explain;

