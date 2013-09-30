#include "Leader.h"

class LearnerHandler {
public:
    LearnerHandler(ip::tcp::socket *client_socket, Leader *leader)
        : socket_(client_socket), leader_(leader)
    {}

    ~LearnerHandler() {
        socket_->close();
        delete socket_;
    }

    void operator()() {
        TCPSocket sock(*socket_);
        Packet p;
        sock.read(p);

        uint32_t packet_type;
        uint64_t peer_zxid;
        p >> packet_type >> peer_zxid >> peer_id_ >> peer_server_version_;
        assert(packet_type == Leader::FOLLOWERINFO);

        uint32_t last_accepted_epoch = epoch_from_zxid(peer_zxid);
        uint32_t new_epoch = leader_->get_epoch_to_propose(peer_id_, last_accepted_epoch);

        p.reset();
        p << (uint32_t)Leader::LEADERINFO 
            << make_zxid(new_epoch, 0) << (int32_t)0x10000;
        sock.write(p);
        sock.read(p);

        uint32_t peer_current_epoch, peer_last_zxid;
        p >> packet_type >> peer_last_zxid >> peer_current_epoch;
        assert(packet_type == Leader::ACKEPOCH);
        wait_for_epoch_ack(peer_.myid(), peer_current_epoch, peer_last_zxid);

        while (1) {}
    }

private:
    ip::tcp::socket *socket_;

    uint32_t peer_id_;
    uint32_t peer_server_version_;

    Leader *leader_;
};

class LearnerAcceptor {
public:
    LearnerAcceptor(Leader *leader)
        : leader_(leader),
          io_service_(new asio::io_service()),
          acceptor_(new ip::tcp::acceptor(*io_service_, 
            ip::tcp::endpoint(leader_->conf().get_my_server().address(), 
                              leader_->conf().get_my_server().port()))),
          ref_count_(new int(1)),
          stop_(false)
    {}

    LearnerAcceptor(const LearnerAcceptor &rhs)
        : io_service_(rhs.io_service_), 
          acceptor_(rhs.acceptor_),
          ref_count_(rhs.ref_count_)
    { *ref_count_++; }

    ~LearnerAcceptor() {
        if (--*ref_count_ == 0) {
            delete acceptor_;
            delete io_service_;
            delete ref_count_;
        }
    }

    void operator ()() {
        while (!stop_) {
            asio::ip::tcp::socket *tcp_socket 
                = new asio::ip::tcp::socket(*io_service_);
            acceptor_->accept(*tcp_socket);
            LearnerHandler peer_handler(tcp_socket, leader_);
            boost::thread peer_handler_thread(peer_handler);
        }
    }

private:
    Learner *leader_;
    asio::io_service *io_service_;
    ip::tcp::acceptor *acceptor_;
    int *ref_count_;
    bool stop_;
};

void Learner::lead() {
    boost::thread learner_acceptor(this); 
    uint32_t epoch = get_epoch_to_propose(peer_.myid(), peer_.get_accepted_epoch());
    wait_for_epoch_ack(peer_.myid(), peer_.get_current_epoch(), 
            peer_.db().get_last_logged_zxid());
    while (1) {}
}

uint32_t Learner::get_epoch_to_propose(uint32_t id, uint32_t last_accepted_epoch) {
    boost::lock_guard<boost::mutex> guard(followers_lock_); 
    if (!waiting_for_new_epoch_)
        return epoch_;
    if (last_accepted_epoch >= epoch_) {
        epoch_ = last_accepted_epoch + 1;
    }
    connecting_followers_.insert(id);
    if (connecting_followers_.find(peer_.myid()) != connecting_followers_.end()
                && connecting_followers_.size() > peer_.conf().servers_size() / 2)
    {
        waiting_for_new_epoch_ = false;
        peer_.set_accepted_epoch(epoch_);
        followers_condition_.notify_all();
    } else {
        while (waiting_for_new_epoch_) {
            followers_condition_.wait(followers_lock_);
        }
    }
    return epoch_;
}

void Learner::wait_for_epoch_ack(uint32_t id, uint32_t epoch, uint64_t last_zxid) {
    if (election_finished_)
        return;
    if (epoch != (uint32_t)-1) {
        assert(epoch <= peer_.get_current_epoch());
        electing_followers_.insert(id);
    }
    if (electing_followers_.find(peer_.myid()) != electing_followers_.end()
            && electing_followers_.size() > peer_.conf().servers_size() / 2)
    {
        election_finished_ = true;
        electors_condition_.notify_all();
    } else {
        while (!election_finished_) {
            electors_condition_.wait(electors_lock_);
        }
    }
}
