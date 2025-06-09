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

	auto context1 = std::make_shared<ContextManager>(workCtxThreadCount);
	auto context2 = std::make_shared<ContextManager>(rpcCtxThreadCount);

	tcp::endpoint thisEndPoint(tcp::v4(), SERVER_PORT);
	tcp::acceptor acceptor(context1->GetContext(), thisEndPoint);
	
	auto server = std::make_shared<Server>(context1->GetStrand(), context2->GetStrand(), acceptor);

	server->Start();
	SPDLOG_INFO("{} Logic Server Started", __func__);

	std::cin.get();

	server->Stop();

	context1->Stop();
	context2->Stop();
	
	return 0;
}