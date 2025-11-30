#include "Server.h"

#include "Session.h"
#include "LockstepGroup.h"
#include "Monitor.h"

Server::Server(const std::shared_ptr<ContextManager>& mainCtxManager, const std::shared_ptr<ContextManager>& rpcCtxManager, tcp::acceptor& acceptor)
    : _normalCtxManager(mainCtxManager), _rpcCtxManager(rpcCtxManager), _acceptor(acceptor),
    _normalPrivateStrand(_normalCtxManager->GetContext()), _rpcPrivateStrand(_rpcCtxManager->GetContext()),
    _udpSocket(std::make_shared<UdpSocket>(_rpcCtxManager->GetContext(), udp::endpoint(udp::v4(), 0)))
{
    _groupManager = std::make_shared<GroupManager>(_normalCtxManager);
    _isRunning = false;
    _isSending = false;
    _allocatedUdpPort = _udpSocket->local_endpoint().port();
    spdlog::info("allocated udp port: {}", _allocatedUdpPort);
}

Server::~Server()
{
    spdlog::info("server destroyed");
}

void Server::Start()
{
    _isRunning = true;
    AcceptClientAsync();
    AsyncReceiveUdpData();

    spdlog::info("server start complete");
}

void Server::Stop(bool forceStop)
{
    spdlog::info("server stopping...");

    _isRunning = false;
    _udpSocket->close();

    _acceptor.close();

    _groupManager->Stop();
    _groupManager.reset();

    spdlog::info("server stopped.");
}

void Server::AcceptClientAsync()
{
    if (!_isRunning)
        return;

    std::weak_ptr<Server> weakSelf(shared_from_this());
    auto newSession = std::make_shared<Session>(_normalCtxManager, _rpcCtxManager);
    newSession->SetSendDataByUdpAction([weakSelf](std::shared_ptr<std::pair<udp::endpoint, std::string>> sendData) {
        if(auto self = weakSelf.lock())
            self->EnqueueSendData(sendData);
        }
    );

    auto self(shared_from_this());
    _acceptor.async_accept(newSession->GetSocket(), _normalPrivateStrand.wrap([self, newSession](const boost::system::error_code& ec) {
        if (ec)
        {
            if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::connection_aborted)
            {
                spdlog::info("accept operation aborted");
                return;
            }

            spdlog::error("accept failed : {}", ec.message());
            self->AcceptClientAsync();
            return;
        }

        self->AcceptClientAsync();
        boost::asio::post(self->_normalPrivateStrand.wrap([self, newSession]() { self->InitSessionNetwork(newSession); }));
        })
    );
}

void Server::InitSessionNetwork(const std::shared_ptr<Session>& newSession)
{
    auto self(shared_from_this());
    newSession->AsyncExchangeUdpPortWork(_allocatedUdpPort, [self, newSession](bool success) {
        if (!success)
        {
            spdlog::error("new session failed to excahnge UDP port");
            newSession->Stop(false);
            return;
        }

        // User info and group info
        newSession->AsyncReceiveUserInfo([self, newSession](bool success) {
            if (!success)
            {
                spdlog::error("new session failed to exchange user info");
                newSession->Stop(false);
                return;
            }

            newSession->AsyncReceiveGroupInfo([self, newSession](bool success, std::shared_ptr<GroupDto> groupInfo) {
                if (!success)
                {
                    spdlog::error("new session failed to exchange user info");
                    newSession->Stop(false);
                    return;
                }

                spdlog::info("group {} set session {}", groupInfo->groupid(), to_string(newSession->GetSessionUuid()));
                self->_groupManager->AddSession(groupInfo, newSession);
                self->AddSession(newSession);

                std::weak_ptr<Server> weakSelf(self);
                newSession->SetStopCallbackByServer([weakSelf](const std::shared_ptr<Session>& session) {
                    if (auto self = weakSelf.lock())
                        self->RemoveSession(session->GetSessionUuid());
                    });
                }); 
            }); 
        }
    );
}

