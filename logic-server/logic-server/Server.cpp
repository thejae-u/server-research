#include "Server.h"

#include "Session.h"
#include "LockstepGroup.h"

Server::Server(const std::shared_ptr<ContextManager>& mainCtxManager, const std::shared_ptr<ContextManager>& rpcCtxManager, tcp::acceptor& acceptor)
    : _mainCtxManager(mainCtxManager), _rpcCtxManager(rpcCtxManager), _acceptor(acceptor),
    _udpSocket(std::make_shared<UdpSocket>(_rpcCtxManager->GetStrand().context(), udp::endpoint(udp::v4(), 0)))
{
    _groupManager = std::make_shared<GroupManager>(_mainCtxManager);
    _isRunning = false;
    _allocatedUdpPort = _udpSocket->local_endpoint().port();
    spdlog::info("allocated udp port: {}", _allocatedUdpPort);
}

void Server::Start()
{
    _isRunning = true;
    AcceptClientAsync();
    ReceiveUdpDataAsync();

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
    auto newSession = std::make_shared<Session>(_mainCtxManager, _rpcCtxManager);

    _acceptor.async_accept(newSession->GetSocket(), _mainCtxManager->GetStrand().wrap([self, newSession](const boost::system::error_code& ec) {
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
        boost::asio::post(self->_mainCtxManager->GetStrand().wrap([self, newSession]() { self->InitSessionNetwork(newSession); }));
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

void Server::ReceiveUdpDataAsync()
{
    auto self(shared_from_this());
    auto receiveBuffer = std::make_shared<std::vector<char>>(_maxPacketSize);
    auto senderEndPoint = std::make_shared<udp::endpoint>();
    _udpSocket->async_receive_from(boost::asio::buffer(*receiveBuffer), *senderEndPoint,
        _rpcCtxManager->GetStrand().wrap([self, receiveBuffer, senderEndPoint](const boost::system::error_code ec, const std::size_t bytesRead) {
            if (ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                {
                    spdlog::info("main server udp receive_from closed");
                    return;
                }

                spdlog::error("udp read error on {} : {}", senderEndPoint->address().to_string(), ec.message());
                boost::asio::post(self->_rpcCtxManager->GetStrand().wrap([self]() { self->ReceiveUdpDataAsync(); }));
                return;
            }

            if (bytesRead < sizeof(std::uint16_t))
            {
                spdlog::error("udp read bytes error on {} : {}", senderEndPoint->address().to_string(), ec.message());
                boost::asio::post(self->_rpcCtxManager->GetStrand().wrap([self]() { self->ReceiveUdpDataAsync(); }));
                return;
            }

            std::uint16_t payloadSize;
            std::memcpy(&payloadSize, receiveBuffer->data(), sizeof(std::uint16_t));
            payloadSize = ntohs(payloadSize);

            if (payloadSize == 0 || payloadSize + sizeof(std::uint16_t) > bytesRead)
            {
                spdlog::error("udp read nothing or over payload size from {}", senderEndPoint->address().to_string());
                boost::asio::post(self->_rpcCtxManager->GetStrand().wrap([self]() { self->ReceiveUdpDataAsync(); }));
                return;
            }

            RpcPacket receivedRpcPacket;
            if (!receivedRpcPacket.ParseFromArray(receiveBuffer->data() + sizeof(std::uint16_t), payloadSize))
            {
                spdlog::error("rpc packet parsing error from {}", senderEndPoint->address().to_string());
                boost::asio::post(self->_rpcCtxManager->GetStrand().wrap([self]() { self->ReceiveUdpDataAsync(); }));
                return;
            }

            // valid data collected to lockstep group
            spdlog::info("data read complete {} : {}", receivedRpcPacket.uid(), Utility::MethodToString(receivedRpcPacket.method()));
            self->_groupManager->CollectInput(std::make_shared<RpcPacket>(receivedRpcPacket));
            boost::asio::post(self->_rpcCtxManager->GetStrand().wrap([self]() { self->ReceiveUdpDataAsync(); }));
            }
        )
    );
}
