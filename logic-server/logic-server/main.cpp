#include "Server.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

const std::string DB_IP = "127.0.0.1"; // DB SERVER IP ( must be changed )
constexpr unsigned short DB_PORT = 53000; // DB SERVER PORT
constexpr unsigned short THIS_PORT = 53200; // LOGIC SERVER PORT ( THIS )

using namespace boost::asio::ip;

int main()
{
	io_context ctx;
	io_context::strand ctxStrand(ctx);

	tcp::endpoint thisEndPoint(tcp::v4(), THIS_PORT);
	tcp::acceptor acceptor(ctx, thisEndPoint);
	
	std::vector<std::shared_ptr<std::thread>> ctxThreads;
	auto server = std::make_shared<Server>(ctxStrand, acceptor);
	const auto ctxThreadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 100;

	boost::asio::post(ctxStrand.wrap([server]() { server->AcceptClientAsync(); }));
	std::cout << "Logic Server Started\n";

	ctxThreads.reserve(ctxThreadCount);
	for (std::size_t i = 0; i < ctxThreadCount; ++i)
	{
		ctxThreads.emplace_back(std::make_shared<std::thread>([&ctx]() { ctx.run(); }));
	}

	for (auto& ctxThread : ctxThreads)
	{
		if (ctxThread->joinable())
		{
			ctxThread->join();
		}
	}

	return 0;
}