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

	std::thread ioThread([&io]() { io.run(); }); // start io thread

	// temp data for test 
	SNetworkData loginReq;
	loginReq.type = ENetworkType::LOGIN;
	loginReq.data = "alice,password1";
	loginReq.bufSize = loginReq.data.size();
	server->AddReq(loginReq);

	SNetworkData registerReq;
	registerReq.type = ENetworkType::REGISTER;
	registerReq.data = "jaeu,hellojaeu";
	registerReq.bufSize = registerReq.data.size();
	server->AddReq(registerReq);

	if (ioThread.joinable())
		ioThread.join();

	return 0;
}