void Server::AsyncReceiveUdpData()
{
    auto self(shared_from_this());
    auto receiveBuffer = std::make_shared<std::vector<char>>(_maxPacketSize);
    auto senderEndPoint = std::make_shared<udp::endpoint>();
    _udpSocket->async_receive_from(boost::asio::buffer(*receiveBuffer), *senderEndPoint,
        _rpcPrivateStrand.wrap([self, receiveBuffer, senderEndPoint](const boost::system::error_code ec, const std::size_t bytesRead) {
            if (ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                {
                    spdlog::info("main server udp receive_from closed");
                    return;
                }

                spdlog::error("udp read error on {} : {}", senderEndPoint->address().to_string(), ec.message());
                boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
                return;
            }

            if (bytesRead < sizeof(std::uint16_t))
            {
                spdlog::error("udp read bytes error on {} : {}", senderEndPoint->address().to_string(), ec.message());
                boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
                return;
            }

            ConsoleMonitor::Get().IncrementUdpPacket();

            std::uint16_t payloadSize;
            std::memcpy(&payloadSize, receiveBuffer->data(), sizeof(std::uint16_t));
            payloadSize = ntohs(payloadSize);

            if (payloadSize == 0 || payloadSize + sizeof(std::uint16_t) > bytesRead)
            {
                spdlog::error("udp read nothing or over payload size from {}", senderEndPoint->address().to_string());
                boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
                return;
            }

            RpcPacket receivedRpcPacket;
            if (!receivedRpcPacket.ParseFromArray(receiveBuffer->data() + sizeof(std::uint16_t), payloadSize))
            {
                spdlog::error("rpc packet parsing error from {}", senderEndPoint->address().to_string());
                boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
                return;
            }

            // valid data collected to session and lockstep group
            auto id = self->_toUuid(receivedRpcPacket.uid());

            std::lock_guard<std::mutex> sessionsLock(self->_sessionsMutex);
            auto sessionIt = self->_sessions.find(id);
            if (sessionIt == self->_sessions.end())
            {
                spdlog::error("invalid session id requested (id: {})", receivedRpcPacket.uid());
                boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
                return;
            }

            sessionIt->second->CollectInput(std::make_shared<RpcPacket>(receivedRpcPacket));
            boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
            }
        )
    );
}

void Server::EnqueueSendData(std::shared_ptr<std::pair<udp::endpoint, std::string>> sendDataPair)
{
    std::lock_guard<std::mutex> lock(_sendDataQueueMutex);
    _sendDataQueue.push(sendDataPair);
    if (!_isSending)
    {
        _isSending = true;
        boost::asio::post(_rpcPrivateStrand.wrap([this]() { AsyncSendUdpData(); }));
    }
}

void Server::AsyncSendUdpData()
{
    auto self(shared_from_this());
    boost::asio::post(_rpcPrivateStrand.wrap([self]() {
        std::queue<std::shared_ptr<std::pair<udp::endpoint, std::string>>> localQueue;

        {
            std::lock_guard<std::mutex> lock(self->_sendDataQueueMutex);
            if (self->_sendDataQueue.empty())
            {
                self->_isSending = false;
                return;
            }

            self->_sendDataQueue.swap(localQueue);
        }

        while (!localQueue.empty())
        {
            auto sendDataPair = localQueue.front();
            localQueue.pop();

            // Lock is released here as the queue has been modified.
            // Prepare and send data outside the lock scope.
            udp::endpoint ep = sendDataPair->first;
            std::string sendData = sendDataPair->second;

            const std::uint16_t payloadSize = static_cast<std::uint16_t>(sendData.size());
            const std::uint16_t payloadNetSize = htons(payloadSize);

            const auto payload = std::make_shared<std::string>();
            payload->append(reinterpret_cast<const char*>(&payloadNetSize), sizeof(payloadNetSize));
            payload->append(sendData);

            self->_udpSocket->async_send_to(boost::asio::buffer(*payload), ep,
                self->_rpcPrivateStrand.wrap([self, payload, ep](boost::system::error_code ec, std::size_t) {
                    if (ec)
                    {
                        spdlog::error("udp send error: {}", ec.message());
                    }
                })
            );
        }

        std::lock_guard<std::mutex> lock(self->_sendDataQueueMutex);
        if (!self->_sendDataQueue.empty())
        {
            boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncSendUdpData(); }));
            return;
        }

        self->_isSending = false;
    }));
}

void Server::AddSession(std::shared_ptr<Session> newSession)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    _sessions[newSession->GetSessionUuid()] = newSession;
    ConsoleMonitor::Get().UpdateClientCount((int)_sessions.size());
}

void Server::RemoveSession(uuid sessionId)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    _sessions.erase(sessionId);
    ConsoleMonitor::Get().UpdateClientCount((int)_sessions.size());
	spdlog::info("Session {} removed from server session map", to_string(sessionId));
}

