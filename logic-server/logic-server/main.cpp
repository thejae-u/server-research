#include "Server.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

// const std::string DB_IP = "127.0.0.1"; // DB SERVER IP ( must be changed )
// constexpr unsigned short DB_PORT = 53000; // DB SERVER PORT
constexpr unsigned short THIS_PORT = 53200; // LOGIC SERVER PORT ( THIS )

using namespace boost::asio::ip;

int main()
{
	IoContext workCtx;
	IoContext rpcCtx;
	IoContext::strand ctxStrand(workCtx);
	IoContext::strand rpcStrand(rpcCtx);
	
	auto workGuard = boost::asio::make_work_guard(workCtx);
	auto rpcWorkGuard = boost::asio::make_work_guard(rpcCtx);

	tcp::endpoint thisEndPoint(tcp::v4(), THIS_PORT);
	tcp::acceptor acceptor(workCtx, thisEndPoint);
	
	std::vector<std::shared_ptr<std::thread>> workCtxThreads;
	std::vector<std::shared_ptr<std::thread>> rpcCtxThreads;
	const auto ctxThreadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 100;
	const std::size_t rpcCtxThreadCount = ctxThreadCount / 5; // 20% of total threads for RPC
	
	auto server = std::make_shared<Server>(ctxStrand, rpcStrand, acceptor);

	server->AcceptClientAsync();
	SPDLOG_INFO("{} Logic Server Started", __func__);

	workCtxThreads.reserve(ctxThreadCount - rpcCtxThreadCount);
	for (std::size_t i = 0; i < ctxThreadCount - rpcCtxThreadCount; ++i)
	{
		workCtxThreads.emplace_back(std::make_shared<std::thread>([&workCtx]() { workCtx.run(); }));
	}

	rpcCtxThreads.reserve(rpcCtxThreadCount);
	for (std::size_t i = 0; i < rpcCtxThreadCount; ++i)
	{
		rpcCtxThreads.emplace_back(std::make_shared<std::thread>([&rpcCtx]() { rpcCtx.run(); }));
	}

	std::cin.get();
	
	workGuard.reset();
	rpcWorkGuard.reset();

	for (auto& ctxThread : workCtxThreads)
	{
		if (ctxThread->joinable())
		{
			ctxThread->join();
		}
	}

	for (auto& rpcCtxThread : rpcCtxThreads)
	{
		if (rpcCtxThread->joinable())
		{
			rpcCtxThread->join();
		}
	}

	return 0;
}