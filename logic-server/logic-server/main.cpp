#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

#include "Server.h"
#include "ContextManager.h"
#include "InternalConnector.h"

constexpr bool NO_WEB_SERVER_MODE = true;
constexpr unsigned short SERVER_PORT = 53200;
using namespace boost::asio::ip;

int main()
{
    spdlog::info("initialize start");
    std::size_t coreCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 2;
    if (coreCount == 0)
        coreCount = 4; // least core count

    const std::size_t mainIoThreads = coreCount / 2;
    const std::size_t mainWorkerThreads = coreCount - mainIoThreads;

    const std::size_t rpcIoThreads = coreCount / 2;
    const std::size_t rpcWorkerThreads = coreCount - rpcIoThreads;

    auto workThreadContext = ContextManager::Create("main", mainIoThreads, mainWorkerThreads);
    auto rpcThreadContext = ContextManager::Create("rpc", rpcIoThreads, rpcWorkerThreads);

    if (!NO_WEB_SERVER_MODE)
    {
        auto internalConnector = std::make_shared<InternalConnector>();
        if (!internalConnector->GetAccessTokenFromInternal())
        {
            spdlog::error("initialize failed close server...");

            workThreadContext->Stop();
            rpcThreadContext->Stop();
            return -1;
        }
    }

    spdlog::info("initialize complete");

    tcp::endpoint thisEndPoint(tcp::v4(), SERVER_PORT);
    tcp::acceptor acceptor(workThreadContext->GetContext(), thisEndPoint);

    auto server = std::make_shared<Server>(workThreadContext, rpcThreadContext, acceptor);

    spdlog::info("start server...");
    server->Start();

    spdlog::warn("press any key to stop server");
    std::cin.get();

    server->Stop();

    workThreadContext->Stop();
    rpcThreadContext->Stop();

    return 0;
}