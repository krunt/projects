#ifndef _DATATREE_
#define _DATATREE_

class DataTree {
public:
    enum {
        NOTIFICATION = 0,
        CREATE = 1,
        DELETE = 2,
        EXISTS = 3,
        GETDATA = 4,
        SETDATA = 5,
        GETACL = 6,
        SETACL = 7,
        GET_CHILDREN = 8,
        SYNC = 8,
        PING = 9,
        CHECK = 10
    };

public:
    DataTree()
    {}

    uint64_t get_last_processed_zxid() const {
        return last_zxid_;
    }

    void process_txn(const TxnHeader &txn_header, const std::string &record);

private:
    uint64_t last_zxid_;
};

#endif
