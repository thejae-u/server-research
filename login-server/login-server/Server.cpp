#include "Server.h"
#include "Session.h"

#include <iostream>

void Server::Start()
{
    std::cout << "Login Server Started\n";
    boost::asio::post(_io, [this]() { AsyncAccept(); }); // Start Accepting Clients
}

void Server::Stop()
{
    _isRunning = false;

    if (!_sessionsPtr->empty()) // if session exist close all sessions
    {
        for (const auto& session : *_sessionsPtr)
        {
            session->GetSocket().close();
            _sessionsPtr->erase(session);
        }
    }
}

void Server::AddReq(const n_data& req)
{
    {
        std::unique_lock<std::mutex> lock(_reqQueueMutex);
        _reqQueueCondVar.wait(lock, [this]() { return _isRunning; }); // Wait for Request
        
        _reqQueue->push(req);
    }

    _reqQueueCondVar.notify_one();
}

void Server::AsyncAccept()
{
    auto self = shared_from_this();

    auto newSessionPtr = std::make_shared<Session>(_io, self, ++_sessionIdCounter);
    
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

void Server::ProcessReq()
{
    n_data req;

    {
        std::unique_lock<std::mutex> lock(_reqQueueMutex);
        _reqQueueCondVar.wait(lock, [this]() { return !_reqQueue->empty() || !_isRunning; });

        if (!_isRunning && _reqQueue->empty())
            return;

        req = _reqQueue->front();
        _reqQueue->pop();
    }

    switch (req.type)
    {
    case n_data_type::LOGIN:
        // Send to DB Server
        break;

    // Not Implemented Types
    case n_data_type::REGISTER:
    default:
        break;
    }

    // Process Request
    // ...

    // Notify all sessions to process request
    for (const auto& session : *_sessionsPtr)
    {
        session->ProcessReq();
    }
}
