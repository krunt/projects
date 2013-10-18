#include "Query.h"

#include <vector>

#include <string.h>
#include <assert.h>

#include <stdio.h>

#include <iostream>

/* #define DEBUG */

namespace myisamlib {

struct QueryParseContext {
    QueryUnion root_union;

    std::vector<QueryUnion> stu;
    std::vector<Query> stq;
    std::vector<QueryUnion> stf;
    std::vector<bool> stf_bool;
};

static void mysql_union_list_begin(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

#ifdef DEBUG
    fprintf(stderr, "<mysql_union_list_begin>\n");
#endif

    ctx->stu.push_back(QueryUnion());
    ctx->stq.clear();
}

static void mysql_union_list_end(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

#ifdef DEBUG
    fprintf(stderr, "<mysql_union_list_end>\n");
#endif

    assert(!ctx->stu.empty());
    ctx->root_union = ctx->stu.back();
    ctx->stu.pop_back();
}

static void mysql_union_list_item_begin(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

#ifdef DEBUG
    fprintf(stderr, "<mysql_union_list_item_begin>\n");
#endif

    ctx->stq.push_back(Query());
}

static void mysql_union_list_item_end(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

#ifdef DEBUG
    fprintf(stderr, "<mysql_union_list_item_end>\n");
#endif

    assert(!ctx->stu.empty());
    ctx->stu.back().query_union_list_.push_back(ctx->stq.back());

    assert(!ctx->stq.empty());
    ctx->stq.pop_back();
}

static void mysql_item_list_begin(void *arg) {}
static void mysql_item_list_add(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    ctx->stq.back().item_list_.push_back(std::string(str, length));
}
static void mysql_item_list_end(void *arg) {}
static void mysql_where_list_item(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    ctx->stq.back().where_list_.push_back(std::string(str, length));
}

static void mysql_group_list_item(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    ctx->stq.back().group_list_.push_back(std::string(str, length));
}

static void mysql_having_list_item(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    ctx->stq.back().having_list_.push_back(std::string(str, length));
}

static void mysql_order_list_item(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    ctx->stq.back().order_list_.push_back(std::string(str, length));
}

static void mysql_limit_str(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    ctx->stq.back().limit_list_.push_back(std::string(str, length));
}

static void mysql_from_list_item_begin(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);
    ctx->stf.push_back(QueryUnion());
    ctx->stf_bool.push_back(false);
}

static void mysql_from_list_on_dual(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    if (!ctx->stf.empty()) {
        ctx->stq.back().from_list_.push_back(
            Query::QueryUnionOrTable(Query::QueryUnionOrTable::TABLE, "dual"));
        ctx->stf_bool.back() = true;
    }
}

static void mysql_from_list_table_name(void *arg, const char *str, int length) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    assert(!ctx->stf.empty());
    ctx->stq.back().from_list_.push_back(
        Query::QueryUnionOrTable(Query::QueryUnionOrTable::TABLE, 
            std::string(str, length)));
    ctx->stf_bool.back() = true;
}

static void mysql_from_list_item_end(void *arg) {
    QueryParseContext *ctx = static_cast<QueryParseContext *>(arg);

    assert(!ctx->stq.empty());
    if (!ctx->stf_bool.back()) {
        ctx->stq.back().from_list_.push_back(Query::QueryUnionOrTable(
            Query::QueryUnionOrTable(Query::QueryUnionOrTable::QUERY_UNION,
               ctx->stf.back())));
    }
    ctx->stf.pop_back();
    ctx->stf_bool.pop_back();
}


QueryUnion ConstructQueryObject(const char *query) {
    mysqldlib::query_parse_callbacks cbs;
    memset(&cbs, 0, sizeof(cbs));

    cbs.union_list_begin = &mysql_union_list_begin;
    cbs.union_list_end = &mysql_union_list_end;

    cbs.union_list_item_begin = &mysql_union_list_item_begin;
    cbs.union_list_item_end = &mysql_union_list_item_end;

    cbs.item_list_begin = &mysql_item_list_begin;
    cbs.item_list_add = &mysql_item_list_add;
    cbs.item_list_end = &mysql_item_list_end;

    cbs.from_list_item_begin = &mysql_from_list_item_begin;
    cbs.from_list_on_dual = &mysql_from_list_on_dual;
    cbs.from_list_table_name = &mysql_from_list_table_name;
    cbs.from_list_item_end = &mysql_from_list_item_end;

    cbs.where_list_item = &mysql_where_list_item;
    cbs.group_list_item = &mysql_group_list_item;
    cbs.having_list_item = &mysql_having_list_item;
    cbs.order_list_item = &mysql_order_list_item;
    cbs.limit_str = &mysql_limit_str;

    QueryParseContext parse_context;
    mysqldlib::MysqlQueryImpl query_impl(cbs, &parse_context);
    if (query_impl.init(query)) {
        throw std::runtime_error("table open error (maybe not found)");
    }
    query_impl.parse_with_callbacks();
    return parse_context.root_union;
}

std::string QueryUnion::to_string(int indent) const {
    std::string result;
    for (int i = 0; i < query_union_list_.size(); ++i) {
        if (i) { 
            result += std::string(indent, ' ') + "UNION\n";
        }
        result += query_union_list_[i].to_string(indent);
    }
    return result;
}

std::string Query::join_strings(const std::string &label,
        const std::vector<std::string> &lst) const
{
    std::string result = label; 
    for (int i = 0; i < lst.size(); ++i) {
        if (i) { result += ", "; }
        result += lst[i];
    }
    return result;
}

std::string Query::to_string(int indent) const {
    std::string result;

    result = std::string(indent, ' ') + "SELECT ";

    /* item-list */
    for (int i = 0; i < item_list_.size(); ++i) {
        if (i) { result += ", "; }
        result += item_list_[i];
    }

    result += "\n";

    result += std::string(indent, ' ') + "FROM\n";

    /* from-list */
    for (int i = 0; i < from_list_.size(); ++i) {
        const QueryUnionOrTable &m = from_list_[i];

        result += std::string(indent, ' ');

        if (m.type_ == Query::QueryUnionOrTable::TABLE) {
            result += m.table_ + "\n";
        } else {
            result += "(" + m.query_.to_string() 
                + std::string(indent, ' ') + ")\n";
        }
    }

    if (!where_list_.empty()) {
        result += join_strings(std::string(indent, ' ') + "WHERE "
            + std::string(indent, ' '), where_list_) + "\n";
    }

    if (!group_list_.empty()) {
        result += join_strings(std::string(indent, ' ') + "GROUP BY\n"
                + std::string(indent, ' '), group_list_) + "\n";
    }

    if (!having_list_.empty()) {
        result += join_strings(std::string(indent, ' ') + "HAVING\n" 
            + std::string(indent, ' '), having_list_) + "\n";
    }

    if (!order_list_.empty()) {
        result += join_strings(std::string(indent, ' ') + "ORDER BY\n"
             + std::string(indent, ' '), order_list_) + "\n";
    }

    if (!limit_list_.empty()) {
        result += join_strings(std::string(indent, ' ') + "LIMIT\n"
             + std::string(indent, ' '), limit_list_) + "\n";
    }

    return result;
}

}

