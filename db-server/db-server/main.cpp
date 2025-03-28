#include <iostream>
#include "Server.h"

int main() 
{
	boost::asio::io_context io;

	std::unique_ptr<Server> server = std::make_unique<Server>(io);

	if (!server->IsInitValid())
	{
		std::cerr << "Server initialization failed.\n";
		return -1;
	}

	server->Start();

	server->ProcessReq({ ENetworkType::OPTION_ONE, 0, nullptr });

	return 0;
}

