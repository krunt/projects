#ifndef _QUORUM_PEER_
#define _QUORUM_PEER_

#include "ConnectionFactory.h"
#include "Config.h"
#include "Database.h"

struct QPacket {
    int type;
    int zxid;
};

struct LearnerInfo {
    int server_id;
    int protocol_version;
};

class Vote {
public:
    Vote(int id = 0, uint64_t zxid = 0, int epoch = 0) 
        : id_(id), zxid_(zxid), epoch_(epoch)
    {}

    void set_id(int id) { id_ = id; }
    void set_zxid(uint64_t zxid) { zxid_ = zxid; }
    void set_epoch(int epoch) { epoch_ = epoch; }

    int id() const { return id_; }
    uint64_t zxid() const { return zxid_; }
    int epoch() const { return epoch_; }

    bool operator<(const Vote &rhs) const {
        return (id_ < rhs.id_ || (!(rhs.id_ < id_) && zxid_ < rhs.zxid_));
    }

    const std::string to_string() const {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%d %d %d", id_, zxid_, epoch_);
        return std::string(buf);
    }

private:
    int id_;
    uint64_t zxid_;
    int epoch_;
};

struct ElectionResult {
    ElectionResult()
        : count(0), winning_count(0), num_valid_votes(0)
    {}

    Vote vote;
    Vote winner;
    int count;
    int winning_count;
    int num_valid_votes;
};

class QuorumPeer {
public:
    enum { LOOKING, FOLLOWING, LEADING };

    QuorumPeer(const char *config_path)
        : conf_(config_path), 
          connection_factory_(create_factory(conf_)),
          state_(LOOKING), running_(false),
          accepted_epoch_((uint32_t)-1)
    {}

    void operator()();
        
    void start();
    void start_leader_election();

    void look_for_leader();

    int state() const {
        return state_;
    }

    const Vote &current_vote() const {
        return vote_;
    }

    const Config &conf() const {
        return conf_;
    }

    const Database &db() const {
        return db_;
    }

    const ConnectionFactory &connection_factory() const {
        return *connection_factory_;
    }

    uint32_t get_accepted_epoch() const {
        return accepted_epoch_;
    }

    void set_accepted_apoch(uint32_t epoch) {
        accepted_epoch_ = epoch;
    }

    uint32_t get_current_epoch() const {
        return db().get_current_epoch();
    }

    uint32_t myid() const {
        return conf_.myid();
    }

    bool is_running() const {
        return running_;
    }

private:
    Config conf_;
    ConnectionFactory *connection_factory_;
    Vote vote_;
    Database db_;
    int state_;
    bool running_;
    uint32_t accepted_epoch_;
};

#endif
