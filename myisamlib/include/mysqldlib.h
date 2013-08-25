#ifndef MYSQLDLIB_DEF
#define MYSQLDLIB_DEF

class MysqlTableFieldsInternalRepr;
class THD;
struct TABLE_LIST;
struct st_table;

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

}


extern "C" {
/* 0 - success, 1 - failure */
int mysqldlib_initialize_library(struct MysqlConfig *config);
void mysqldlib_shutdown_library();
}

#endif /* MYSQLDLIB_DEF */
