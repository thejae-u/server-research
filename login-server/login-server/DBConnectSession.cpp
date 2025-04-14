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

    // send to DB Server
    std::string serializedData;
    req.SerializeToString(&serializedData);
    uint32_t dataSize = static_cast<uint32_t>(serializedData.size());
    uint32_t netDataSize = htonl(dataSize);
    boost::asio::async_write(*_socketPtr, boost::asio::buffer(&netDataSize, sizeof(netDataSize)),
        [this, self, serializedData](const boost_ec& sizeEc, std::size_t)
        {
            if (sizeEc)
            {
                std::cerr << "Error Sending Size: " << sizeEc.message() << "\n";
                return;
            }

            boost::asio::async_write(*_socketPtr, boost::asio::buffer(serializedData),
                [this, self](const boost_ec& dataEc, std::size_t)
                {
                    if (dataEc)
                    {
                        std::cerr << "Error Sending Data: " << dataEc.message() << "\n";
                        return;
                    }

                    std::cout << "DBConnectSession Send Data Success\n";
                });
        });
}
