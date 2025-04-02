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

	void static StartTime(std::chrono::time_point<std::chrono::system_clock>& start)
	{
		start = std::chrono::system_clock::now();
	}

	std::chrono::milliseconds static EndTime(std::chrono::time_point<std::chrono::system_clock>& start)
	{
		auto end = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	}
}
