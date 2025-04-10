#include "ClientSession.h"
#include "Server.h"
#include "DBConnectSession.h"

ClientSession::ClientSession(io_context& io, const std::shared_ptr<Server>& serverPtr, const std::size_t sessionId)
    : _io(io), _serverPtr(serverPtr), _sessionId(sessionId)
{
    _socketPtr = std::make_shared<boost_socket>(io); // empty socket
}

ClientSession::~ClientSession()
{
    if (_socketPtr->is_open())
    {
        _socketPtr->close();
    }
}

void ClientSession::Start()
{
    _dbConnectSessionPtr = std::make_shared<DBConnectSession>(_io, shared_from_this());
    
    // Start Receiving Request
    boost::asio::post(_io, [this] { ReceiveSize(); });
}

void ClientSession::ReceiveSize()
{
    _socketPtr->async_read_some(boost::asio::buffer(&_netSize, sizeof(_netSize)),
        [this](const boost_ec& ec, std::size_t)
        {
            if (!ec)
            {
                _dataSize = ntohl(_netSize);
                if (_buffer.capacity() < _dataSize)
                    _buffer.resize(_dataSize);
                
                ReceiveData();
            }
            else
            {
                if (ec == boost::asio::error::eof)
                {
                    std::cout << "Client Disconnected : " << _socketPtr->remote_endpoint().address() << "\n";
                    Stop();
                    return;
                }
                
                std::cout << "Error Receive Size: " << ec.message() << "\n";
            }
        });
}

void ClientSession::ReceiveData()
{
    // Receive Network Data
    auto self(shared_from_this());

    _socketPtr->async_read_some(boost::asio::buffer(_buffer),
        [this, self](const boost_ec& ec, std::size_t)
        {
            if (!ec)
            {
                std::string receiveData = std::string(_buffer.begin(), _buffer.end());
                n_data deserializedData;
                deserializedData.ParseFromString(receiveData);

                _dbConnectSessionPtr->ProcessRequest(deserializedData);

                _netSize = 0;
                _dataSize = 0;
            }
        });
}