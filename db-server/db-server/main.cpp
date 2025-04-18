#pragma once
#include <iostream>

#include "Server.h"

const extern int PORT = 53000; // DB Server Port

int main()
{
	std::string id;
	std::string password;

	std::cout << "DB User: ";
	std::cin >> id;
	std::cout << "DB Password: ";
	std::cin >> password;

	io_context io;
	boost_acceptor acceptor(io, boost_ep(boost::asio::ip::tcp::v4(), PORT)); // acceptor init
	const std::size_t threadCount = static_cast<std::size_t>(std::thread::hardware_concurrency()) * 20; // get hardware concurrency * 20

	// other threads are spare threads
	
	auto server = std::make_shared<Server>(io, acceptor, threadCount, "localhost", id, password); // need to be changed ip
	server->Start(); // connect to db

	std::vector<std::shared_ptr<std::thread>> ioThreads;
	ioThreads.reserve(threadCount);

	for (std::size_t i = 0; i < threadCount; i++) // create io threads
	{
		ioThreads.emplace_back(std::make_shared<std::thread>([&io]() { io.run(); }));
	}
	
	for (auto& ioThread : ioThreads) // block until all threads are finished
	{
		if(ioThread->joinable())
			ioThread->join();
	}

	acceptor.close(); // close acceptor

	return 0;
}

