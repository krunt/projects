-- access
A = load 'acc1' as (ip,method,protocol,referer,request,
    timestamp,timezone,unixtime,vhost);

B = foreach A generate vhost, ToString(ToDate(timestamp, 'dd/MMM/YYYY:HH:mm:ss'), 'YYYY-MM-dd') as dt, ip;
D = group B by (vhost, dt);

E = foreach D {
    ips = distinct B.ip;
    generate flatten(group), COUNT(ips) as uniq, COUNT($1) as hits;
};

H = group E by dt;
results1 = foreach H generate group as dt, SUM(E.uniq) as uniq, SUM(E.hits) as hits;

-- bar
A = load 'bar1' as (ip_numeric,unixtime,yandexuid);

-- hack here
B = foreach A generate yandexuid as vhost, ToString(ToDate(unixtime * 1000), 'YYYY-MM-dd') as dt, ip_numeric;
D = group B by (vhost, dt);

E = foreach D {
    ips = distinct B.ip_numeric;
    generate flatten(group), COUNT(ips) as uniq, COUNT($1) as hits;
};

H = group E by dt;
results2 = foreach H generate group as dt, SUM(E.uniq) as uniq, SUM(E.hits) as hits;

-- watch
A = load 'watch1' as (ip_numeric,referer,region_id,timestamp,yandexuid);

C = foreach A generate region_id, SUBSTRING(timestamp, 0, 10) as dt, ip_numeric;
D = group C by (region_id, dt);

E = foreach D {
    ips = distinct C.ip_numeric;
    generate flatten(group), COUNT(ips) as uniq, COUNT($1) as hits;
};

H = group E by dt;
results3 = foreach H generate group as dt, SUM(E.uniq) as uniq, SUM(E.hits) as hits;

-- mail
A = load 'mail1' as (ans_count,db_num,login,processing_time,query,suid,unixtime);
C = foreach A generate login, ToString(ToDate(unixtime * 1000), 'YYYY-MM-dd') as dt;
D = group C by (login, dt);

E = foreach D {
    logins = distinct C.login;
    generate flatten(group), COUNT(logins) as uniq, COUNT($1) as hits;
};

H = group E by dt;
results4 = foreach H generate group as dt, SUM(E.uniq) as uniq, SUM(E.hits) as hits;

-- join
results_pre = join results1 by dt, results2 by dt, results3 by dt, results4 by dt;
results = foreach results_pre generate
    results1::uniq + results2::uniq + results3::uniq + results4::uniq as uniq,
    results1::hits + results2::hits + results3::hits + results4::hits as hits;

store results1 into 'd1' using PigStorage('\t');
store results2 into 'd2' using PigStorage('\t');
store results3 into 'd3' using PigStorage('\t');
store results4 into 'd4' using PigStorage('\t');
store results  into 'd5' using PigStorage('\t');
