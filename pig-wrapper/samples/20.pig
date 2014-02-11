register 'stxjava.jar';
register 'stxeval.jar';

DEFINE TaskParamsParse org.TaskParamsParse();
DEFINE JobCategory org.JobCategory();
DEFINE Cube2String org.Cube2String();

A = load 'tsk-log' using TSKVStorage as (m:map[]);
B = filter A by m#'task_action_id' eq 'mapreduce/xjob';
C = foreach B generate
    TaskParamsParse(m#'task_params') as m:map[],
    (m#'failure' is null or SIZE((chararray)m#'failure') == 0L ? 0 : 1) as is_failure:int,
    (double)m#'time_taken_sec' as time_taken_sec:double;

D = foreach C generate
    m#'job' as job_name,
    JobCategory(m#'job', m#'is_dynamic') as job_category,
    m#'mrbasename' as mrbasename,
    m#'scale' as job_scale,
    SUBSTRING(m#'date', 6, 16) as job_date, -- iso-date
    (is_failure == 1 ? 0 : CEIL(time_taken_sec)) as taken_sec_success:int,
    (is_failure == 1 ? CEIL(time_taken_sec) : 0) as taken_sec_failure:int;
E = cube D by cube(mrbasename, job_name, job_category, 
        job_date, job_scale);
F = foreach E generate Cube2String(group), 
    SUM(cube.taken_sec_success) as taken_sec_success:int, 
    SUM(cube.taken_sec_failure) as taken_sec_failure:int;

store F into 'jobresults' using TSVStorage;
explain;

