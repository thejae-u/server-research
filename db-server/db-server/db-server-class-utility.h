#pragma once
#include <vector>
#include <sstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Server_Util
{
	std::vector<std::string> static SplitString(std::string data)
	{
		std::vector<std::string> splitData;
		std::string buffer;
		std::istringstream iss(data);

		while (std::getline(iss, buffer, ','))
		{
			splitData.push_back(buffer);
		}

		return splitData;
	}
}
