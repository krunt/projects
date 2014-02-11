REGISTER datafu.jar;
REGISTER piggybank.jar;
 
DEFINE Sessionize  datafu.pig.sessions.Sessionize('10m');
DEFINE Median      datafu.pig.stats.Median();
DEFINE VAR         datafu.pig.stats.VAR();
DEFINE UnixToISO org.apache.pig.piggybank.evaluation.datetime.convert.UnixToISO;
 
pv = LOAD 'clicks.csv' USING PigStorage('\t') 
    AS (memberId:int, time:long, url:chararray);
 
pv = FOREACH pv
     -- Sessionize expects an ISO string
     GENERATE UnixToISO(time) as isoTime,
              time,
              memberId;
 
pv_sessionized = FOREACH (GROUP pv BY memberId) {
  ordered = ORDER pv BY isoTime;
  GENERATE FLATTEN(Sessionize(pv)) AS (isoTime, time, memberId, sessionId);
};

pv_sessionized = FOREACH pv_sessionized GENERATE sessionId, time;
 
-- compute length of each session in minutes
session_times = FOREACH (GROUP pv_sessionized BY sessionId)
                GENERATE group as sessionId,
                         (MAX(pv_sessionized.time)-MIN(pv_sessionized.time))
                            / 1000.0 / 60.0 as session_length;
 
-- compute stats on session length
session_stats = FOREACH (GROUP session_times ALL) {
  ordered = ORDER session_times BY session_length;
  GENERATE
    AVG(ordered.session_length) as avg_session,
    SQRT(VAR(ordered.session_length)) as std_dev_session,
    Median(ordered.session_length) as median_session;
};

store session_stats into 'd1' using PigStorage('\t');
 
-- DUMP session_stats
--(15.737532575757575,31.29552045993877,(2.848041666666667),(14.648516666666666,31.88788333333333,86.69525))
