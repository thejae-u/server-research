#include "Session.h"

Session::Session(socket_sptr socket)
	: socket_(std::move(socket)), maxBufferSize_(MAX_BUF)
{
	std::cout << std::this_thread::get_id() << " Session Successfully Created\n";
	RecvData(); // Start to receive data
}

Session::~Session() // when the socket is closed, server will flush the session
{
	std::cout << std::this_thread::get_id() << " Session Successfully Closed\n";
}

void Session::RecvData()
{
	auto data = std::make_shared<std::string>();
	auto self = shared_from_this();

	socket_->async_read_some(boost::asio::buffer(*data, maxBufferSize_),
		[this, self, data](boost::system::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				// Do something
				std::cout << "Received Data: " << *data << std::endl;

				this->RecvData(); // Continue to receive data
			}
			else
			{
				// Exception Handling
				// Close the session
				std::cout << "Error: " << ec.message() << "\nSession closed\n";
				socket_->close();
			}
		});
}