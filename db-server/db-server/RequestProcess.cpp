#include "RequestProcess.h"
#include "db-system-utility.h"

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
	_transactionMutex.lock();

	// select Users
	try
	{
		_dbSessionPtr->startTransaction();

		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", userName[0]).execute();

		if (data.count() == 0)
		{
			_transactionMutex.unlock();
			return ELastErrorCode::USER_NOT_FOUND;
		}

		_dbSessionPtr->commit();
		_transactionMutex.unlock();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		_transactionMutex.unlock();
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	_transactionMutex.unlock();
	return ELastErrorCode::USER_ALREADY_EXIST;
}

int RequestProcess::GetUserID(std::string userName)
{
	_transactionMutex.lock();

	try
	{
		_dbSessionPtr->startTransaction();
		std::cout << "GetUserID start transaction\n";

		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", userName).execute();
		std::cout << "Get Table\n";

		if (data.count() == 0)
		{
			_dbSessionPtr->rollback();
			_transactionMutex.unlock();
			return -1;
		}

		auto row = data.fetchOne();
		_dbSessionPtr->commit();
		_transactionMutex.unlock();

		return row[0];
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return -1;
	}

	_transactionMutex.unlock();
	return -1;
}

ELastErrorCode RequestProcess::Login(std::vector<std::string> loginData)
{
	assert(loginData.size() == 2);
	_transactionMutex.lock();

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
			_transactionMutex.unlock();
			return ELastErrorCode::INCORRECT;
		}

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		_transactionMutex.unlock();
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	_transactionMutex.unlock();
	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::Register(std::vector<std::string> registerData)
{
	assert(registerData.size() == 2);

	try
	{
		_transactionMutex.lock();
		_dbSessionPtr->startTransaction();

		GetTable(USER_TABLE)
			.insert("user_name", "user_password")
			.values(registerData[0], registerData[1])
			.execute();

		_dbSessionPtr->commit();
		_transactionMutex.unlock();
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
	try
	{
		_transactionMutex.lock();
		_dbSessionPtr->startTransaction();

		GetTable("server_log").insert("log_text").values(log).execute(); // insert log to server_log table

		_dbSessionPtr->commit();
		_transactionMutex.unlock();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	std::cout << System_Util::GetCurrentTime() << " " << log << "\n";
	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::SaveUserLog(std::string userName, std::string log)
{
	try
	{
		_transactionMutex.lock();
		_dbSessionPtr->startTransaction();

		GetTable("user_log").insert("uuid", "log_text").values(GetUserID(userName), log).execute(); // insert log to user_log table

		_dbSessionPtr->commit();
		_transactionMutex.unlock();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	std::cout << System_Util::GetCurrentTime() << " " << userName << " user log saved\n";

	return ELastErrorCode::SUCCESS;
}
