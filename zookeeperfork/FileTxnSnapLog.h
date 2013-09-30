#ifndef _FILETXNSNAPLOG_
#define _FILETXNSNAPLOG_

class FileSnap {
public:
    FileSnap(const std::string &snap_dir) 
    {}
};

class FileTxnSnapLog {
public:
    FileTxnSnapLog(const std::string &data_dir, const std::string &snap_dir)
        : data_dir_(data_dir), snap_dir_(snap_dir),
          txnlog_(data_dir_), snaplog_(snap_dir_)
    {}

    void restore(DataTree &tree);
    
private:
    std::string data_dir_;
    std::string snap_dir_;

    FileTxnLog txnlog_;
    FileSnap snaplog_;
};

#endif
