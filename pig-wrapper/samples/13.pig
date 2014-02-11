A = load 'acc111' as (ip,method,protocol,referer,request,timestamp,timezone,unixtime,vhost);

B = foreach A generate vhost, method;
split B into B1 if method == 'GET',
             B2 if method == 'HEAD',
             B3 if method == 'POST';

C1 = foreach B1 generate '1' as class, vhost as vhost1, method;
C2 = foreach B2 generate '2' as class, vhost, method;
C3 = foreach B3 generate '3' as class, vhost, REPLACE(method, 'POST', 'GET') as method;

D = join C1 by method, C3 by method; 
describe D;

store D into 'd5' using PigStorage('\t');
store C1 into 'd6' using PigStorage('\t');
store C2 into 'd7' using PigStorage('\t');
store C3 into 'd8' using PigStorage('\t');
explain;
