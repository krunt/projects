#ifndef QUERY_DEF
#define QUERY_DEF

#include "mysqldlib.h"

#include <map>
#include <string>
#include <stdexcept>
#include <vector>

namespace myisamlib {

struct Query {
    struct QueryUnionOrTable;

    std::string to_string(int indent = 0) const;
    std::string join_strings(const std::string &label,
        const std::vector<std::string> &lst) const;

    std::vector<std::string> item_list_;
    std::vector<QueryUnionOrTable> from_list_;
    std::vector<std::string> where_list_;
    std::vector<std::string> group_list_;
    std::vector<std::string> having_list_;
    std::vector<std::string> order_list_;
    std::vector<std::string> limit_list_;
};

struct QueryUnion {
    std::string to_string(int indent = 0) const;

    std::vector<Query> query_union_list_;
};

struct Query::QueryUnionOrTable {
    enum { TABLE = 0, QUERY_UNION = 1 };

    QueryUnionOrTable (int type, const QueryUnion &q)
        : type_(type), query_(q)
    {}

    QueryUnionOrTable(int type, const std::string &table)
        : type_(type), table_(table)
    {}

    int type_;
    QueryUnion query_;
    std::string table_;
};

QueryUnion ConstructQueryObject(const char *query);

}

#endif /* QUERY_DEF */
