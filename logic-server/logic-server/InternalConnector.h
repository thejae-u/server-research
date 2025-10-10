#pragma once
#include <string_view>
#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <thread>
#include <cpr/cpr.h>

#include "ContextManager.h"

constexpr unsigned short INTERNAL_WEB_SEVER_PORT = 18080;
constexpr std::string_view INTERNAL_WEB_SERVER_IP = "localhost"; // String view can't change
constexpr std::string_view INTERNAL_FILE_NAME = "InternalId.json";

using json = nlohmann::json;

class InternalConnector : std::enable_shared_from_this<InternalConnector>
{
public:
	InternalConnector(std::shared_ptr<ContextManager> ctxManagerPtr);

private:
	std::shared_ptr<ContextManager> _ctxManagerPtr;

	std::string _username;
	std::string _password;

	std::string _accessToken;

	void GetAccessTokenFromInternal();
};
