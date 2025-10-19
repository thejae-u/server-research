#include "InternalConnector.h"

#include "ContextManager.h"
#include "HttpStatus.h"

InternalConnector::InternalConnector()
{
	std::fstream f(INTERNAL_FILE_NAME.data());

	if (!f.is_open() || f.bad() || f.eof() || f.fail())
	{
		spdlog::error("file open failed");
		return;
	}

	try
	{
		std::stringstream buffer;
		buffer << f.rdbuf();
		f.close();

		auto status = google::protobuf::util::JsonStringToMessage(buffer.str(), &_loginData);
		if (status.message().size() > 0)
		{
			spdlog::error("protobuf deserialize error: {}", status.message());
			return;
		}
	}
	catch (std::exception& ex)
	{
		spdlog::error("{}", ex.what());
		return;
	}
}

bool InternalConnector::GetAccessTokenFromInternal()
{
	std::string postUrl = WEB_SERVER_IP;
	postUrl += API_GET_TOKEN;

	// TODO : 웹 서버가 문제가 있는 경우 일부 기능을 비활성화 해야함

	//json bodyJson = _loginData.internal; // LoginDto to_json() 호출
	const auto& loginInfo = _loginData.internal();
	std::string jsonString;
	auto status = google::protobuf::util::MessageToJsonString(loginInfo, &jsonString);

	if (status.message().size() > 0)
	{
		spdlog::error("error parsing login info to json string: {}", status.message());
		return false;
	}

	spdlog::info("request access token");

	int retryCount = 3;
	cpr::Response r;
	while (retryCount)
	{
		// Request to Web server
		r = cpr::Post(cpr::Url{ postUrl },
			cpr::Header{ {"Content-Type", "application/json"} },
			cpr::Body{ jsonString });

		// network failed retrying
		if (r.error)
		{
			if (r.error.code == cpr::ErrorCode::COULDNT_CONNECT)
			{
				spdlog::error("{}, retrying...{}", r.error.message, retryCount--);
				continue;
			}

			spdlog::error("{}, retrying...{}", r.error.message, retryCount--);
			continue;
		}

		// success
		break;
	}

	if (retryCount == 0)
	{
		spdlog::error("network failed request http");
		return false;
	}

	std::string statusCodeStr = "(code: " + std::to_string(r.status_code) + ")";
	auto result = GetStatusCodeCategory(r.status_code);

	switch (result)
	{
	case HttpStatusCodeCategory::Success:
	case HttpStatusCodeCategory::Redirection:
		// Access Token parsing
		spdlog::info("get access token success");

		status = google::protobuf::util::JsonStringToMessage(r.text, &_accessToken);
		if (status.message().size() > 0)
		{
			spdlog::error("parsing error occured: access token");
			return false;
		}

		spdlog::info("internal connector intialize complete");
		return true;

	case HttpStatusCodeCategory::ClientError:
		spdlog::error("internal request error {}", statusCodeStr);
		return false;
	case HttpStatusCodeCategory::ServerError:
		spdlog::error("internal response error {}", statusCodeStr);
		return false;
	default:
		spdlog::error("unknown error {}", statusCodeStr);
		return false;
	}
}

void InternalConnector::SaveGameDataToDB()
{
	std::string postUrl = WEB_SERVER_IP;
	postUrl = API_SAVE_GAME;
}