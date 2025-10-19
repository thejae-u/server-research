#pragma once
#include <string_view>
#include <fstream>
#include <memory>
#include <thread>
#include <cpr/cpr.h>

#include <google/protobuf/util/json_util.h>
#include "NetworkData.pb.h"

class ContextManager;
class HttpStatus;

constexpr auto WEB_SERVER_IP = "localhost:18080";
constexpr std::string_view INTERNAL_FILE_NAME = "InternalId.json";

constexpr auto API_SAVE_GAME = "/api/internal/save-game";
constexpr auto API_GET_TOKEN = "/api/auth/internal-login";

class InternalConnector : public std::enable_shared_from_this<InternalConnector>
{
public:
	InternalConnector();
	bool GetAccessTokenFromInternal();

	void SaveGameDataToDB(/*Game Result DTO*/);

private:
	NetworkData::InternalData _loginData;
	NetworkData::AccessToken _accessToken;
};
