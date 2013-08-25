#include "MyIsamLib.h"

#include <stdexcept>

#include <boost/filesystem/operations.hpp>

namespace fs = boost::filesystem;

namespace myisamlib {

MyIsamLib::MyIsamLib(const std::string &basedir) {
  tmpdirname_ = fs::unique_path(fs::temp_directory_path().native() 
          + "/%%%%%%").native();
  fs::create_directories(tmpdirname_ + "/support_files/datadir/test");
  fs::create_symlink(basedir + "/support_files/datadir/mysql", 
    tmpdirname_ + "/support_files/datadir/mysql");
  fs::create_symlink(basedir + "/support_files/share", 
    tmpdirname_ + "/support_files/share");
  fs::create_symlink(basedir + "/support_files/plugin", 
    tmpdirname_ + "/support_files/plugin");

  dst_basedir = tmpdirname_ + "/support_files";
  dst_datadir = tmpdirname_ + "/support_files/datadir";
  dst_language = tmpdirname_ + "/support_files/share/mysql/english";
  dst_plugindir = tmpdirname_ + "/support_files/plugin";

  mysqlconfig_ = new MysqlConfig();
  mysqlconfig_->basedir= dst_basedir.c_str();
  mysqlconfig_->datadir= dst_datadir.c_str();
  mysqlconfig_->language= dst_language.c_str();
  mysqlconfig_->plugin_dir= dst_plugindir.c_str();

  if (mysqldlib_initialize_library(mysqlconfig_))
    throw std::runtime_error("initialization error");
}

MyIsamLib::MyIsamLib(struct MysqlConfig *mysqlconfig) 
  : mysqlconfig_(mysqlconfig)
{
  if (mysqldlib_initialize_library(mysqlconfig))
    throw std::runtime_error("initialization error");
}

MyIsamLib::~MyIsamLib() {
  mysqldlib_shutdown_library();
  if (!tmpdirname_.empty()) {
    delete mysqlconfig_;
    fs::remove_all(fs::path(tmpdirname_));
  }
}

}
