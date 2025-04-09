#include <thread>
#include <vector>

#include "Server.h"

int main()
{
	boost::asio::io_context io;
	boost::asio::ip::tcp::acceptor acceptor(io);

	const std::size_t totalThreadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 10; // get hardware concurrency * 20
	const std::size_t ioThreadCount = totalThreadCount / 2; // io thread count
	const std::size_t sessionControlCount = ioThreadCount / 2; // Connection Control sessions thread count

	const auto server = std::make_shared<Server>(io, acceptor, sessionControlCount);
	server->Start();

	auto ioThreads = std::make_shared<std::vector<std::shared_ptr<std::thread>>>();
	ioThreads->reserve(ioThreadCount);
	for (std::size_t i = 0; i < ioThreadCount; i++) // create io threads
	{
		ioThreads->emplace_back(std::make_shared<std::thread>([&io]() { io.run(); }));
	}

	for (const auto& ioThread : *ioThreads)
	{
		if (ioThread->joinable())
			ioThread->join();
	}
	
	return 0;
}