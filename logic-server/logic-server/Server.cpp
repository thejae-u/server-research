#include "Server.h"

#include "Session.h"
#include "LockstepGroup.h"

Server::Server(const std::shared_ptr<ContextManager>& mainCtxManager, const std::shared_ptr<ContextManager>& rpcCtxManager, tcp::acceptor& acceptor)
    : _normalCtxManager(mainCtxManager), _rpcCtxManager(rpcCtxManager), _acceptor(acceptor),
    _normalPrivateStrand(_normalCtxManager->GetContext()), _rpcPrivateStrand(_rpcCtxManager->GetContext()),
    _udpSocket(std::make_shared<UdpSocket>(_rpcCtxManager->GetContext(), udp::endpoint(udp::v4(), 0)))
{
    _groupManager = std::make_shared<GroupManager>(_normalCtxManager);
    _isRunning = false;
    _allocatedUdpPort = _udpSocket->local_endpoint().port();
    spdlog::info("allocated udp port: {}", _allocatedUdpPort);
}

void Server::Start()
{
    _isRunning = true;
    AcceptClientAsync();
    //AsyncReceiveUdpData();
    //AsyncSendUdpData();

    spdlog::info("server start complete");
}

void Server::Stop()
{
    spdlog::info("server stopping...");

    _isRunning = false;
    _acceptor.close();
    _groupManager.reset();

    spdlog::info("server stopped.");
}

void Server::AcceptClientAsync()
{
    if (!_isRunning)
        return;

    auto self(shared_from_this());
    auto newSession = std::make_shared<Session>(_normalCtxManager, _rpcCtxManager);
    /*newSession->SetSendDataByUdpAction([self](std::shared_ptr<std::pair<udp::endpoint, std::string>> sendData) {
        self->EnqueueSendData(sendData);
        }
    );*/

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

void Server::InitSessionNetwork(const std::shared_ptr<Session>& newSession) const
{
    auto self(shared_from_this());
    newSession->AsyncExchangeUdpPortWork(_allocatedUdpPort, [self, newSession](bool success) {
        if (!success)
        {
            spdlog::error("new session failed to excahnge UDP port");
            newSession->Stop();
            return;
        }

        // User info and group info
        newSession->AsyncReceiveUserInfo([self, newSession](bool success) {
            if (!success)
            {
                spdlog::error("new session failed to exchange user info");
                newSession->Stop();
                return;
            }

            newSession->AysncReceiveGroupInfo([self, newSession](bool success, std::shared_ptr<GroupDto> groupInfo) {
                if (!success)
                {
                    spdlog::error("new session failed to exchange user info");
                    newSession->Stop();
                    return;
                }

                spdlog::info("group {} set session {}", groupInfo->groupid(), to_string(newSession->GetSessionUuid()));
                self->_groupManager->AddSession(groupInfo, newSession);
                }
            ); }
        ); }
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

            // valid data collected to lockstep group
            self->_groupManager->CollectInput(std::make_shared<RpcPacket>(receivedRpcPacket));
            boost::asio::post(self->_rpcPrivateStrand.wrap([self]() { self->AsyncReceiveUdpData(); }));
            }
        )
    );
}

void Server::EnqueueSendData(std::shared_ptr<std::pair<udp::endpoint, std::string>> sendDataPair)
{
    std::lock_guard<std::mutex> lock(_sendDataQueueMutex);
    _sendDataQueue.push(sendDataPair);
}

void Server::AsyncSendUdpData()
{
    auto self(shared_from_this());
    boost::asio::post(_rpcPrivateStrand.wrap([self]() {
        udp::endpoint ep;
        std::string sendData;

        {
            std::lock_guard<std::mutex> lock(self->_sendDataQueueMutex);
            if (self->_sendDataQueue.empty())
            {
                self->AsyncSendUdpData();
                return;
            }

            ep = self->_sendDataQueue.front()->first; // endpoint pop
            sendData = self->_sendDataQueue.front()->second; // send data pop
            self->_sendDataQueue.pop();
        }

        spdlog::info("send packet to client {}:{}", ep.address().to_string(), ep.port());
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
                    self->AsyncSendUdpData();
                    return;
                }

                spdlog::info("send udp packet complete to {}", ep.address().to_string());
                self->AsyncSendUdpData();
                }
            ));
        })
    );
}

