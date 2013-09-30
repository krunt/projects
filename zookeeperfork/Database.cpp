#include "Database.h"

void Database::load() {
    snap_log_.restore(data_tree_);
}
