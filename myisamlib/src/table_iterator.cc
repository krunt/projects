#include "TableIterator.h"

#include <boost/filesystem/operations.hpp>

#include <string>
#include <vector>

namespace fs = boost::filesystem;

namespace myisamlib {

  TableIterator::TableIterator(const MyIsamLib &isamlib, const std::string &filename) 
      : copier_(isamlib.tmpdirname(), filename), 
        table_("test", fs::path(filename).filename().native()), at_end_(false)
  {
    if (!(iter_= myisam_get_iterator(filename.c_str(), table_.get_record()))) {
      throw std::runtime_error("iterator-creation failure");
    }
    /* default - select all fields */
    table_.set_unpack_flag_for_field("*"); 
  }

  TableIterator::TableIterator(const MyIsamLib &isamlib, const std::string &dbname, 
          const std::string &tablename)
    : copier_(), table_(dbname, tablename), at_end_(false)
  {
    char filename[4096];
    snprintf(filename, sizeof(filename), 
        "%s/%s/%s", isamlib.mysqlconfig()->datadir, dbname.c_str(), tablename.c_str());
    if (!(iter_= myisam_get_iterator(filename, table_.get_record()))) {
      throw std::runtime_error("iterator-creation failure");
    }
    /* default - select all fields */
    table_.set_unpack_flag_for_field("*"); 
  }

  TableIterator::~TableIterator() {
    if (myisam_close_iterator(iter_))
      throw std::runtime_error("close iterator error");
  }

  bool TableIterator::next() {
    int ret;
    if (at_end_ || !(ret= myisam_next_iterator(iter_))) {
      at_end_= true;
      return false;
    }
    if (ret == -1) { 
      throw std::runtime_error("get_next iterator failure");
    }
    table_.unpack_fields();
    return true;
  }

  void TableIterator::set_unpack_flag_for_field(const std::string &fieldname) {
    table_.set_unpack_flag_for_field(fieldname);
  }

  void TableIterator::clear_unpack_flags() {
    table_.clear_unpack_flags();
  }

  std::vector<std::string> TableIterator::get_fieldnames() {
    std::vector<std::string> result;
    Table::FieldsMapType &fields_map = get_fields();
    Table::FieldsMapType::const_iterator fit = fields_map.begin();
    while (fit != fields_map.end()) { 
      result.push_back((*fit).first);
      fit++;
    }
    return result;
  }

  TableIterator::IsamTableCopier::IsamTableCopier() {
  }

  TableIterator::IsamTableCopier::IsamTableCopier(
    const std::string &basedir, const std::string &filename) 
  {
    const std::string tabledir = basedir + "/support_files/datadir/test";
    const std::string table_basename = fs::path(filename).filename().native();

    fs::create_directories(tabledir);

    std::vector<std::string> possible_exts;
    possible_exts.push_back(".MYD");
    possible_exts.push_back(".MYI");
    possible_exts.push_back(".FRM");
    possible_exts.push_back(".frm");
    possible_exts.push_back(".myi");
    possible_exts.push_back(".myd");

    for (int i = 0; i < possible_exts.size(); ++i) {
      if (fs::is_regular_file(filename + possible_exts[i])) {
        fs::create_symlink(filename + possible_exts[i],
          tabledir + "/" + table_basename + possible_exts[i]);
      }
    }
  }
}
