#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

#include "Server.h"
#include "ContextManager.h"

constexpr unsigned short SERVER_PORT = 53200;
using namespace boost::asio::ip;

int main()
{
	const auto ctxThreadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 100;
	const std::size_t rpcCtxThreadCount = ctxThreadCount / 5; // 20% of total threads for RPC
	const std::size_t workCtxThreadCount = ctxThreadCount - rpcCtxThreadCount; // Remaining threads for work context

	auto workThreadContext = std::make_shared<ContextManager>(workCtxThreadCount);
	auto rpcThreadContext = std::make_shared<ContextManager>(rpcCtxThreadCount);

	tcp::endpoint thisEndPoint(tcp::v4(), SERVER_PORT);
	tcp::acceptor acceptor(workThreadContext->GetContext(), thisEndPoint);
	
	auto server = std::make_shared<Server>(workThreadContext, rpcThreadContext, acceptor);

	server->Start();
	SPDLOG_INFO("{} Logic Server Started", __func__);

	std::cin.get();

	server->Stop();

	workThreadContext->Stop();
	rpcThreadContext->Stop();
	
	return 0;
}