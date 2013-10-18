#ifndef MYSQLDLIB_DEF
#define MYSQLDLIB_DEF

class MysqlTableFieldsInternalRepr;

#ifndef MYSQLINTERNALS
class THD;
struct TABLE_LIST;
struct st_table;
struct SELECT_LEX;
struct SELECT_LEX_UNIT;
struct ORDER;
struct TABLE_LIST;

template<typename T>
struct List;
#endif /* MYSQLINTERNALS */

struct MysqlConfig {
    const char *basedir;
    const char *datadir;
    const char *language;
    const char *plugin_dir;
};

namespace mysqldlib {

struct MysqlFieldStr {
  void *ptr; 
  int length;
  int need_unpack;
};

class MysqlTableImpl {
public:
  MysqlTableImpl();
  ~MysqlTableImpl();

  /* 0 - success, 1 - failure */
  int init(const char *dbname, const char *tablename);
  void get_field_names(char **names);
  void unpack_fields();
  void *get_record() const;

  MysqlFieldStr *get_fields() const;
  int fields_count() const;

private:
  THD *thd_;
  TABLE_LIST *tables_;
  st_table *table_;

  MysqlTableFieldsInternalRepr *fields_repr_;
  MysqlFieldStr *fields_array_;
};

struct query_parse_callbacks {
    void (*union_list_begin)(void *arg);
    void (*union_list_end)(void *arg);

    void (*union_list_item_begin)(void *arg);
    void (*union_list_item_end)(void *arg);

    void (*item_list_begin)(void *arg);
    void (*item_list_add)(void *arg, const char *str, int length);
    void (*item_list_end)(void *arg);

    void (*from_list_begin)(void *arg);
    void (*from_list_on_dual)(void *arg);
    void (*from_list_item_begin)(void *arg);

    void (*from_list_table_name)(void *arg, const char *str, int length);

    void (*from_list_item_end)(void *arg);
    void (*from_list_end)(void *arg);

    void (*where_list_begin)(void *arg);
    void (*where_list_item)(void *arg, const char *str, int length);
    void (*where_list_end)(void *arg);

    void (*group_list_begin)(void *arg);
    void (*group_list_item)(void *arg, const char *str, int length);
    void (*group_list_end)(void *arg);

    void (*having_list_begin)(void *arg);
    void (*having_list_item)(void *arg, const char *str, int length);
    void (*having_list_end)(void *arg);

    void (*order_list_begin)(void *arg);
    void (*order_list_item)(void *arg, const char *str, int length);
    void (*order_list_end)(void *arg);

    void (*limit_begin)(void *arg);
    void (*limit_str)(void *arg, const char *str, int length);
    void (*limit_end)(void *arg);
};

class MysqlQueryImpl {
public:
    MysqlQueryImpl();
    MysqlQueryImpl(const query_parse_callbacks &cbs, void *callback_arg);
    ~MysqlQueryImpl();

    int init(const char *query);
    void parse_with_callbacks();
    const char *to_string();

private:
    void set_default_callbacks();
    void parse_unit_with_callbacks(SELECT_LEX_UNIT *unit);
    void parse_table_list_element_with_callbacks(SELECT_LEX *sl,
        TABLE_LIST *elem);
    void parse_join_list_with_callbacks(SELECT_LEX *sl, List<TABLE_LIST> *tables);
    void parse_limit_with_callbacks(SELECT_LEX *sl);
    void parse_order_with_callbacks(SELECT_LEX *sl, ORDER *order);
    void parse_lex_with_callbacks(SELECT_LEX *sl);

    THD *thd_;
    query_parse_callbacks cbs_;
    void *cbs_arg_;
};

}

extern "C" {
/* 0 - success, 1 - failure */
int mysqldlib_initialize_library(struct MysqlConfig *config);
void mysqldlib_shutdown_library();
}

#endif /* MYSQLDLIB_DEF */
