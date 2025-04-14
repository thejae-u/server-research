#include "DBConnectSession.h"

#include <utility>
#include "Server.h"
#include "ClientSession.h"

DBConnectSession::DBConnectSession(io_context& io, const std::shared_ptr<ClientSession>& clientSessionPtr, const std::shared_ptr<boost_ep>& dbEndPointPtr)
    : _io(io), _clientSessionPtr(clientSessionPtr), _dbEndPointPtr(dbEndPointPtr), _isConnectedToDb(false)
{
    _socketPtr = std::make_shared<boost_socket>(io); // empty socket

    std::cout << "DB EndPoint : " << _dbEndPointPtr->address() << "\n";
    std::cout << "DB Port : " << _dbEndPointPtr->port() << "\n";
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

    try
    {
        _socketPtr->connect(*_dbEndPointPtr);
        std::cout << std::this_thread::get_id() << " DBSession DB Server Connected\n";
    }
    catch (std::exception& e)
    {
        std::cerr << "DBConnectSession Connect Failed\n";
        std::cerr << e.what() << "\n";

        _clientSessionPtr->Stop();
    }
}

void DBConnectSession::Stop()
{
    // Remove from server
    const auto self(shared_from_this());
    
    // stop session
    if (_socketPtr->is_open())
        _socketPtr->close();

    _isConnectedToDb = false;
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