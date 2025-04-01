#pragma once
#include <iostream>
#include <memory>
#include <mysqlx/xdevapi.h>
#include <mutex>

#define USER_TABLE "users"

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
	RequestProcess(std::shared_ptr<mysqlx::Session> dbSessionPtr, std::shared_ptr<mysqlx::Schema> dbPtr);
	~RequestProcess();

	ELastErrorCode RetreiveUserID(std::vector<std::string> userName);
	ELastErrorCode Login(std::vector<std::string> loginData); // Return Logic Server Connection
	ELastErrorCode Register(std::vector<std::string> registerData);
	ELastErrorCode SaveServerLog(std::string log);
	ELastErrorCode SaveUserLog(std::string userName, std::string log);
	int GetUserID(std::string userName);

private:
	std::shared_ptr<mysqlx::Schema> _dbPtr;
	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::mutex _transactionMutex;

	mysqlx::Table GetTable(std::string tableName)
	{
		return _dbPtr->getTable(tableName, true);
	}
};
