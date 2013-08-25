#ifndef TABLE_DEF
#define TABLE_DEF

#include "mysqldlib.h"

#include <map>
#include <string>
#include <stdexcept>

namespace myisamlib {

class Table {
private:
    /* this should definitely be sufficient */
    static const int MAX_FIELDNAME_LENGTH = 1024;
    
public:
  typedef std::map<std::string, mysqldlib::MysqlFieldStr*> FieldsMapType;

public:
  Table(const std::string &dbname, const std::string &tablename);

  void set_unpack_flag_for_field(const std::string &fieldname);
  void clear_unpack_flags();

  void *get_record() const {
    return impl_.get_record();
  }

  void unpack_fields() {
    impl_.unpack_fields();
  }

  FieldsMapType &get_fields() {
    return fields_map_;
  }

private:
  mysqldlib::MysqlTableImpl impl_;
  FieldsMapType fields_map_;
};

}

#endif /* TABLE_DEF */
