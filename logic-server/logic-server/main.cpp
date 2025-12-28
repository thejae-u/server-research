#include <asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

#include "Server.h"
#include "ContextManager.h"
#include "InternalConnector.h"
#include "Monitor.h"
#include "spdlog/sinks/stdout_color_sinks.h"

constexpr bool NO_WEB_SERVER_MODE = true;
constexpr unsigned short SERVER_PORT = 53200;
using namespace asio::ip;

int main()
{
    auto monitorSink = std::make_shared<MonitorSink_mt>();
    auto logger = std::make_shared<spdlog::logger>("monitor", monitorSink);
    spdlog::set_default_logger(logger);
    ConsoleMonitor::Get().Start();

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

    std::cin.clear();

    spdlog::warn("Server is running... Press ENTER to exit.");
    
    // Wait for monitor to signal exit (ENTER key pressed)
    while (ConsoleMonitor::Get().IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    ConsoleMonitor::Get().Stop();
    spdlog::set_default_logger(spdlog::stdout_color_mt("console"));

    server->Stop(true);

    workThreadContext->Stop();
    rpcThreadContext->Stop();

    return 0;
}