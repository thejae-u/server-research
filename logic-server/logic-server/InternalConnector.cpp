#include "InternalConnector.h"

#include "ContextManager.h"
#include "HttpStatus.h"

InternalConnector::InternalConnector()
{
	std::fstream f(INTERNAL_FILE_NAME.data());

	if (!f.is_open())
	{
		spdlog::error("file open failed");
		return;
	}

	try
	{
		auto config = json::parse(f);
		loginData = config.get<InternalData>();
	}
	catch (const json::exception& e)
	{
		std::string msg = e.what();
		spdlog::error("json parsing error: " + msg);
	}

	GetAccessTokenFromInternal();
}

void InternalConnector::GetAccessTokenFromInternal()
{
	std::string postUrl = WEB_SERVER_IP;
	postUrl += "/api/auth/internal-login";

	// TODO : Throw 는 임시 조치 -> 실패 시 다시 시도 하거나 예외 케이스가 있으면 서버를 종료하도록
	// 서버 연결과 관련한 처리는 일정 횟수를 다시 시도하고 마지막에 종료 처리

	try
	{
		json bodyJson = loginData.internal; // LoginDto to_json() 호출

		spdlog::info("request access token");

		// Request to Web server
		auto r = cpr::Post(cpr::Url{ postUrl },
			cpr::Header{ {"Content-Type", "application/json"} },
			cpr::Body{ bodyJson.dump() });

		if (r.error)
		{
			throw std::runtime_error(r.error.message);
		}

		std::string statusCodeStr = "(code: " + std::to_string(r.status_code) + ")";
		auto result = GetStatusCodeCategory(r.status_code);
		json responseJson;

		switch (result)
		{
		case HttpStatusCodeCategory::Success:
		case HttpStatusCodeCategory::Redirection:
			// Access Token parsing
			spdlog::info("get access token success");
			responseJson = json::parse(r.text);
			_accessToken = responseJson.get<AccessToken>(); // AccessToken from_json() 호출
			spdlog::info("internal connector intialize complete");
			break;

		case HttpStatusCodeCategory::ClientError:
			throw std::runtime_error("invalid request" + statusCodeStr);
		case HttpStatusCodeCategory::ServerError:
			throw std::runtime_error("internal error " + statusCodeStr);
		default:
			throw std::runtime_error("unknown error " + statusCodeStr);
		}
	}
	catch (const json::exception& e)
	{
		std::string msg = e.what();
		spdlog::error("json error: " + msg);
	}
	catch (const std::exception& e)
	{
		std::string msg = e.what();
		spdlog::error("http error: " + msg);
	}
}