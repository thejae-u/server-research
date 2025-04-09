#include "Session.h"
#include "Server.h"

Session::Session(io_context& io, const std::shared_ptr<Server>& serverPtr, const std::size_t sessionId)
    : _io(io), _serverPtr(serverPtr), _sessionId(sessionId)
{
    _socketPtr = std::make_shared<boost_socket>(io); // empty socket
}

Session::~Session()
{
    if (_socketPtr->is_open())
    {
        _socketPtr->close();
    }
}

void Session::Start()
{
    // Start Process Request
    boost::asio::post(_io, [this] { ProcessReq(); });
    
    // Start Receiving Request
    boost::asio::post(_io, [this] { ReceiveReq(); });
}

void Session::ProcessReq()
{
    
}

void Session::ReceiveReq()
{
    // Receive from client(Login, Logic Server Connection)
    // Add Request to Server
    auto self = shared_from_this();
    std::string recvData;
    
    _socketPtr->async_read_some(boost::asio::buffer(recvData), [this, self, recvData] // Buffer Size first
        (const boost_ec& ec, const std::size_t length)
    {
        if (!ec && length == sizeof(std::size_t))
        {
            n_data reqSize;
            reqSize.ParseFromString(recvData);

            // Receive Data
            _socketPtr->async_read_some(boost::asio::buffer(*dataBuffer), [this, self, dataBuffer](const boost_ec& ec, const std::size_t length)
            {
                if (!ec && length == dataBuffer->size())
                {
                    n_data req;
                    if (req.ParseFromString())
                    {
                        // Send Request to Server
                        return;
                    }

                    // Error in Parsing
                    std::cerr << "Error in Parsing Data\n";
                }
            });
        }
    });
}

