#ifndef _DATABASE_
#define _DATABASE_

#include "DataTree.h"

class Database {
public:
    Database()
    {}

    void load() 
    {}

    uint64_t get_last_logged_zxid() const {
        return data_tree_.get_last_processed_zxid();
    }

    int32_t get_current_epoch() const {
        return 0;
    }

private:
    FileTxnSnapLog snap_log_;
    DataTree data_tree_;
};

#endif
