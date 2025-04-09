#include "RequestProcess.h"
#include "Server.h"
#include "db-system-utility.h"

RequestProcess::RequestProcess(const std::shared_ptr<mysqlx::Session>& dbSessionPtr, const std::shared_ptr<mysqlx::Schema>& dbPtr)
	: _dbPtr(dbPtr), _dbSessionPtr(dbSessionPtr)
{
}

RequestProcess::~RequestProcess()
{
}

ELastErrorCode RequestProcess::RetrieveUserId(const std::shared_ptr<std::vector<std::string>>& userName)
{
	assert(!userName->empty());
	std::lock_guard<std::mutex> lock(_transactionMutex);

	// select Users
	try
	{
		_dbSessionPtr->startTransaction();

		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", (*userName)[0]).execute();

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

ELastErrorCode RequestProcess::GetUserId(const std::shared_ptr<std::string>& userName, std::shared_ptr<int> uuid)
{
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		auto data = GetTable(USER_TABLE).select("uuid").where("user_name = :name").bind("name", *userName).execute();

		if (data.count() == 0)
		{
			_dbSessionPtr->rollback();
			return ELastErrorCode::USER_NOT_FOUND;
		}

		uuid = std::make_shared<int>(data.fetchOne());
		_dbSessionPtr->commit();

		return ELastErrorCode::SUCCESS;
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}
}

ELastErrorCode RequestProcess::Login(const std::vector<std::string>& loginData) 
{
	assert(loginData.size() == 2);
	
	try
	{
		auto data = GetTable(USER_TABLE)
			.select("uuid")
			.where("user_name = :name AND user_password = SHA2(:pass, 256)")
			.bind("name", loginData[0])
			.bind("pass", loginData[1])
			.execute();

		if (data.count() == 0)
		{
			return ELastErrorCode::INCORRECT;
		}
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::Register(const std::shared_ptr<std::vector<std::string>>& registerData)
{
	assert(registerData->size() == 2);
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		GetTable(USER_TABLE)
			.insert("user_name", "user_password")
			.values((*registerData)[0], (*registerData)[1])
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

ELastErrorCode RequestProcess::SaveServerLog(const std::shared_ptr<std::string>& log)
{
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		GetTable("server_log").insert("log_text").values(*log).execute(); // insert log to server_log table

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	std::cout << System_Util::GetNowTime() << " " << *log << "\n";
	return ELastErrorCode::SUCCESS;
}

ELastErrorCode RequestProcess::SaveUserLog(const std::shared_ptr<std::string>& userName, const std::shared_ptr<std::string>& log)
{
	std::lock_guard<std::mutex> lock(_transactionMutex);

	try
	{
		_dbSessionPtr->startTransaction();

		const auto uuid = PrGetUserId(userName); // check user exist

		GetTable("user_log").insert("uuid", "log_text").values(uuid, *log).execute(); // insert log to user_log table

		_dbSessionPtr->commit();
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		return ELastErrorCode::UNKNOWN_ERROR;
	}

	std::cout << System_Util::GetNowTime() << " " << *userName << " user log saved\n";

	return ELastErrorCode::SUCCESS;
}

// Private Function Area

int RequestProcess::PrGetUserId(const std::shared_ptr<std::string>& userName) const
{
	try
	{
		std::unique_lock<std::mutex> lock(_tableLock);
		auto table = _dbPtr->getTable(USER_TABLE, true);
		
		auto data = table.select("uuid").where("user_name = :name").bind("name", *userName).execute();
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
}
