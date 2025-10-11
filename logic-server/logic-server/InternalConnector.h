#pragma once
#include <string_view>
#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <thread>
#include <cpr/cpr.h>

class ContextManager;
class HttpStatus;

constexpr auto WEB_SERVER_IP = "localhost:18080";
constexpr std::string_view INTERNAL_FILE_NAME = "InternalId.json";

using json = nlohmann::json;

struct LoginDto
{
	std::string username;
	std::string password;
};

struct InternalData
{
	LoginDto internal;
};

struct AccessToken
{
	std::string accessToken;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LoginDto, username, password);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InternalData, internal);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AccessToken, accessToken);

class InternalConnector : public std::enable_shared_from_this<InternalConnector>
{
public:
	InternalConnector();

private:
	InternalData loginData;
	AccessToken _accessToken;

	void GetAccessTokenFromInternal();
};
