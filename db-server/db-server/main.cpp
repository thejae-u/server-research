#pragma once
#include <iostream>

#include "Server.h"

int main()
{
	std::string id;
	std::string password;

	std::cout << "DB User: ";
	std::cin >> id;
	std::cout << "DB Password: ";
	std::cin >> password;

	io_context io;
	boost_acceptor acceptor(io, boost_ep(boost::asio::ip::tcp::v4(), 55000)); // acceptor init

	std::shared_ptr<Server> server = std::make_shared<Server>(io, acceptor, id, password); // io, db user, db password init

	server->Start(); // connect to db

	std::size_t threadCount = static_cast<size_t>(std::thread::hardware_concurrency()) * 2; // get hardware concurrency * 2
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

	acceptor.close(); // close acceptor

	return 0;
}

