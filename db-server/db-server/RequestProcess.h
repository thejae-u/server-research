#pragma once
#include <iostream>
#include <memory>
#include <mysqlx/xdevapi.h>
#include <mutex>
#include <chrono>

#include "NetworkData.pb.h"

#define USER_TABLE "users"

using time_point = std::chrono::system_clock::time_point;

enum class ELastErrorCode
{
	SUCCESS,
	USER_NOT_FOUND,
	USER_ALREADY_EXIST,
	INCORRECT,

	UNKNOWN_ERROR,
};

class RequestProcess
{
public:
	RequestProcess(const std::shared_ptr<mysqlx::Session>& dbSessionPtr, const std::shared_ptr<mysqlx::Schema>& dbPtr);
	~RequestProcess();

	ELastErrorCode RetrieveUserId(const std::shared_ptr<std::vector<std::string>>& userName);
	ELastErrorCode Login(const std::vector<std::string>& loginData); // Return Logic Server Connection
	ELastErrorCode Register(const std::shared_ptr<std::vector<std::string>>& registerData);
	ELastErrorCode SaveServerLog(const std::shared_ptr<std::string>& log);
	ELastErrorCode SaveUserLog(const std::shared_ptr<std::string>& userName, const std::shared_ptr<std::string>& log);
	ELastErrorCode GetUserId(const std::shared_ptr<std::string>& userName, std::shared_ptr<int> uuid);

private:
	std::shared_ptr<mysqlx::Schema> _dbPtr;
	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::mutex _transactionMutex;
	mutable std::mutex _tableLock;

	int PrGetUserId(const std::shared_ptr<std::string>& userName) const;

	mysqlx::Table GetTable(const std::string& tableName) const
	{
		std::unique_lock<std::mutex> lock(_tableLock);
		return _dbPtr->getTable(tableName, true);
	}
};
