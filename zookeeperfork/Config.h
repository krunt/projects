#ifndef _CONFIG_
#define _CONFIG_

#include <vector>
#include <string>
#include <assert.h>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

class ServerHostPort {
public:
    ServerHostPort(const std::string &hostport) {
        assert(hostport.find(':') != std::string::npos);
        host_ = hostport.substr(0, hostport.find(':'));
        std::size_t pos = hostport.find(':') + 1;
        std::size_t pos1 = hostport.find(':', pos) + 1;
        std::string string_port = hostport.substr(pos, pos1 - pos - 1);
        std::string udp_string_port = hostport.substr(pos1);
        port_ = boost::lexical_cast<int>(string_port.c_str());
        udp_port_ = boost::lexical_cast<int>(udp_string_port.c_str());
    }

    boost::asio::ip::address address() const {
        return boost::asio::ip::address::from_string(host_);
    }
    std::string host() const {
        return host_;
    }
    int port() const {
        return port_;
    }
    int udp_port() const {
        return udp_port_;
    }
    const std::string to_tcp_string() const {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s:%d", host_.c_str(), port_);
        return std::string(buf);
    }
    const std::string to_udp_string() const {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s:%d", host_.c_str(), udp_port_);
        return std::string(buf);
    }
private:
    std::string host_;
    int port_;
    int udp_port_;
};

class Config {
public:
    Config() : myid_(-1), client_port_(-1)
    {}

    Config(const std::string &filename) 
        : myid_(-1), client_port_(-1)
    {
        initialize(filename);
    }

    const ServerHostPort get_my_server() const {
        assert(myid_ != -1);
        for (int i = 0; i < servers_.size(); ++i) {
            if (servers_[i].first == myid_) {
                return servers_[i].second;
            }
        }
        assert(0);
    }

    typedef std::vector<std::pair<int, ServerHostPort> > peers_storage_type;
    const peers_storage_type get_peers() const {
        return servers_;
    }

    const peers_storage_type
    get_voting_peers() const {
        peers_storage_type result;
        for (int i = 0; i < servers_.size(); ++i) {
            if (servers_[i].first != myid_) {
                result.push_back(servers_[i]);
            }
        }
        return result;
    }

    int servers_size() const {
        return servers_.size();
    }

    uint32_t myid() const {
        return (uint32_t)myid_;
    }

    int client_port() const {
        return client_port_;
    }

private:
    void initialize(const std::string &filename);

    std::string strip_string(const std::string &str) const {
        int b, e;
        for (b = 0; b < str.size() && str[b] == ' '; ++b);
        for (e = str.size() - 1; e > b && str[e] == ' '; --e);
        return str.substr(b, e - b + 1);
    }

public:
    std::vector<std::pair<int, ServerHostPort> > servers_;
    int myid_;
    int client_port_;
};

#endif
