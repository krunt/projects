#include "FileTxnLog.h"
#include <boost/filesystem.hpp>
#include <vector>

namespace fs = namespace boost::filesystem;

FileTxnIterator FileTxnLog::read(uint64_t zxid) {
    return FileTxnIterator(log_dir_, zxid);
}

FileTxnIterator::FileTxnIterator(const std::string &log_dir_, uint64_t zxid) 
    : fd_(0)
{
    fs::directory_iterator begin(log_dir_);
    fs::directory_iterator end;
    std::vector<std::string> files;
    while (begin != end) {
        if (fs::is_file(*begin)) {
            files.push_back(*begin);
        }
    }
    std::sort(files.begin(), files.end(), std::greater<std::string>());
    for (int i = 0; i < files.size(); ++i) {
        if (get_zxid_from_log(files[i]) > zxid) {
            stored_files_.push_back(files[i]);
        } else {
            stored_files_.push_back(files[i]);
            break;
        }
    }
    if (goto_next_log()) {
        while (next() && hdr_.zxid() < zxid) {
        }
    }
}

bool FileTxnIterator::next() {
    if (!fd_.is_open())
        return false;

    fd_.peek();
    if (!fd_.eof()) {
        uint32_t txnlen;
        fd_ >> txnlen >> hdr_.session_id 
            >> hdr_.cxid >> hdr_.zxid >> hdr_.time
            >> hdr_.type;
        size_ = txnlen - 4 - sizeof(TxnHeader);
        fd_.read(record_, size_);
        return true;
    } else {
        return goto_next_log();
    }
}
