
#include "ConnectionFactory.h"
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "Logger.h"

namespace asio = boost::asio;

ConnectionFactory::~ConnectionFactory() {
    if (--*ref_count_ == 0) {
        ConnectionsType::iterator it = connections_.begin();
        while (it != connections_.end()) {
            delete it->second;
            it->second = NULL;
            it++;
        }
        delete acceptor_;
        delete io_service_;
        delete ref_count_;
    }
}

void ConnectionFactory::operator()() {
    mlog(LOG_INFO, "ConnectionFactory thread started");
    start_accept();
    io_service_->run();
}

void ConnectionFactory::start_accept() {
    asio::ip::tcp::socket *tcp_socket = new asio::ip::tcp::socket(*io_service_);
    acceptor_->async_accept(
            *tcp_socket,
            boost::bind(&ConnectionFactory::handle_accept, this, tcp_socket));   
}

void ConnectionFactory::handle_accept(ip::tcp::socket *client_socket) {
    Connection *connection = create_connection(client_socket);
    mlog(LOG_INFO, "Got connection from %s:%d", 
            connection->socket()->remote_endpoint().address().to_string().c_str(),
            connection->socket()->remote_endpoint().port());
    connections_[connection->socket()->remote_endpoint()] = connection;
    connection->handle_read();
    start_accept();
}

ConnectionFactory *create_factory(const Config &config) {
    return new ConnectionFactory(asio::ip::tcp::v4(), config.client_port());
}

Connection *create_connection(ip::tcp::socket *client_socket) {
    return new Connection(client_socket);
}

