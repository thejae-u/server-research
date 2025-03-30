#pragma once
#include <iostream>
#include <memory>
#include <mysqlx/xdevapi.h>

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
	RequestProcess(std::shared_ptr<mysqlx::Schema> dbPtr);
	~RequestProcess();

	ELastErrorCode RetreiveUserID(std::vector<std::string> userName);
	ELastErrorCode Login(std::vector<std::string> loginData); // Return Logic Server Connection
	ELastErrorCode Register(std::vector<std::string> registerData);
	ELastErrorCode SaveServerLog(std::string log);

private:
	std::shared_ptr<mysqlx::Schema> _dbPtr;

	mysqlx::Table GetTable(std::string tableName)
	{
		return _dbPtr->getTable(tableName, true);
	}
};

