#ifndef _CONNECTION_FACTORY_
#define _CONNECTION_FACTORY_

#include <boost/thread.hpp>
#include <boost/asio/ip/address.hpp>
#include <list>
#include "Connection.h"
#include "Config.h"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

class Connection;

class ConnectionFactory {
public:
    ConnectionFactory(asio::ip::address address, int port) 
        : listen_address_(address), port_(port),
          io_service_(new asio::io_service()),
          acceptor_(new ip::tcp::acceptor(*io_service_, 
                      ip::tcp::endpoint(listen_address_, port_))),
          ref_count_(new int(1))
    {}

    ConnectionFactory(const ConnectionFactory &rhs)
        : listen_address_(rhs.listen_address_), port_(rhs.port_),
          io_service_(rhs.io_service_), acceptor_(rhs.acceptor_),
          ref_count_(rhs.ref_count_)
    { *ref_count_++; }

    ~ConnectionFactory();

    void operator()();

    void start_accept();
    void handle_accept(ip::tcp::socket *);

    asio::ip::address listen_address() const {
        return listen_address_;
    }

private:
    asio::ip::address listen_address_;
    int port_;
    asio::io_service *io_service_;
    ip::tcp::acceptor *acceptor_;
    int *ref_count_;

    typedef std::map<ip::tcp::endpoint, Connection*> ConnectionsType;
    ConnectionsType connections_;
};

ConnectionFactory *create_factory(const Config &config);
Connection *create_connection(ip::tcp::socket *client_socket);

#endif
