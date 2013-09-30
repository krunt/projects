#include "QuorumPeer.h"
#include "Logger.h"

int main() {
    const char *config_name = "log";
    const char *default_config_path = "config";

    logger_init(config_name);

    mlog(LOG_INFO, "started");

    QuorumPeer peer(default_config_path);
    peer.start();

    return 0;
}
