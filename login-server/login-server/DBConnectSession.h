#pragma once

#include <boost/asio.hpp>
#include <string>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_ec = boost::system::error_code;

using n_data = NetworkData::NetworkData;
using n_type = NetworkData::ENetworkType;

class ClientSession;

class DBConnectSession : std::enable_shared_from_this<DBConnectSession>
{
public:
    DBConnectSession(io_context& io, std::shared_ptr<ClientSession> clientSessionPtr, const std::string& dbIp, const unsigned short dbPort);
    ~DBConnectSession();

    void Start();
    void Stop();
    void ProcessRequest(const n_data& req);

    std::shared_ptr<boost_socket> GetSocketPtr() const { return _socketPtr; }

private:
    io_context& _io;
    std::shared_ptr<boost_socket> _socketPtr;
    std::shared_ptr<ClientSession> _clientSessionPtr;

    std::string _dbIp;
    unsigned short _dbPort;
};
