#ifndef _FOLLOWER_
#define _FOLLOWER_

#include "QuorumPeer.h"
#include "TCPSocket.h"

class Follower {
public:
    Follower(QuorumPeer &peer)
        : peer_(peer)
    {}

    void follow_leader();

private:
    const ServerHostPort find_leader() const;
    void connect_to_leader(const ServerHostPort &);
    uint64_t register_with_leader();
    void sync_with_leader(uint64_t);
    void process_packet(const Packet &p);

    QuorumPeer &peer_;
    TCPSocket socket_;
};

#endif
