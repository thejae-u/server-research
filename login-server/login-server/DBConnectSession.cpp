#include "DBConnectSession.h"
#include "Server.h"
#include "ClientSession.h"

DBConnectSession::DBConnectSession(io_context& io, std::shared_ptr<ClientSession> clientSessionPtr, const std::string& dbIp, const unsigned short dbPort)
    : _io(io), _clientSessionPtr(std::move(clientSessionPtr))
{
    _socketPtr = std::make_shared<boost_socket>(io); // empty socket

    _dbIp = dbIp;
    _dbPort = dbPort;
}

DBConnectSession::~DBConnectSession()
{
    if (_socketPtr->is_open())
    {
        _socketPtr->close();
    }

    std::cout << "DBConnectSession Closed : " << _socketPtr->remote_endpoint().address() << "\n";
}

void DBConnectSession::Start()
{
    // Connect with DB Server
    const auto self(shared_from_this());

    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(_dbIp), _dbPort);
    
    _socketPtr->connect(ep);

    std::cout << "DBConnectSession Connected\n";
}

void DBConnectSession::Stop()
{
    // Remove from server
    const auto self(shared_from_this());
    _clientSessionPtr->Stop();
    
    // stop session
    _socketPtr->close();
}

void DBConnectSession::ProcessRequest(const n_data& req)
{
    auto self(shared_from_this());

    n_data replyData;

    // Send To DB Server and Receive Reply
    switch (req.type())
    {
    case NetworkData::LOGIN:
        break;
    case NetworkData::REGISTER:
        break;

    // Not Implemented Cases are handled here
    default:
        break;
    }

    // Reply to Client Session
    _clientSessionPtr->ReplyReq(replyData);
}