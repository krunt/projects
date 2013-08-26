#include <stdio.h>
#include <mysql/mysql.h>

int main() {
    MYSQL mysql_connection, *sql;
    mysql_init(&mysql_connection);
    sql = mysql_real_connect(&mysql_connection, "localhost", 
            "lua", "pass", "lua", 3306, NULL, 0);
    if (!sql)
        return 1;
    if (mysql_query(sql, "SELECT COUNT(*) as cnt FROM lua_info"))
        return 2;
    MYSQL_RES *res = mysql_store_result(sql);
    if (!res)
        return 3;
    
    char **p;
    while ((p = mysql_fetch_row(res))) {
        printf("%s\n", p && p[0] ? p[0] : "<NONE>");
    }

    mysql_free_result(res);
    mysql_close(sql);
    return 0;
}
