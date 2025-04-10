#include <thread>
#include <vector>
#include "Server.h"

const extern int PORT = 53000; // DB SERVER PORT
const extern int THIS_PORT = 53100; // LOGIN SERVER PORT ( THIS )
const extern std::string DB_IP = "localhost"; // DB SERVER IP ( must be changed )

int main()
{
	boost::asio::io_context io;
	boost::asio::ip::tcp::acceptor acceptor(io);

	Server server(io, acceptor, "localhost", THIS_PORT, PORT); // need to be changed ip
	
	
	return 0;
}