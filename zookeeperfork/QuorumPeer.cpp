#include "QuorumPeer.h"
#include <boost/asio.hpp>
#include <set>
#include <map>
#include "Logger.h"
#include "Follower.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

void QuorumPeer::start() {
    boost::thread connection_factory_thread(*connection_factory_);
    boost::thread quorum_peer_thread(*this);
    quorum_peer_thread.join();
    connection_factory_thread.join();
}

void QuorumPeer::operator()() {
    running_ = true;

    start_leader_election();

    while (running_) {
        switch (state()) {
          case LOOKING:
            look_for_leader();            
            break;
          case FOLLOWING:
            {
                Follower f(*this);
                f.follow_leader();
            }
            break;
          case LEADING:
            {
                Leader leader(*this);
                leader.lead();
            }
            break;
          default:
            running_ = false;
        };
    }
}

ElectionResult count_votes(const std::map<ip::address, Vote> &votes, 
        const std::set<int> &heard_from);

void QuorumPeer::look_for_leader() {
    asio::io_service io_service;
    ip::udp::socket usocket(io_service, ip::udp::endpoint(ip::udp::v4(), 0));

    mlog(LOG_INFO, "Started leader-election");
    while (running_) {
        std::set<int> heard_from;
        std::map<ip::address, Vote> votes;
        Config::peers_storage_type voting_peers = conf_.get_voting_peers();
        char buf[128];
        for (int i = 0; i < voting_peers.size(); ++i) {
            int id = voting_peers[i].first;
            const ServerHostPort &hostport = voting_peers[i].second;
            sprintf(buf, "%d", 0);  //stub = 0
            usocket.connect(ip::udp::endpoint(hostport.address(), hostport.udp_port()));
            usocket.send(asio::buffer(buf, strlen(buf)));
            mlog(LOG_INFO, "Sent voting request to %s", hostport.to_udp_string().c_str());
            usocket.receive(asio::buffer(buf, sizeof(buf)));

            int remote_id, remote_vote_id; 
            uint64_t remote_vote_zxid;
            sscanf(buf, "%d %d %d", &remote_id, &remote_vote_id, &remote_vote_zxid);

            mlog(LOG_INFO, "Received vote (%s) from %s", 
                    Vote(remote_vote_id, remote_vote_zxid).to_string().c_str(),
                    hostport.to_udp_string().c_str());

            votes[hostport.address()] = Vote(remote_vote_id, remote_vote_zxid);
            heard_from.insert(remote_id);
        }
    
        ElectionResult res = count_votes(votes, heard_from);
        if (res.num_valid_votes == 0) {
            vote_ = Vote(conf_.myid(), db_.get_last_logged_zxid());
        } else {
            if (res.winner.id() >= 0) {
                vote_ = res.vote;
                mlog(LOG_INFO, "Chose vote %s", vote_.to_string().c_str());
                if (res.winning_count > (voting_peers.size() / 2)) {
                    vote_ = res.winner;
                    state_ = conf_.myid() == res.winner.id() ? LEADING : FOLLOWING;
                    mlog(LOG_INFO, "Finished election (vote: %s). Changing state to %d", 
                                vote_.to_string().c_str(),
                                state_ == LEADING ? "LEADING" : "FOLLOWING");
                    if (state_ == FOLLOWING) {
                        usleep(100 * 1000);
                    }
                    return;
                }
            }
        }
        sleep(1);
    }
}

