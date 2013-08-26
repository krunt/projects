#!/usr/bin/python3

import os
import sys
import http.cookiejar
import urllib.request
import urllib.parse
import sqlite3
import time
import subprocess

LIMIT_STEP = 10000
RELAX_STEP = 100
BASE = "http://boxrec.com/prt.php?human_id=999999999"
RESERVED_WORDS = ("GENERAL", "IGNORE_SERVER_IDS", "MASTER_HEARTBEAT_PERIOD", "MAXVALUE", "RESIGNAL", "SIGNAL", "SLOW", "ACCESSIBLE", "ADD", "ALL", "ALTER", "ANALYZE", "AND", "AS", "ASC", "ASENSITIVE", "BEFORE", "BETWEEN", "BIGINT", "BINARY", "BLOB", "BOTH", "BY", "CALL", "CASCADE", "CASE", "CHANGE", "CHAR", "CHARACTER", "CHECK", "COLLATE", "COLUMN", "CONDITION", "CONSTRAINT", "CONTINUE", "CONVERT", "CREATE", "CROSS", "CURRENT_DATE", "CURRENT_TIME", "CURRENT_TIMESTAMP", "CURRENT_USER", "CURSOR", "DATABASE", "DATABASES", "DAY_HOUR", "DAY_MICROSECOND", "DAY_MINUTE", "DAY_SECOND", "DEC", "DECIMAL", "DECLARE", "DEFAULT", "DELAYED", "DELETE", "DESC", "DESCRIBE", "DETERMINISTIC", "DISTINCT", "DISTINCTROW", "DIV", "DOUBLE", "DROP", "DUAL", "EACH", "ELSE", "ELSEIF", "ENCLOSED", "ESCAPED", "EXISTS", "EXIT", "EXPLAIN", "FALSE", "FETCH", "FLOAT", "FLOAT4", "FLOAT8", "FOR", "FORCE", "FOREIGN", "FROM", "FULLTEXT", "GRANT", "GROUP", "HAVING", "HIGH_PRIORITY", "HOUR_MICROSECOND", "HOUR_MINUTE", "HOUR_SECOND", "IF", "IGNORE", "IN", "INDEX", "INFILE", "INNER", "INOUT", "INSENSITIVE", "INSERT", "INT", "INT1", "INT2", "INT3", "INT4", "INT8", "INTEGER", "INTERVAL", "INTO", "IS", "ITERATE", "JOIN", "KEY", "KEYS", "KILL", "LEADING", "LEAVE", "LEFT", "LIKE", "LIMIT", "LINEAR", "LINES", "LOAD", "LOCALTIME", "LOCALTIMESTAMP", "LOCK", "LONG", "LONGBLOB", "LONGTEXT", "LOOP", "LOW_PRIORITY", "MASTER_SSL_VERIFY_SERVER_CERT", "MATCH", "MAXVALUE", "MEDIUMBLOB", "MEDIUMINT", "MEDIUMTEXT", "MIDDLEINT", "MINUTE_MICROSECOND", "MINUTE_SECOND", "MOD", "MODIFIES", "NATURAL", "NOT", "NO_WRITE_TO_BINLOG", "NULL", "NUMERIC", "ON", "OPTIMIZE", "OPTION", "OPTIONALLY", "OR", "ORDER", "OUT", "OUTER", "OUTFILE", "PRECISION", "PRIMARY", "PROCEDURE", "PURGE", "RANGE", "READ", "READS", "READ_WRITE", "REAL", "REFERENCES", "REGEXP", "RELEASE", "RENAME", "REPEAT", "REPLACE", "REQUIRE", "RESIGNAL", "RESTRICT", "RETURN", "REVOKE", "RIGHT", "RLIKE", "SCHEMA", "SCHEMAS", "SECOND_MICROSECOND", "SELECT", "SENSITIVE", "SEPARATOR", "SET", "SHOW", "SIGNAL", "SMALLINT", "SPATIAL", "SPECIFIC", "SQL", "SQLEXCEPTION", "SQLSTATE", "SQLWARNING", "SQL_BIG_RESULT", "SQL_CALC_FOUND_ROWS", "SQL_SMALL_RESULT", "SSL", "STARTING", "STRAIGHT_JOIN", "TABLE", "TERMINATED", "THEN", "TINYBLOB", "TINYINT", "TINYTEXT", "TO", "TRAILING", "TRIGGER", "TRUE", "UNDO", "UNION", "UNIQUE", "UNLOCK", "UNSIGNED", "UPDATE", "USAGE", "USE", "USING", "UTC_DATE", "UTC_TIME", "UTC_TIMESTAMP", "VALUES", "VARBINARY", "VARCHAR", "VARCHARACTER", "VARYING", "WHEN", "WHERE", "WHILE", "WITH", "WRITE", "XOR", "YEAR_MONTH", "ZEROFILL")

def construct_query(columns, table, where_clause=None, limit_clause=None):
    assert(len(columns) >= 1)
    res = "SELECT CONCAT_WS('||', 'MINE>', " + ", ".join(map(lambda c: "CONVERT(IFNULL(" + c + ", '') USING utf8)", columns)) \
           + ", '<MINE') as x" + " FROM " + table
    if where_clause:
        res += " WHERE " + where_clause
    if limit_clause:
        res += " LIMIT " + limit_clause
    return res

