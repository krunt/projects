#include "mysqldlib.h"
#include "TableIterator.h"

#include <vector>
#include <string>
#include <string.h>

char row[1024*1024];
void print_mapfields_fast(myisamlib::Table::FieldsMapType &mp,
        const std::vector<std::string> &fieldnames) 
{
    int len = 1;
    row[0] = '\t';
    for (int i= 0; i < fieldnames.size(); ++i) {
      memcpy(&row[len], fieldnames[i].c_str(), fieldnames[i].length());
      len += fieldnames[i].length();
      row[len++] = '=';

      const mysqldlib::MysqlFieldStr *field = mp[fieldnames[i]];
      char *p= (char*)field->ptr;
      if (!p) {
        memcpy(&row[len], "NULL", 4); 
        len += 4;
      } else {
        memcpy(&row[len], p, field->length);
        len += field->length;
      }
      row[len++] = '\t';
    }
    row[len] = 0;
    printf("%s\n", row);
}



int main(int argc, const char **argv) {
    myisamlib::MyIsamLib library("/stuff/mysql-5.1.65/myisamlib");
    myisamlib::TableIterator it(library, 
            "/x");

    it.clear_unpack_flags();
    std::vector<std::string> fieldnames;
    fieldnames.push_back("HostID");
    fieldnames.push_back("ClientIP");
    fieldnames.push_back("LogID");

    for (int i = 0; i < fieldnames.size(); ++i) {
        it.set_unpack_flag_for_field(fieldnames[i]);
    }

    while (it.next()) {
      print_mapfields_fast(it.get_fields(), fieldnames);
    }
    return 0;
}
