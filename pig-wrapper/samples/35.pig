
A = load 'tsk-log' using TSKVStorage as (m:map[]);
B = foreach A generate m#'task_action_id' as action_id, 
                       m#'executor_host' as host, 
                       m#'executor_pid' as pid, 
                       m#'executor_start_time' as start_time;
split B 
    into C1 if SUBSTRING(start_time, 0, 10) eq '2014-01-01',
         C2 if SUBSTRING(start_time, 0, 10) eq '2014-01-02';

D = join C1 by (action_id, host, pid), C2 by (action_id, host, pid);
E = foreach D generate C1::action_id as action_id, 
    C1::host as host, C1::pid as pid, 1 as cnt:double;
F = group E by (action_id, host, pid);
G = foreach F generate flatten(group), SUM($1.cnt) as cnt;

store G into 'jobresults' using TSKVStorage('\t', 'action_id,host,pid,cnt');
explain;

