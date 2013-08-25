#include "Table.h"

namespace myisamlib {

Table::Table(const std::string &dbname, const std::string &tablename)
  : impl_()
{
  if (impl_.init(dbname.c_str(), tablename.c_str())) {
    throw std::runtime_error("table open error (maybe not found)");
  }

  char *fieldnames_buf = new char[impl_.fields_count() * MAX_FIELDNAME_LENGTH];
  char **fields = new char *[impl_.fields_count()];
  for (int i= 0; i < impl_.fields_count(); ++i) {
    fields[i]= &fieldnames_buf[i * MAX_FIELDNAME_LENGTH];
  }

  impl_.get_field_names(fields);
  for (int i= 0; i < impl_.fields_count(); ++i) {
    fields_map_[fields[i]]= &impl_.get_fields()[i];
  }

  delete [] fields;
  delete [] fieldnames_buf;
}

void Table::set_unpack_flag_for_field(const std::string &fieldname) {
  if (fieldname == "*") {
    for (FieldsMapType::iterator it = fields_map_.begin(); 
      it != fields_map_.end(); ++it) 
    { (*it).second->need_unpack= 1; }
  } else {
    if (fields_map_.find(fieldname) == fields_map_.end()) { 
      throw std::runtime_error("fieldname " + fieldname + " was not found");
    }
    fields_map_[fieldname]->need_unpack= 1;
  }
}

void Table::clear_unpack_flags() {
  for (FieldsMapType::iterator it = fields_map_.begin(); 
      it != fields_map_.end(); ++it) 
  { (*it).second->need_unpack= 0; }
}

}
