#pragma once

#include "Essential.h"

#define MAX_BUF 1024

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(socket_sptr socket);
	~Session();

	socket_sptr& GetSocket() { return socket_; }

private:
	socket_sptr socket_;
	std::size_t maxBufferSize_;

	void RecvData();
};

