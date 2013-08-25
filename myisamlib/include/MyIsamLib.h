#ifndef MYISAMLIB_DEF 
#define MYISAMLIB_DEF 

#include "mysqldlib.h"
#include <string>

namespace myisamlib {

class MyIsamLib {
public:
    MyIsamLib(const std::string &basedir="/usr/share/myisamlib");
    MyIsamLib(struct MysqlConfig *mysqlconfig);
    ~MyIsamLib();

    struct MysqlConfig *mysqlconfig() const { return mysqlconfig_; }
    std::string tmpdirname() const { return tmpdirname_; }

private:
    struct MysqlConfig *mysqlconfig_;
    std::string tmpdirname_;

    std::string dst_basedir;
    std::string dst_datadir;
    std::string dst_language;
    std::string dst_plugindir;
};

}

#endif /* MYISAMLIB_DEF */
