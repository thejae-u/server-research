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
	io_context io;
	io_context::strand ioStrand(io);

	tcp::endpoint thisEndPoint(tcp::v4(), THIS_PORT);
	tcp::acceptor acceptor(io, thisEndPoint);
	
	std::vector<std::shared_ptr<std::thread>> ioThreads;
	auto server = std::make_shared<Server>(ioStrand, acceptor);
	const auto ioThreadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 100;

	boost::asio::post(ioStrand.wrap([server]() { server->AcceptClientAsync(); }));
	std::cout << "Logic Server Started\n";

	ioThreads.reserve(ioThreadCount);
	for (std::size_t i = 0; i < ioThreadCount; ++i)
	{
		ioThreads.emplace_back(std::make_shared<std::thread>([&io]() { io.run(); }));
	}

	for (auto& ioThread : ioThreads)
	{
		if (ioThread->joinable())
		{
			ioThread->join();
		}
	}

	return 0;
}