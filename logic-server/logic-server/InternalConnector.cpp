#include "InternalConnector.h"

InternalConnector::InternalConnector(std::shared_ptr<ContextManager> ctxManagerPtr) : _ctxManagerPtr(ctxManagerPtr)
{
	const std::string idField = "user";
	const std::string usernameField = "username";
	const std::string passwordField = "password";

	std::fstream f(INTERNAL_FILE_NAME.data());
	json config = json::parse(f);

	_username = config[idField][usernameField];
	_password = config[idField][passwordField];

	GetAccessTokenFromInternal();
}

void InternalConnector::GetAccessTokenFromInternal()
{
	auto self(shared_from_this());
}