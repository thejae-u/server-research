#pragma once
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace System_Util
{
	std::string GetCurrentTime()
	{
		auto now = std::chrono::system_clock::now();
		auto nowTimeT = std::chrono::system_clock::to_time_t(now);
		struct tm timeInfo;

		localtime_s(&timeInfo, &nowTimeT);

		std::stringstream ss;
		ss << std::put_time(&timeInfo, "[%Y-%m-%d %X]");

		return ss.str();
	}
}
