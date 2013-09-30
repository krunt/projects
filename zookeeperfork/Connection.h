#ifndef _CONNECTION_
#define _CONNECTION_

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include "Logger.h"
#include "Utils.h"

namespace ip = boost::asio::ip;

class Connection {
public:
    enum { max_length_ = 1024 };

public:
    Connection(ip::tcp::socket *client_socket)
        : socket_(client_socket)
    {}

    ~Connection() {
        socket_->close();
        delete socket_;
    }

    void read(const boost::system::error_code& error, size_t bytes_transferred) {
        data_[bytes_transferred] = 0;
        if (bytes_transferred) {
            mlog(LOG_INFO, "got %d bytes from client (%s:%d) %s",
                bytes_transferred,
                socket_->remote_endpoint().address().to_string().c_str(),
                socket_->remote_endpoint().port(), 
                quote_string(data_).c_str());
        }
        if (error.value() == boost::system::errc::success 
            || error.value() == boost::system::errc::interrupted) 
        { handle_read(); }
    }

    void handle_read() {
        socket_->async_read_some(
            boost::asio::buffer(reinterpret_cast<char*>(data_), max_length_),
            boost::bind(&Connection::read, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    ip::tcp::socket *socket() {
        return socket_;
    }

    char *data() {
        return data_;
    }

    int max_length() const {
        return max_length_;
    }

private:
    ip::tcp::socket *socket_;
    char data_[max_length_];
};

#endif
