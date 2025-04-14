#pragma once

#include <boost/asio.hpp>
#include <string>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_ec = boost::system::error_code;
using boost_ep = boost::asio::ip::tcp::endpoint;

using n_data = NetworkData::NetworkData;
using n_type = NetworkData::ENetworkType;

class ClientSession;

class DBConnectSession : public std::enable_shared_from_this<DBConnectSession>
{
public:
    DBConnectSession(io_context& io, const std::shared_ptr<ClientSession>& clientSessionPtr, const std::shared_ptr<boost_ep>& dbEndPointPtr);
    ~DBConnectSession();

    void Start();
    void Stop();
    void ProcessRequest(const n_data& req);

    std::shared_ptr<boost_socket> GetSocketPtr() const { return _socketPtr; }
    bool IsConnected() const { return _socketPtr->is_open(); }

private:
    io_context& _io;
    std::shared_ptr<boost_socket> _socketPtr;
    std::shared_ptr<ClientSession> _clientSessionPtr;
    std::shared_ptr<boost_ep> _dbEndPointPtr;

    bool _isConnectedToDb;
};
