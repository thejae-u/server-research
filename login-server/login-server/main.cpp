#include <thread>
#include <vector>
#include "Server.h"

using namespace boost::asio::ip;

const extern unsigned short DB_PORT = 53000; // DB SERVER PORT
const extern unsigned short THIS_PORT = 53100; // LOGIN SERVER PORT ( THIS )
const extern std::string DB_IP = "127.0.0.1"; // DB SERVER IP ( must be changed )

int main()
{
	// Asynchronous IO Context
	boost::asio::io_context io;

	// Hardware Concurrency count
	std::size_t threadCount = static_cast<std::size_t>(std::thread::hardware_concurrency() * 2);

	// Login Server TCP endpoint 
	auto thisEndPoint = tcp::endpoint(tcp::v4(), THIS_PORT);
	tcp::acceptor acceptor(io, thisEndPoint);

	// DB Server TCP endpoint
	auto dbEndPoint = std::make_shared<tcp::endpoint>(make_address(DB_IP), DB_PORT);

	// Start Login Server
	auto server = std::make_shared<Server>(io, acceptor, dbEndPoint);
	server->Start();

	// IO Context Threads
	std::vector<std::shared_ptr<std::thread>> ioThreads;
	ioThreads.reserve(threadCount);

	for (std::size_t i = 0; i < threadCount; ++i)
	{
		ioThreads.emplace_back(std::make_shared<std::thread>([&io]() { io.run(); }));
	}

	// Wait for IO Threads to finish until the server is stopped
	for (auto& ioThread : ioThreads)
	{
		if (ioThread->joinable())
			ioThread->join();
	}
	
	return 0;
}