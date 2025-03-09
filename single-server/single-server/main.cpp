#include "Server.h"
#include <chrono>

int main()
{
	auto io = std::make_shared<io_context>();
	auto server = std::make_shared<Server>(*io, 8080);

	std::cout << "server start before\n";

	server->Start();

	std::this_thread::sleep_for(std::chrono::milliseconds(5000));

	std::cout << "stop server\n";

	server->Stop();

	return 0;
}