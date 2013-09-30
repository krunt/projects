#ifndef _FILETXNLOG_
#define _FILETXNLOG_

struct TxnHeader {
    uint64_t session_id;
    uint32_t cxid;
    uint64_t zxid;
    uint64_t time;
    uint32_t type;
};

struct Request {
    uint64_t session_id;
    uint32_t cxid;
    uint32_t type;

    TxnHeader header;
    std::string txn;
};

class FileTxnIterator {
public:
    FileTxnIterator(const std::string &log_dir_, uint64_t zxid);
    bool next();

    TxnHeader header() const {
        return hdr_;
    }

    const std::string record() const {
        return std::string(record_, size_);
    }

private:
    uint64_t get_zxid_from_log(const std::string &logname) const {
        uint32_t left, right;
        sscanf(logname.substr(4).c_str(), "%X%X", &left, &right);
        return (uint64_t)left << 32 + right;
    }

    bool goto_next_log() {
        if (stored_files_.size() > 0) {
            current_logfile_ = stored_files_.back();
            stored_files_.pop_back();
            if (fd_.is_open()) {
                fd_.close();
            }
            fd_.open(current_logfile_.c_str(), std::ios_base::in | std::ios_base::binary);
            return true;
        } else {
            current_logfile_ = "";
        }
        return false;
    }

    std::vector<std::string> stored_files_;
    std::string current_logfile_;

    std::fstream fd_;
    TxnHeader hdr_;

    int size_;
    char record_[1024];
};

class FileTxnLog {
public:
    FileTxnLog(const std::string &data_dir) 
        : log_dir_(data_dir)
    {}
    
    FileTxnIterator read(uint64_t);

private:
    std::string log_dir_;
};

#endif
