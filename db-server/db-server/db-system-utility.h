#pragma once
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace System_Util
{
	std::string static GetNowTime()
	{
		auto now = std::chrono::system_clock::now();
		auto nowTimeT = std::chrono::system_clock::to_time_t(now);
		struct tm timeInfo;

		localtime_s(&timeInfo, &nowTimeT);

		std::stringstream ss;
		ss << std::put_time(&timeInfo, "[%Y-%m-%d %X]");

		return ss.str();
	}

	std::chrono::system_clock::time_point static StartTime()
	{
		std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
		return start;
	}

	std::chrono::milliseconds static EndTime(const std::chrono::time_point<std::chrono::system_clock>& start)
	{
		const auto end = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	}
}
