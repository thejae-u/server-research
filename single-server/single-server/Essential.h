#ifndef ESSENTIAL_H
#define ESSENTIAL_H

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <iostream>

using io_context = boost::asio::io_context;
using tcp = boost::asio::ip::tcp;
using socket_sptr = std::shared_ptr<tcp::socket>;

#include "PacketHeader.h"

#endif // ESSENTIAL_H
