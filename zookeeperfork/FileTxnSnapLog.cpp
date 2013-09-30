
void FileTxnSnapLog::restore(DataTree &data_tree) {
    FileTxnLog log(data_dir_);
    FileTxnIterator it = read(data_tree.get_last_processed_zxid()+1);
    while (true) {
        if (!it.next())
            return;
        process_transaction(data_tree, it.header(), it.record());
    }
}

void process_transaction(DataTree &data_tree, 
        const TxnHeader &txn_header, const std::string &record) 
{ data_tree.process_txn(txn_header, record); }
