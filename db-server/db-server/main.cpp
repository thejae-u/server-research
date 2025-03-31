#include <iostream>

#include "Server.h"

int main()
{
	boost::asio::io_context io;
	std::string id;
	std::string password;

	std::cout << "DB User: ";
	std::cin >> id;
	std::cout << "DB Password: ";
	std::cin >> password;

	std::unique_ptr<Server> server = std::make_unique<Server>(io, id, password); // io, db user, db password init

	server->Start(); // connect to db

	if (server->IsInitValid()) // if connection is valid
	{
		std::cout << "Server is running\n";
	}
	else
	{
		std::cout << "Server failed to start\n";
		return -1;
	}

	std::size_t threadCount = std::thread::hardware_concurrency() * 2; // get hardware concurrency * 2
	std::vector<std::shared_ptr<std::thread>> ioThreads;

	for (std::size_t i = 0; i < threadCount; i++) // create io threads
	{
		ioThreads.emplace_back(std::make_shared<std::thread>([&io]() { io.run(); }));
	}
	
	for (auto& ioThread : ioThreads) // block until all threads are finished
	{
		if(ioThread->joinable())
			ioThread->join();
	}

	return 0;
}

