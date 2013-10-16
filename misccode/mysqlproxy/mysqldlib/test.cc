#include <mysqldlib.h>
#include <MyIsamLib.h>
#include <TableIterator.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

int main(int argc, const char **argv) {
    if (argc != 2) {
        return 1;
    }

    myisamlib::MyIsamLib library;
    myisamlib::TableIterator it(library, argv[1]);
    mysqldlib::MysqlQueryImpl q;

    //if (q.init("SELECT s FROM (SELECT m FROM Z) M")) {
    if (q.init("select s.a from ("
        "SELECT COUNT(*) a, SUM(v) FROM ("
        "SELECT v FROM smile) m JOIN smile k ON (k.v=m.v) "
        "where m.v > 2 GROUP BY v) s where s.a * 2 > 3")) {
        return 1;
    }

    const char *str = q.to_string();
    printf("%s\n", str);

    exit(0);
}
