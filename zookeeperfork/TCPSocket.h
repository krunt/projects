#ifndef _TCP_SOCKET_
#define _TCP_SOCKET_

#include "Config.h"
#include "Packet.h"
#include <boost/asio.hpp>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

class TCPSocket {
public:
    TCPSocket()
        : socket_(io_service_)
    {}

    TCPSocket(const ip::tcp::socket &sock)
        : socket_(sock), connected_(sock.is_open())
    {}

    ~TCPSocket() {
    }

    bool connected() const {
        return connected_;
    }

    void connect(const ServerHostPort &host_port) {
        socket_.connect(ip::tcp::endpoint(host_port.address(), host_port.port()));
        connected_ = true;
    }

    void write(const Packet &packet) {
        asio::write(socket_, asio::buffer(packet.data(), packet.size()));
    }

    void read(Packet &packet) {
        const int max_packet_size = 1024;
        char buffer[max_packet_size]; 
        size_t read_length = asio::read(socket_, asio::buffer(buffer, max_packet_size));
        packet.reset();
        packet.append(buffer, read_length);
    }

private:
    asio::io_service io_service_;
    ip::tcp::socket socket_;
    bool connected_;
};

#endif
