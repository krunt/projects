#ifndef TABLE_ITERATOR_DEF
#define TABLE_ITERATOR_DEF

extern "C" {
#include "myisamreader.h"
}

#include "Table.h"
#include "MyIsamLib.h"

#include <string>
#include <vector>

#include <stdio.h>

namespace myisamlib {

class TableIterator {
public:
  TableIterator(const MyIsamLib &isamlib, const std::string &filename);
  TableIterator(const MyIsamLib &isamlib, const std::string &dbname, 
          const std::string &tablename);
  ~TableIterator();

  bool next();

  void set_unpack_flag_for_field(const std::string &fieldname);
  void clear_unpack_flags();

  const Table &get_table() const {
    return table_;
  }

  Table::FieldsMapType &get_fields() {
    return table_.get_fields();
  }

  std::vector<std::string> get_fieldnames();

private:
  class IsamTableCopier {
  public:
    IsamTableCopier();
    IsamTableCopier(const std::string &basedir, const std::string &filename);
  };

private:
  IsamTableCopier copier_;
  void *iter_;
  Table table_;
  bool at_end_;
};


}


#endif /* TABLE_ITERATOR_DEF */
