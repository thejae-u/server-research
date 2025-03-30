#pragma once
#include <vector>
#include <sstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Server_Util
{
	std::vector<std::string> SplitString(std::string data)
	{
		std::vector<std::string> splitedData;
		std::string buffer;
		std::istringstream iss(data);

		while (std::getline(iss, buffer, ','))
		{
			splitedData.push_back(buffer);
		}

		return splitedData;
	}
}
