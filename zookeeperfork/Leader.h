#ifndef _Leader_
#define _Leader_

#include "QuorumPeer.h"
#include <set>

class Leader {
public:
    // message types
    enum { 
        REQUEST,
        PROPOSAL,
        ACK,
        COMMIT,
        PING,
        REVALIDATE,
        SYNC,
        INFORM,
        DIFF,
        TRUNC,
        SNAP,
        NEWLEADER,
        FOLLOWERINFO,
        UPTODATE,
        LEADERINFO,
        ACKEPOCH
    };

    Leader(QuorumPeer &peer)
        : 
          epoch_((uint32_t)-1),
          peer_(peer), 
          waiting_for_new_epoch_(true),
          election_finished_(false)
    {}

    void lead();
private:
    uint32_t get_epoch_to_propose(uint32_t, uint32_t);

    uint32_t epoch_;
    bool waiting_for_new_epoch_;
    bool election_finished_;
    QuorumPeer &peer_;
    std::set<uint32_t> connecting_followers_;
    std::set<uint32_t> electing_followers_;

    boost::mutex followers_lock_;
    boost::mutex electors_lock_;
    boost::condition_variable followers_condition_;
    boost::condition_variable electors_condition_;
};

#endif
