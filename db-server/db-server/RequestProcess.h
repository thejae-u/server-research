#pragma once
#include <iostream>
#include <memory>
#include <mysqlx/xdevapi.h>
#include <mutex>

#define USER_TABLE "users"

#include "NetworkData.h"

/*
	Pr태	그가 붙은 함수는 트랜잭션 내에서 사용하는 함수로 외부에서 호출할 수 없음
	(Private Function)
*/

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
	ELastErrorCode GetUserID(std::string userName, int& uuid);

private:
	std::shared_ptr<mysqlx::Schema> _dbPtr;
	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::mutex _transactionMutex;

	int PrGetuserID(std::string userName);
	

	mysqlx::Table GetTable(std::string tableName)
	{
		return _dbPtr->getTable(tableName, true);
	}
};