ElectionResult count_votes(
    const std::map<ip::address, Vote> &votes, const std::set<int> &heard_from)
{
    ElectionResult result;
    result.vote = Vote(-1, -1);
    result.winner = Vote(-1, -1);

    std::map<uint64_t, uint64_t> max_zxids;
    std::map<ip::address, Vote> valid_votes;
    std::map<ip::address, Vote>::const_iterator it = votes.begin();
    while (it != votes.end()) {
        ip::address addr = (*it).first;
        Vote vote  = (*it).second;
        if (max_zxids.find(vote.id()) != max_zxids.end()) {
            max_zxids[vote.id()] = std::max(max_zxids[vote.id()], vote.zxid());
        } else {
            max_zxids[vote.id()] = vote.zxid();
        }
        if (heard_from.find(vote.id()) != heard_from.end()) {
            valid_votes[addr] = vote;
        }
        it++;
    }

    std::map<ip::address, Vote>::iterator it1 = valid_votes.begin();
    while (it1 != valid_votes.end()) {
        int id = (*it1).second.id();
        (*it1).second = Vote(id, max_zxids[id]);
        it1++;
    }

    result.num_valid_votes = valid_votes.size();
    std::map<Vote, int> count_table;

    std::map<ip::address, Vote>::iterator it2 = valid_votes.begin();
    result.count = 0;
    while (it2 != valid_votes.end()) {
        Vote vote = (*it2).second;
        int count = 0;
        if (count_table.find(vote) != count_table.end()) {
            count = count_table[vote]++;
        } else {
            count_table[vote] = 1;
        }
        if (vote.id() == result.vote.id()) {
            result.count++;
        } else if (vote.zxid() > result.vote.zxid() 
                    || (vote.zxid() == result.vote.zxid() 
                        && vote.id() > result.vote.id())) 
        {
            result.vote = vote;
            result.count = 1;
        }
        it2++;
    }

    result.winning_count = 0;
    std::map<Vote, int>::const_iterator it3 = count_table.begin();
    while (it3 != count_table.end()) {
        Vote vote = (*it3).first;
        int cnt = (*it3).second;
        if (cnt > result.winning_count) {
            result.winning_count = cnt;
            result.winner = vote;
        }
        it3++;
    }

    return result;
}

class ResponderThread {
public:
    ResponderThread(QuorumPeer *peer)
        : running_(false), peer_(peer)
    { udp_port_ = peer_->conf().get_my_server().udp_port(); }

    ResponderThread(const ResponderThread &rhs)
        : running_(rhs.running_), peer_(rhs.peer_)
    { udp_port_ = peer_->conf().get_my_server().udp_port(); }

    void operator()() {
        mlog(LOG_INFO, "Started Responder udp election thread");
        running_ = true;
        io_service_ = new asio::io_service();
        socket_ = new ip::udp::socket(*io_service_, 
                        ip::udp::endpoint(peer_->connection_factory().listen_address(), 
                            udp_port_));
        char buf[128];
        while (running_) {
            ip::udp::endpoint endpoint;
            socket_->receive_from(asio::buffer(buf, sizeof(buf)), endpoint);
            mlog(LOG_INFO, "Got udp-packet from %s:%d",
                    endpoint.address().to_string().c_str(), endpoint.port());
            switch (peer_->state()) {
                case QuorumPeer::LOOKING:
                    sprintf(buf, "%d %d %d", peer_->conf().myid(), 
                            peer_->current_vote().id(), peer_->current_vote().zxid());
                    mlog(LOG_INFO, "Sending my udp-vote (%s) packet to %s:%d", buf,
                        endpoint.address().to_string().c_str(), endpoint.port());
                    socket_->send_to(asio::buffer(buf, strlen(buf)), endpoint);
                    break;
                case QuorumPeer::FOLLOWING:
                case QuorumPeer::LEADING:
                default:
                    running_ = false;
            };
        }
        delete socket_;
        delete io_service_;
    }

private:
    bool running_;
    int udp_port_;
    QuorumPeer *peer_;
    asio::io_service *io_service_;
    ip::udp::socket *socket_;
};

void QuorumPeer::start_leader_election() {
    vote_ = Vote(conf_.myid(), db_.get_last_logged_zxid(), db_.get_current_epoch());
    boost::thread responder_of_election_information(ResponderThread(this));
}
