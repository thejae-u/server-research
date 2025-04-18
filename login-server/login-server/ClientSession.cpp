#include "ClientSession.h"
#include "Server.h"
#include "DBConnectSession.h"

ClientSession::ClientSession(io_context& io, const std::shared_ptr<Server>& serverPtr, const std::size_t sessionId)
    : _io(io), _serverPtr(serverPtr), _sessionId(sessionId)
{
    _socketPtr = std::make_shared<boost_socket>(io); // empty socket

    _netSize = 0;
    _dataSize = 0;
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
    auto self(shared_from_this());
    _dbConnectSessionPtr = std::make_shared<DBConnectSession>(_io, self, _serverPtr->GetEndPoint());
    _dbConnectSessionPtr->Start();
    
    // Start Receiving Request
    boost::asio::post(_io, [this] { ReceiveSize(); });
}

void ClientSession::Stop()
{
    // Remove from server
    const auto self(shared_from_this());
    //_serverPtr->RemoveSession(self);
    
    // stop session
    if (_socketPtr->is_open())
        _socketPtr->close();
}

void ClientSession::ReceiveSize()
{
    boost::asio::async_read(*_socketPtr, boost::asio::buffer(&_netSize, sizeof(_netSize)),
        [this](const boost_ec& ec, std::size_t)
        {
            if (ec)
            {
                if (ec == boost::asio::error::eof)
                {
                    std::cout << "Client Disconnected : " << _socketPtr->remote_endpoint().address() << "\n";
                    Stop();
                    return;
                }
                
                std::cout << "Error Receive Size: " << ec.message() << "\n";
                return;
            }
            
             _dataSize = ntohl(_netSize);
            if (_buffer.capacity() < _dataSize)
                _buffer.resize(_dataSize);
                            
            ReceiveData();
        });
}

void ClientSession::ReceiveData()
{
    // Receive Network Data
    auto self(shared_from_this());

    boost::asio::async_read(*_socketPtr, boost::asio::buffer(_buffer),
        [this, self](const boost_ec& ec, std::size_t)
        {
            if (ec)
            {
                std::cerr << "Error Receive Data: " << ec.message() << "\n";
                return;
            }
            
            const auto receiveData = std::string(_buffer.begin(), _buffer.end());
            n_data deserializedData;
            
            deserializedData.ParseFromString(receiveData);
            _dbConnectSessionPtr->ProcessRequest(deserializedData);
            
            _netSize = 0;
            _dataSize = 0;
        });
}

void ClientSession::ReplyReq(const n_data& reply) const
{
    // Reply to Client
    std::string serializedData;
    reply.SerializeToString(&serializedData);
    const auto dataSize = static_cast<uint32_t>(serializedData.size());
    uint32_t netDataSize = htonl(dataSize);

    // non-blocking send
    boost::asio::async_write(*_socketPtr, boost::asio::buffer(&netDataSize, sizeof(netDataSize)),
        [this, serializedData](const boost_ec& sizeEc, std::size_t)
        {
            if (sizeEc)
            {
                std::cerr << "Error Sending Size: " << sizeEc.message() << "\n";
                return;    
            }
            
            boost::asio::async_write(*_socketPtr, boost::asio::buffer(serializedData),
            [](const boost_ec& dataEc, std::size_t)
            {
                if (dataEc)
                {
                    std::cout << "Error Sending Data: " << dataEc.message() << "\n";
                }
            });
        });
}