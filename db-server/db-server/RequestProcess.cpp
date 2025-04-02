#include "RequestProcess.h"
#include "Server.h"
#include "db-system-utility.h"

/*
	DB Transaction 단위로 처리
	트랜잭션 내에서 다른 함수를 호출하지 않도록 구현하여야 함
	만약 반복적으로 사용하는 함수 (유저 아이디 조회 등)이 있다면 트랜잭션을 다시 시작 하지 않도록 구현하여야 함
*/

RequestProcess::RequestProcess(std::shared_ptr<mysqlx::Session> dbSessionPtr, std::shared_ptr<mysqlx::Schema> dbPtr)
	: _dbPtr(dbPtr), _dbSessionPtr(dbSessionPtr)
{
}

RequestProcess::~RequestProcess()
{
}

ELastErrorCode RequestProcess::RetreiveUserID(std::vector<std::string> userName)
{
	assert(userName.size() != 0);
	std::lock_guard<std::mutex> lock(_transactionMutex);

	// select Users
	try
	{
		_dbSessionPtr->startTransaction();

		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", userName[0]).execute();

		if (data.count() == 0)
		{
			return ELastErrorCode::USER_NOT_FOUND;
		}

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::USER_ALREADY_EXIST;
}

ELastErrorCode RequestProcess::GetUserID(std::string userName, int& uuid)
{
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", userName).execute();

		if (data.count() == 0)
		{
			_dbSessionPtr->rollback();
			return ELastErrorCode::USER_NOT_FOUND;
		}

		uuid = data.fetchOne();
		_dbSessionPtr->commit();

		return ELastErrorCode::SUCCESS;
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::UNKNOWN_ERROR;
}

ELastErrorCode RequestProcess::Login(std::vector<std::string> loginData)
{
	assert(loginData.size() == 2);
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		auto data = GetTable(USER_TABLE)
			.select("uuid")
			.where("user_name = :name AND user_password = SHA2(:pass, 256)")
			.bind("name", loginData[0])
			.bind("pass", loginData[1])
			.execute();

		if (data.count() == 0)
		{
			_dbSessionPtr->rollback();
			return ELastErrorCode::INCORRECT;
		}

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::Register(std::vector<std::string> registerData)
{
	assert(registerData.size() == 2);
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		GetTable(USER_TABLE)
			.insert("user_name", "user_password")
			.values(registerData[0], registerData[1])
			.execute();

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::SaveServerLog(std::string log)
{
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		GetTable("server_log").insert("log_text").values(log).execute(); // insert log to server_log table

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	std::cout << System_Util::GetNowTime() << " " << log << "\n";
	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::SaveUserLog(std::string userName, std::string log)
{
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		auto uuid = PrGetuserID(userName); // check user exist

		GetTable("user_log").insert("uuid", "log_text").values(uuid, log).execute(); // insert log to user_log table

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	std::cout << System_Util::GetNowTime() << " " << userName << " user log saved\n";

	return ELastErrorCode::SUCCESS;
}

// Private Function Area

int RequestProcess::PrGetuserID(std::string userName)
{
	try
	{
		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", userName).execute();
		if (data.count() == 0)
		{
			return -1;
		}
		auto row = data.fetchOne();
		return row[0];
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return -1;
	}
	return -1;
}
