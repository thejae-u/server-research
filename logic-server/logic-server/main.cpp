#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

#include "Server.h"
#include "ContextManager.h"
#include "InternalConnector.h"

constexpr unsigned short SERVER_PORT = 53200;
using namespace boost::asio::ip;

int main()
{
    spdlog::info("initialize start");
    const auto ctxThreadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 100;
    const std::size_t rpcCtxThreadCount = ctxThreadCount / 5; // 20% of total threads for RPC
    const std::size_t workCtxThreadCount = ctxThreadCount - rpcCtxThreadCount; // Remaining threads for work context

    auto workThreadContext = std::make_shared<ContextManager>(workCtxThreadCount * 0.3f, workCtxThreadCount * 0.7f); // work thread for normal network callback
    auto rpcThreadContext = std::make_shared<ContextManager>(rpcCtxThreadCount * 0.3f, rpcCtxThreadCount * 0.7f); // rpc thread for udp network callback

    auto internalConnector = std::make_shared<InternalConnector>();
    if (!internalConnector->GetAccessTokenFromInternal())
    {
        spdlog::error("initialize failed close server...");

        workThreadContext->Stop();
        rpcThreadContext->Stop();
        return -1;
    }

    spdlog::info("initialize complete");

    tcp::endpoint thisEndPoint(tcp::v4(), SERVER_PORT);
    tcp::acceptor acceptor(workThreadContext->GetContext(), thisEndPoint);

    auto server = std::make_shared<Server>(workThreadContext, rpcThreadContext, acceptor);

    spdlog::info("start server...");
    server->Start();

    std::cin.get();

    server->Stop();

    workThreadContext->Stop();
    rpcThreadContext->Stop();

    return 0;
}