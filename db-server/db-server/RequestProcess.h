#pragma once
#include <iostream>
#include <memory>
#include <mysqlx/xdevapi.h>
#include <mutex>

#define USER_TABLE "users"

/*
	Pr태그가 붙은 함수는 트랜잭션 내에서 사용하는 함수로 외부에서 호출할 수 없음
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

	int PrGetUserId(const std::shared_ptr<std::string>& userName) const;

	mysqlx::Table GetTable(const std::string& tableName) const
	{
		return _dbPtr->getTable(tableName, true);
	}
};
