#pragma once

#include <boost/asio.hpp>
#include <string>

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_ec = boost::system::error_code;

class DBSession
{
public:

private:
    io_context& _io;
    boost_socket _socket;
    
    
};
