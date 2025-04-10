#include "DBConnectSession.h"
#include "Server.h"

DBConnectSession::DBConnectSession(io_context& io, const std::shared_ptr<ClientSession>& clientSessionPtr)
    : _io(io), _clientSessionPtr(clientSessionPtr)
{
    _socketPtr = std::make_shared<boost_socket>(io); // empty socket
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
    
    
    // Start Process Request
    boost::asio::post(_io, [this] { ProcessRequest(); });
}

void DBConnectSession::Stop()
{
    // Remove from server
    const auto self(shared_from_this());
    _clientSessionPtr->GetServerPtr()->RemoveSession(self);
    
    // stop session
    _socketPtr->close();
}

void DBConnectSession::ProcessRequest(const n_data& req)
{
    auto self(shared_from_this());
}