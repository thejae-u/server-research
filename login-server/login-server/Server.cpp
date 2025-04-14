#include "Server.h"
#include "ClientSession.h"

Server::Server(io_context& io, boost_acceptor& acceptor, const std::shared_ptr<boost_ep>& endPoint)
    : _io(io), _acceptor(acceptor), _dbEndPointPtr(endPoint), _isRunning(false)
{
    _sessionIdCounter = 0;
    _sessionsPtr = std::make_shared<std::set<std::shared_ptr<ClientSession>>>();
    _reqQueue = std::make_shared<std::queue<n_data>>();
}

Server::~Server()
{
    std::cout << "Server Closed\n";
}

void Server::Start()
{
    std::cout << "Server Started\n";
    boost::asio::post(_io, [this]() { AsyncAccept(); }); // Start Accepting Clients
}

void Server::Stop()
{
    _isRunning = false;
    
    if (_sessionsPtr->empty())
    {
        return;
    }
    
    for (auto it = _sessionsPtr->begin(); it != _sessionsPtr->end();)
    {
        (*it)->Stop();
        it = _sessionsPtr->erase(it);
    }
}

void Server::AsyncAccept()
{
    auto self(shared_from_this());
    auto newSessionPtr = std::make_shared<ClientSession>(_io, self, ++_sessionIdCounter);
    
    _acceptor.async_accept(newSessionPtr->GetSocket(), [this, newSessionPtr](const boost_ec& ec)
    {
        if (!ec)
        {
            std::cout << "Client Connected\n";
            newSessionPtr->Start();

            {
                std::unique_lock<std::mutex> lock(_sessionSetMutex);
                _sessionsPtr->insert(newSessionPtr);
            }
            
            std::cout << "New Session Created : " << newSessionPtr->GetSocket().remote_endpoint().address() << "\n";
        }
        else
        {
            std::cerr << "Accept Failed: " << ec.message() << "\n";
        }

        AsyncAccept(); // Accept next client
    });
}