def extract_data_from_html(columns, data, with_columns=None):
    res = []
    i = 0
    while 1:
        f = data.find("MINE>||", i)
        if f == -1:
            break
        t = data.find("||<MINE", f+1)
        if t == -1:
            break
        if with_columns:
            record = {}
            for item in zip(columns, data[f+7:t].split("||")):
                record[item[0]] = item[1]
            res.append(record) 
        else:
            record = []
            for item in data[f+7:t].split("||"):
                record.append(item)
            res.append(record)
        i = t + 1
    return res

def get_cookies(cj, ff_cookies):
    con = sqlite3.connect(ff_cookies)
    cur = con.cursor()
    cur.execute("SELECT host, path, isSecure, expiry, name, value from moz_cookies where host LIKE '%boxrec.com%'")
    for item in cur.fetchall():
        c = http.cookiejar.Cookie(0, item[4], item[5],
                None, False,
                item[0], item[0].startswith('.'), item[0].startswith('.'),
                item[1], False,
                item[2], 
                item[3], item[3] == "",
                None, None, {})
        cj.set_cookie(c)

def make_query(opener, query):
    query_to_make = " ) UNION (SELECT 1, 1, y.x, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, z.boxer_a_id, z.user, z.bout_id, z.division, z.show_id, z.boxer_b_id, z.referee, z.judge1, z.judge2, z.judge3 FROM ( %s ) y, ( SELECT * FROM bouts WHERE boxer_a_id = 7035 LIMIT 1,1 ) z) UNION (SELECT * FROM bouts WHERE boxer_a_id = 999999999 " % query
    #print(BASE + urllib.parse.quote(query_to_make))
    return opener.open(BASE + urllib.parse.quote(query_to_make)).read()

def get_result_from_query(opener, columns, table, where_clause=None, with_columns=None, limit_clause=None):
    tables_data = make_query(opener, construct_query(columns, table, where_clause, limit_clause))
    return extract_data_from_html(columns, tables_data.decode('utf8'), with_columns)

cookies_path = "/home/alexey/.mozilla/firefox/p2jcz2hj.default/cookies.sqlite"
cj = http.cookiejar.CookieJar()
get_cookies(cj, cookies_path)

opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))

def get_max_elements(tables, id_map):
    max_script_name = "./get_max.py"
    res = {}
    for table in tables:
        res[table] = int(subprocess.check_output([max_script_name, table, id_map[table]]))
    return res

def get_counts(tables, id_map, max_elems):
    res = {}
    for table in tables:
        cnt = get_result_from_query(opener, ["COUNT(*)"], table, where_clause = "%s > %d" % (id_map[table], max_elems[table]))
        res[table] = cnt[0]
    return res

def process_column_name(column_name):
    restricted = frozenset(RESERVED_WORDS)
    if column_name.upper() in restricted:
        return column_name + "_my"
    return column_name.replace('-', '_')

_ALWAYS_SAFE = frozenset(b'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
                         b'abcdefghijklmnopqrstuvwxyz'
                         b'0123456789'
                         b'_.-: @"!#$%&()*+,/;<=>?[]^~{}|')

def quote(item):
    if isinstance(item, str):
        item = item.encode('ascii', 'ignore')
    else:
        item = bytes([c for c in item if c < 128])
    return ''.join([chr(c) if c in _ALWAYS_SAFE else '%{:02X}' . format(c) for c in item])


def steal_data(tables, id_map, counts, max_elems):
    relax_cnt = 0
    for table in tables:
        columns = get_result_from_query(opener, ["COLUMN_NAME"], "INFORMATION_SCHEMA.COLUMNS", "TABLE_SCHEMA = 'v3' AND TABLE_NAME = '%s'" % table)
        column_names = list(map(lambda c: process_column_name(c[0]), columns))
        columns_string = ", " . join(column_names)
        i = 0; 
        n = counts[table]
        while i < int(n[0]):
            relax_cnt += 1
            if relax_cnt == RELAX_STEP:
                relax_cnt = 0
                time.sleep(5)
            table_data = get_result_from_query(opener, column_names, table, limit_clause = "%d,%d" % (i, LIMIT_STEP), where_clause = "%s > %d" % (id_map[table], max_elems[table]))
            sys.stderr.write("%20s: %d/%d\n" % (table, i, int(n[0])))
            i += LIMIT_STEP 
            for data_item in table_data:
                values_string = ", " . join(map(lambda c: "'" + quote(c) + "'", data_item))
                subquery = "INSERT INTO " + table + " ( %s ) VALUES (%s);" % (columns_string, values_string)
                sys.stdout.write(subquery + "\n")

tables = ('bouts', 'shows')
id_map = { 'bouts': 'bout_id', 'shows': 'show_id' }
max_elems = get_max_elements(tables, id_map)
counts = get_counts(tables, id_map, max_elems)
steal_data(tables, id_map, counts, max_elems)
