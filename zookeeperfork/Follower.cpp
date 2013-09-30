#include "Follower.h"
#include "Packet.h"
#include "Leader.h"
#include "Utils.h"

void Follower::follow_leader() {
    const ServerHostPort leader_address = find_leader();
    connect_to_leader(leader_address);
    uint64_t new_zxid = register_with_leader();
    sync_with_leader(new_zxid);

    Packet p;
    while (peer_.is_running()) {
        socket_.read(p);
        process_packet(p);
    }
}

const ServerHostPort Follower::find_leader() const {
    const Vote &vote = peer_.current_vote();
    const Config::peers_storage_type &peers = peer_.conf().get_peers();
    for (int i = 0; i < peers.size(); ++i) {
        if (peers[i].first == vote.id()) {
            return peers[i].second;
        }
    }
    assert(0);
}

void Follower::connect_to_leader(const ServerHostPort &host_port) {
    if (!socket_.connected()) {
        socket_.connect(host_port);
    }
}

uint64_t Follower::register_with_leader() {
    const uint32_t default_version = 0x1;
    uint64_t last_logged_zxid = peer_.db().get_last_logged_zxid();
    Packet p;
    p << (uint32_t)Leader::FOLLOWERINFO << last_logged_zxid 
      << peer_.conf().myid() << default_version;

    socket_.write(p);
    socket_.read(p);

    uint32_t packet_type, new_epoch;
    uint64_t leader_zxid;
    p >> packet_type >> leader_zxid;
    assert(packet_type == Leader::LEADERINFO);
    new_epoch = epoch_from_zxid(leader_zxid);

    Packet ack;
    ack << (uint32_t)Leader::ACKEPOCH << last_logged_zxid;
    if (new_epoch > peer_.get_accepted_epoch()) {
        ack << peer_.get_current_epoch();
    } else if (new_epoch == peer_.get_accepted_epoch()) {
        ack << (uint32_t)-1;
    } else {
        assert(0);
    }
    socket_.write(ack);

    return make_zxid(new_epoch, 0);
}

void Follower::sync_with_leader(uint64_t zxid) {

}

void Follower::process_packet(const Packet &p) {
    sleep(100);    
}
